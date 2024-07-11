.. zephyr:code-sample:: ipc-basic
   :name: IPC service: basic struct
   :relevant-api: ipc

   Send messages between two cores using mbox and shared data structure.

Overview
********

This application demonstrates how to communcate between cores without any backend in
Zephyr. It is designed to demonstrate how to integrate it with Zephyr both
from a build perspective and code.

Building the application for nrf54l15pdk/nrf54l15/cpuapp
*****************************************************

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/ipc/basic
   :board: nrf54l15pdk/nrf54l15/cpuapp
   :goals: debug
   :west-args: --sysbuild

Open a serial terminal (minicom, putty, etc.) and connect the board with the
following settings:

- Speed: 115200
- Data: 8 bits
- Parity: None
- Stop bits: 1

Reset the board and the following message will appear on the corresponding
serial port, one is host another is remote:

.. code-block:: console

   *** Booting Zephyr OS build v3.7.0-rc1-427-g7f891a6a40e8 ***
   I: No data from remote
   I: Sent 14 bytes to host
   I: Read 14 bytes from remote, received 14
   I: Basic core-communication HOST demo ended

.. code-block:: console

   *** Booting Zephyr OS build v3.7.0-rc1-427-g7f891a6a40e8 ***
   I: No data from host
   I: Read 14 bytes from host, received 14
   I: Sent 14 bytes to host
   I: Basic core-communication REMOTE demo ended
