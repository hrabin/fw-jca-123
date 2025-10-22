#!/bin/bash

# low level update FW using serial port communication (USART1 debug port)

DEVICE=/dev/ttyUSB0

cd ./app/
make deploy

# command for reboot and stay in bootloader
echo "REBOOT=1" > $DEVICE

sleep 1
cat ./build/JCA-123-*_container.hex | ../ut/upload/upload.bin -d $DEVICE
sleep 1
echo "F" > $DEVICE

