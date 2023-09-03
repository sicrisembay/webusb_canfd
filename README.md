# WebUSB CAN-FD
This is a USB device that allows a browser that supports WebUSB to access to a CAN-FD network.

# Note
1. Ubuntu 20.04 on Windows (WSL2) is used.
2. As of time writing, GNU ARM Embedded toolchain version 10.3-2021.10 is used.  See installation [link](https://askubuntu.com/questions/1243252/how-to-install-arm-none-eabi-gdb-on-ubuntu-20-04-lts-focal-fossa).
3. I'm using ST-LINK/V2.  Install stlink-tools (`sudo apt-get install stlink-tools`).  If you run some problems in connecting to ST-LINK, refer to this [link](https://github.com/stlink-org/stlink/blob/develop/doc/tutorial.md#solutions-to-common-problems)
4. I'm using VScode IDE.  For debugging in VSCODE, refer to this [video](https://youtu.be/g2Kf6RbdrIs?si=oaFvvUStnbVimICW).