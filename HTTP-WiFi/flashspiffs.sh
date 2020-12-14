#!/bin/sh
echo "Enter serial flasher port"
read serialflasherport
./util/mkspiffs/mkspiffs -c ./html/ -b 4096 -p 256 -s 0x210000 ./util/spiffs.bin
$IDF_PATH/components/esptool_py/esptool/esptool.py --port $serialflasherport --before default_reset write_flash 0x1F0000 ./util/spiffs.bin
