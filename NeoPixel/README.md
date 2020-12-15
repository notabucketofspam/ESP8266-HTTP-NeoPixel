# NeoPixel

Notes:

- So it turns out that a level shifter might not be necessary after all. The official datasheet for the WS2821B states that there exists an "internal signal reshaping amplification circuit" which (theoretically) boosts the signal from 3.3V to 5V after the first LED.
- To add on to that, using a level shifter at all may be detrimental in the end. Most cheap off-the-shelf level shifters don't have a high enough switching speed for the 800 kHz signal required by WS2812B LEDs. The best alternatives are an op amp (such as the [CA3080](https://www.mouser.com/catalog/specsheets/intersil_fn475.pdf)) with a high slew rate (above 26 V/us) configured to mimic a traditional level shifter or an optocoupler (such as the [6N137](https://www.vishay.com/docs/84732/6n137.pdf)), either of which would need to be compatible with 3.3V signal logic input. Microchip has a diagram of the former configuration [here](https://www.newark.com/pdfs/techarticles/microchip/3_3vto5vAnalogTipsnTricksBrchr.pdf) (page 18), but again it might not be necessary to begin with.
