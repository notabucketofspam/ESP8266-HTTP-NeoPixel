#ifndef HW_TEST_H
#define HW_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "esp_log.h"

#include "hw_def.h"

#include "anp_component.h"

extern QueueHandle_t http_to_spi_queue_handle;
extern EventGroupHandle_t http_and_spi_event_group_handle;
extern StreamBufferHandle_t http_to_spi_stream_buffer_handle;

void test_spi_pattern_task(void *arg);

void test_spi_dynamic_task (void *arg);

void test_spi_pattern_array_task(void *arg);

#ifdef __cplusplus
}
#endif

#endif // HW_TEST_H
