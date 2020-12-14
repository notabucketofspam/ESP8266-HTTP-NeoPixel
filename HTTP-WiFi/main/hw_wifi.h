#ifndef HW_WIFI_H
#define HW_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"

#include "hw_def.h"

/*
 * Concise way of starting the WiFi
 * Uses the Kconfig values for the static IP address if enabled.
 */
void vHwSetupWifi(void);

#ifdef __cplusplus
}
#endif

#endif // HW_WIFI_H
