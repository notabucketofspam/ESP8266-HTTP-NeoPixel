#ifndef HW_HTTP_H
#define HW_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "jsmn.h"

#include "hw_def.h"

/*
 * Allows us to later refer back to the main HTTP task
 */
extern TaskHandle_t xHttpServerTaskHandle;
/*
 * Simplifies things in app_main()
 */
void vHwHttpSetup(void);

#ifdef __cplusplus
}
#endif

#endif // HW_HTTP_H
