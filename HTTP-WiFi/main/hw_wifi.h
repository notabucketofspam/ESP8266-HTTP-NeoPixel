#ifndef HW_WIFI_H
#define HW_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hw_def.h"

/*
 * This will probably be important later
 */
extern TaskHandle_t xWifiTaskHandle;

/*
 * Concise way of starting the WiFi
 * Uses the Kconfig values for the static IP address if enabled.
 */
void vHwSetupWifi(void);

#ifdef __cplusplus
}
#endif

#endif // HW_WIFI_H
