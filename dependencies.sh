#!/usr/bin/env bash
yes | rm -r include;mkdir include; cd include
git clone https://github.com/adafruit/Adafruit_NeoPixel.git
git clone https://github.com/notabucketofspam/ESP8266-general-include.git
cp ESP8266-general-include/Kconfig.projbuild ../main/Kconfig.projbuild
git clone https://github.com/notabucketofspam/ESP8266-HTML-GPIO.git
cd ..
