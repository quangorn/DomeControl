avr-objcopy.exe -j .text -j .data -O ihex %1 %1.hex
avr-size.exe -C --mcu=atmega8 %1
avrdude.exe -p m8 -c usbasp -P usb -U flash:w:%1.hex:i