# ESP8266-HTTP-NeoPixel

Interface for controlling a strip of Neopixel lights via a HTTP server hosting a HTML document.

Notes:

- This project requires two ESP8266 modules; one of them to run the NeoPixel strip, the other to run the HTTP server and WiFi. Consequently, the two sub-projects must be compiled separately.
- A level shifter from the NeoPixel module to the actual NeoPixel strip, since the former runs at 3V3 and the latter is 5V.
- SPI is used for communication between the modules instead of the UART serial system. This is done so that
  - a, the serial lines do not interfere with each other,
  - b, high-speed communication may be retained,
  - c, the two modules may be directly stacked on top of each other using tall female-to-male pin headers

Ideal use case: two Wemos D1 Mini modules and a perfboard shield utilizing the aforementioned stacking technique:

- Bottom: module with female pin headers facing upwards (on the side of the ESP-12F chip).
- Middle: perfboard shield with dedicated 5V power input on one end, level shifter in the center, and NeoPixel strip on the other end.
- Top: module with male pin headers facing downwards (on the side of the CH340 chip)

Which module is assigned to what sub-project is irrelevant. Refer to the MS Paint schematics in `resources/` for an implementation of the level shifter on perfboard shield.
