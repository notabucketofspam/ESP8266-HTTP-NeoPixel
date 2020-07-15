#!/usr/bin/env bash
yes | rm -r component; mkdir component; cd component
git clone https://github.com/adafruit/Adafruit_NeoPixel.git
git clone https://github.com/notabucketofspam/ESP8266-general-component.git
git clone https://github.com/notabucketofspam/ESP8266-HTML-GPIO.git
cd ..
