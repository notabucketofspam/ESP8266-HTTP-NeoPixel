#ifndef HW_WIFI_H
#define HW_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"

/*
 * Concise way of starting the WiFi
 * Uses the Kconfig values for the static IP address if enabled.
 */
void hw_setup_wifi(void);

#ifdef __cplusplus
}
#endif

#endif // HW_WIFI_H
