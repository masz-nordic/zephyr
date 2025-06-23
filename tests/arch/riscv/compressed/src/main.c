/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>
#include <zephyr/arch/riscv/exception.h>

#if CONFIG_RISCV_ISA_EXT_ZCA
ZTEST(riscv_compressed, test_zca_load_store)
{
	uint32_t a = 5;
	uint32_t b = 0;

	/* c.swsp and c.lwsp */
	__asm__ volatile("addi sp, sp, -0x4;"
			 "c.swsp %1, 0x0(sp);"
			 "c.lwsp %0, 0x0(sp);"
			 "addi sp, sp, 0x4;"
			: "=&r"(b)
			: "r"(a)
			: "memory");

	zexpect_equal(b, 5, "c.swsp and c.lwsp");

	b = 0;
	a = 7;
	uint32_t * c = &a;

	/* c.lw */
	__asm__ volatile("c.lw %0, 0x0(%1);"
			: "=&r"(b)
			: "r"(c)
			: "memory");

	zexpect_equal(b, 7, "c.lw");

	b = 3;
	/* c.sw */
	__asm__ volatile("c.sw %0, 0x0(%1);"
			:
			: "r"(b), "r"(c)
			: "memory");

	zexpect_equal(a, 3, "c.sw");
}

ZTEST(riscv_compressed, test_zca_control)
{
	volatile bool skipped = true;
	/* c.j */
	__asm__ goto("c.j %l[cj];"
		    :
		    :
		    :
		    : cj);

	skipped = false;
cj:
	zexpect_equal(skipped, true, "c.j");

#if CONFIG_RISCV_ISA_RV32E || CONFIG_RISCV_ISA_RV32I
	/* c.jal */
	__asm__ volatile goto("c.jal %l[cjal];"
			     :
			     :
			     : "ra"
			     : cjal);
	// TODO test

	skipped = false;
cjal:
	zexpect_equal(skipped, true, "c.jal");
#endif

	/* c.jr */
	__asm__ volatile goto("la s0, %l[cjr];"
			      "c.jr s0;"
			     :
			     :
			     : "s0"
			     : cjr);

	skipped = false;
cjr:
	zexpect_equal(skipped, true, "c.jr");

	/* c.jalr */
	__asm__ volatile goto("la s0, %l[cjalr];"
			      "c.jalr s0;"
			     :
			     :
			     : "s0", "ra"
			     : cjalr);

	skipped = false;
cjalr:
	zexpect_equal(skipped, true, "c.jalr");

	/* c.bnez */
	__asm__ volatile goto("c.bnez %0, %l[bnez];"
			     :
			     : "r"(skipped)
			     :
			     : bnez);

	skipped = false;
bnez:
	zexpect_equal(skipped, true, "c.bnez");

	/* c.beqz */
	__asm__ volatile goto("c.beqz %0, %l[beqz];"
			     :
			     : "r"(skipped)
			     :
			     : beqz);

	skipped = false;
beqz:
	zexpect_equal(skipped, false, "c.beqz");
}

static volatile bool ebreak_executed;

void ztest_post_fatal_error_hook(unsigned int reason, const struct arch_esf *esf)
{
	unsigned long mcause;

	__asm__ volatile("csrr %0, mcause;"
			: "=r" (mcause));
	zexpect_equal(mcause, 3, "c.ebreak");

	zexpect_equal(reason, K_ERR_CPU_EXCEPTION, "c.ebreak");

	ebreak_executed = true;
}

ZTEST(riscv_compressed, test_zca_integer)
{
	unsigned long a = 0;
	unsigned long b = 0;
	unsigned long * c = &b;

	/* c.li */
	__asm__ volatile("c.li %0, 0xF;"
			: "=r"(a)
			:);

	zexpect_equal(a, 0xF, "c.li");

	/* c.lui */
	__asm__ volatile("c.lui %0, 0xF;"
			: "=r"(a)
			:);

	zexpect_equal(a, 0xFUL << 12, "c.lui");

	a = 2;
	/* c.addi */
	__asm__ volatile("c.addi %0, 1;"
			: "+r"(a)
			: "r"(a));

	zexpect_equal(a, 3, "c.addi");

	__asm__ volatile("sw sp, 0x0(%0);"
			:
			: "r"(c)
			: "memory");

	a = b;

	/* c.addi16sp */
	__asm__ volatile("c.addi16sp sp, -16;"
			 "sw sp, 0x0(%0);"
			 "c.addi16sp sp, 16;"
			 :
			 : "r"(c)
			 : "memory");

	zexpect_equal(a - 16, b, "c.addi16sp");

#if 0
	/* TODO how to force !a6 */
	/* c.addi4spn */
	__asm__ volatile("c.addi4spn %0, 4;"
			 : "=r"(b)
			 :
			 : "memory");

	zexpect_equal(a - 4, b, "c.addi4spn");
#endif

	a = 0xF;

	/* c.slli */
	__asm__ volatile("c.slli %0, 1;"
			: "+r"(a)
			: "r"(a));

	zexpect_equal(a, 0xF << 1, "c.slli");

	a = ULONG_MAX;

	/* c.srli */
	__asm__ volatile("c.srli %0, 1;"
			: "+r"(a)
			: "r"(a));

	zexpect_equal(a, ULONG_MAX >> 1UL, "c.srli");

	a = ULONG_MAX;

	/* c.srai */
	__asm__ volatile("c.srai %0, 1;"
			: "+r"(a)
			: "r"(a));

	zexpect_equal(a, ULONG_MAX, "c.srai");

	/* c.andi */
	__asm__ volatile("c.andi %0, 0;"
			: "+r"(a)
			: "r"(a));

	zexpect_equal(a, 0, "c.andi");

	b = 5;
	/* c.mv */
	__asm__ volatile("c.mv %0, %1;"
			: "=r"(a)
			: "r"(b));

	zexpect_equal(a, 5, "c.mv");

	/* c.add */
	__asm__ volatile("c.add %0, %1;"
			: "+r"(a)
			: "r"(b));

	zexpect_equal(a, 10, "c.add");

	a = 0xFF00;
	b = 0x0FF0;
	/* c.and */
	__asm__ volatile("c.and %0, %1;"
			: "+r"(a)
			: "r"(b));

	zexpect_equal(a, 0xF00, "c.and");

	/* c.or */
	__asm__ volatile("c.or %0, %1;"
			: "+r"(a)
			: "r"(b));

	zexpect_equal(a, 0xFF0, "c.or");

	/* c.xor */
	__asm__ volatile("c.xor %0, %1;"
			: "+r"(a)
			: "r"(b));

	zexpect_equal(a, 0, "c.xor");

	a = 0xFF0;
	/* c.sub */
	__asm__ volatile("c.sub %0, %1;"
			: "+r"(a)
			: "r"(b));

	zexpect_equal(a, 0, "c.sub");

	/* c.nop */
	__asm__ volatile("c.nop");

	ztest_set_fault_valid(true);
	/* c.ebreak */
	__asm__ volatile("c.ebreak");
	zexpect_true(ebreak_executed);
	ztest_set_fault_valid(false);
}

#if CONFIG_RISCV_ISA_RV64I
ZTEST(riscv_compressed, test_zca_64i)
{

	uint64_t i = UINT64_MAX;
	uint64_t j = 0;

	/* c.sdsp and c.ldsp */
	__asm__ volatile("addi sp, sp, -0x8;"
			 "c.sdsp %1, 0x0(sp);"
			 "c.ldsp %0, 0x0(sp);"
			 "addi sp, sp, 0x8;"
			: "=&r"(j)
			: "r"(i)
			: "memory");

	zexpect_equal(j, UINT64_MAX, "c.sdsp and c.ldsp");

	j = 0;
	i = UINT64_MAX - 5;
	uint64_t * k = &i;

	/* c.ld */
	__asm__ volatile("c.ld %0, 0x0(%1);"
			: "=&r"(j)
			: "r"(k)
			: "memory");

	zexpect_equal(j, UINT64_MAX - 5, "c.ld");

	j = UINT64_MAX - 3;

	/* c.sd */
	__asm__ volatile("c.sd %0, 0x0(%1);"
			:
			: "r"(j), "r"(k)
			: "memory");

	zexpect_equal(i, UINT64_MAX - 3, "c.sd");

	/* c.addiw */
	/* c.addw */
	/* c.subw */
}
#endif
#endif



#if CONFIG_RISCV_ISA_EXT_ZCF
// flwsp lfdsp fswsp fsdsp
#endif

// flw fld fsw fsd

ZTEST_SUITE(riscv_compressed, NULL, NULL, NULL, NULL, NULL);
