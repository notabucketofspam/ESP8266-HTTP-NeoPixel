#ifndef NP_ANP_H
#define NP_ANP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"

#include "np_def.h"

#include "anp_component.h"

/*
 * Handles for receiving different kinds data from the SPI task
 */
extern QueueHandle_t xSpiToAnpQueueHandle;
extern EventGroupHandle_t xSpiAndAnpEventGroupHandle;
//extern StreamBufferHandle_t xSpiToAnpStreamBufferHandle;
extern StreamBufferHandle_t xSpiStreamBufferHandle;
/*
 * Task handles
 */
extern TaskHandle_t xDynamicDataTaskHandle;
//extern TaskHandle_t xAnpStripTaskHandle;
/*
 * Uses the Kconfig values for the strip length
 */
void vNpSetupAnp(void);
/*
 * Deals solely with SPI stream buffer data
 */
void IRAM_ATTR vDynamicDataProcessTask(void *arg);
/*
 * Schedule sub-processes to run dynamic / static patterns
 */
//void IRAM_ATTR vNpAnpStripUpdateTask(void *arg);

#ifdef __cplusplus
}
#endif

#endif // NP_ANP_H
