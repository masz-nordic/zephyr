.. _nordic-flpr:

Nordic FLPR snippet (nordic-flpr)
###############################

Overview
********

This snippet allows users to build Zephyr with the capability to boot Nordic FLPR
(Fast Lightweight Peripheral Processor) from application core.
FLPR code is to be executed from RRAM, so the PPR image must be built
for the ``xip`` board variant, or with :kconfig:option:`CONFIG_XIP` enabled.
