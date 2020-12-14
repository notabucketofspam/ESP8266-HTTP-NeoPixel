# HTTP-WiFi

Making / flashing the SPIFFS image:

- `./makemkspiffs.sh` (this only needs to be run once)
- `make flash`
- `./flashspiffs.sh`
- `make app-flash`

Other notes:

- If using a static IP, set `tcpip adapter -> IP Address lost time interval (seconds)` in menuconfig to zero. This should've been something that's dealt with in the code, but order of operations in the preprocessor seems to negate it.
- For whatever reason, `esp_wifi_connect()` often hangs with the message `wifi:state: 0 -> 2 (b0)`. The task watchdog will subsequently restart the module and (hopefully) reconnect the second time