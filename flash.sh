#!/bin/bash

# low level update FW using serial port communication (USART1 debug port)

DEVICE=/dev/ttyUSB0

cd ./app/
make deploy

echo "auth=\"uhlokRopnude\"" > $DEVICE
sleep 0.2

# command for reboot and stay in bootloader
echo "REBOOT=1" > $DEVICE

sleep 2

# flush all old responses 
while read -t 0.1 -r discard; do :; done < /dev/ttyUSB0

cat ./build/JCA-123-*_container.hex | ../ut/upload/upload.bin -d $DEVICE
sleep 1
echo "F" > $DEVICE

