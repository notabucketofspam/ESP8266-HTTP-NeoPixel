#include "hw_test.h"

#ifdef __cplusplus
extern "C" {
#endif

void test_spi_pattern_task(void *arg) {
  for (;;) {
    // Set the strip to red
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    struct hw_message *test_message_red = malloc(sizeof(struct hw_message));
    struct hw_message_metadata *test_metadata_red = malloc(sizeof(struct hw_message_metadata));
    strcpy(test_metadata_red->name, "test_message_red");
    test_metadata_red->type = STATIC_PATTERN_DATA;
    test_message_red->metadata = test_metadata_red;
    struct hw_pattern_data *test_pattern_data_red = malloc(sizeof(struct hw_pattern_data));
    test_pattern_data_red->val = 0;
    test_pattern_data_red->cmd = 0;
    test_pattern_data_red->pattern = FILL_COLOR;
    test_pattern_data_red->pixel_index_start = 0;
    test_pattern_data_red->pixel_index_end = 29;
    test_pattern_data_red->delay = 0;
    test_pattern_data_red->color = 0x00FF0000;
    test_message_red->pattern_data = test_pattern_data_red;
    xQueueSendToBack(http_to_spi_queue_handle, (void *) &test_message_red, portMAX_DELAY);
    xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_START);
    xEventGroupWaitBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
    free(test_message_red->metadata);
    free(test_message_red->pattern_data);
    free(test_message_red);
    xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_HTTP_MSG_MEM_FREE);
    // Set the strip to green
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    struct hw_message *test_message_green = malloc(sizeof(struct hw_message));
    struct hw_message_metadata *test_metadata_green = malloc(sizeof(struct hw_message_metadata));
    strcpy(test_metadata_green->name, "test_message_green");
    test_metadata_green->type = STATIC_PATTERN_DATA;
    test_message_green->metadata = test_metadata_green;
    struct hw_pattern_data *test_pattern_data_green = malloc(sizeof(struct hw_pattern_data));
    test_pattern_data_green->val = 0;
    test_pattern_data_green->cmd = 0;
    test_pattern_data_green->pattern = FILL_COLOR;
    test_pattern_data_green->pixel_index_start = 0;
    test_pattern_data_green->pixel_index_end = 29;
    test_pattern_data_green->delay = 0;
    test_pattern_data_green->color = 0x0000FF00;
    test_message_green->pattern_data = test_pattern_data_green;
    xQueueSendToBack(http_to_spi_queue_handle, (void *) &test_message_green, portMAX_DELAY);
    xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_START);
    xEventGroupWaitBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
    free(test_message_green->metadata);
    free(test_message_green->pattern_data);
    free(test_message_green);
    xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_HTTP_MSG_MEM_FREE);
  }
}
void test_spi_dynamic_task (void *arg) {
  TickType_t xPreviousWakeTime = xTaskGetTickCount(); // Used in conjunction with vTaskDelayUntil()
  const TickType_t xTimeIncrement = 30 / portTICK_PERIOD_MS; // Convert milliseconds to FreeRTOS ticks
  const uint16_t pixel_index_start = 0;
  const uint16_t pixel_index_end = 299;
  for (;;) {
    // Send a rainbow pattern; stolen from the Adafruit example
    struct hw_message *test_message = malloc(sizeof(struct hw_message));
    test_message->xStreamBufferHandle = &http_to_spi_stream_buffer_handle;
    for (uint32_t first_pixel_hue = 0; first_pixel_hue < 5 * 256; ++first_pixel_hue) {
      struct hw_message_metadata *test_metadata = malloc(sizeof(struct hw_message_metadata));
      strcpy(test_metadata->name, "test_message");
      test_metadata->type = DYNAMIC_PATTERN_DATA;
      test_message->metadata = test_metadata;
      // Need an array so that we can free its elements later
      struct hw_dynamic_data **dynamic_data_pointer_array = malloc((pixel_index_end - pixel_index_start) *
        sizeof(struct hw_dynamic_data *));
      for (uint16_t pixel_index = pixel_index_start; pixel_index < pixel_index_end; ++pixel_index) {
        uint16_t pixel_hue = (256 * first_pixel_hue) + ((pixel_index * (uint32_t) 65536) / pixel_index_end);
        // The below is pretty much equivalent to anp_setPixelColor(),
        // in fact that's what it gets converted to on the NeoPixel device
        struct hw_dynamic_data *dynamic_data = malloc(sizeof(struct hw_dynamic_data));
        dynamic_data->pixel_index = pixel_index;
        dynamic_data->color = anp_ColorHSV(pixel_hue, 255, 255);
        dynamic_data_pointer_array[pixel_index - pixel_index_start] = dynamic_data;
        xStreamBufferSend(*test_message->xStreamBufferHandle, (void *) dynamic_data, sizeof(dynamic_data),
          portMAX_DELAY);
      }
      xQueueSendToBack(http_to_spi_queue_handle, (void *) &test_message, portMAX_DELAY);
      xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_START);
      xEventGroupWaitBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
      free(test_message->metadata);
      // Do a loop to free each of the hw_dynamic_data structs, i.e. each pixel in the strip
      for (uint16_t pixel_index = pixel_index_start; pixel_index < pixel_index_end; ++pixel_index) {
        free(dynamic_data_pointer_array[pixel_index - pixel_index_start]);
      }
      free(dynamic_data_pointer_array);
      xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_HTTP_MSG_MEM_FREE);
      vTaskDelayUntil(&xPreviousWakeTime, xTimeIncrement);
      ESP_LOGI(__ESP_FILE__, "One loop finished");
//      vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    free(test_message);
    ESP_LOGI(__ESP_FILE__, "Task complete");
    vTaskDelayUntil(&xPreviousWakeTime, 10000 / portTICK_PERIOD_MS);
  }
}
void test_spi_pattern_array_task(void *arg) {
//  TickType_t xPreviousWakeTime = xTaskGetTickCount();
  const uint32_t pattern_data_array_length = 4;
  for (;;) {
    struct hw_message *test_message = malloc(sizeof(struct hw_message));
    struct hw_message_metadata *test_metadata = malloc(sizeof(struct hw_message_metadata));
    strcpy(test_metadata->name, "test_message");
    test_metadata->type = STATIC_PATTERN_DATA;
    test_message->metadata = test_metadata;
    struct hw_pattern_data **pattern_data_array = calloc(pattern_data_array_length, sizeof(struct hw_pattern_data *));
    // Blank section
    struct hw_pattern_data *pattern_data_0 = malloc(sizeof(struct hw_pattern_data));
    pattern_data_0->val = 0;
    pattern_data_0->pattern = NO_PATTERN;
    pattern_data_0->pixel_index_start = 0;
    pattern_data_0->pixel_index_end = 7;
    pattern_data_0->delay = 0;
    pattern_data_0->color = 0x00;
    pattern_data_array[0] = pattern_data_0;
    // Red section
    struct hw_pattern_data *pattern_data_1 = malloc(sizeof(struct hw_pattern_data));
    pattern_data_1->val = 0;
    pattern_data_1->pattern = NO_PATTERN;
    pattern_data_1->pixel_index_start = 8;
    pattern_data_1->pixel_index_end = 15;
    pattern_data_1->delay = 0;
    pattern_data_1->color = 0xFF << 16;
    pattern_data_array[1] = pattern_data_1;
    // Green section
    struct hw_pattern_data *pattern_data_2 = malloc(sizeof(struct hw_pattern_data));
    pattern_data_2->val = 0;
    pattern_data_2->pattern = NO_PATTERN;
    pattern_data_2->pixel_index_start = 16;
    pattern_data_2->pixel_index_end = 23;
    pattern_data_2->delay = 0;
    pattern_data_2->color = 0xFF << 8;
    pattern_data_array[2] = pattern_data_2;
    // Blue section
    struct hw_pattern_data *pattern_data_3 = malloc(sizeof(struct hw_pattern_data));
    pattern_data_3->val = 0;
    pattern_data_3->pattern = NO_PATTERN;
    pattern_data_3->pixel_index_start = 24;
    pattern_data_3->pixel_index_end = 31;
    pattern_data_3->delay = 0;
    pattern_data_3->color = 0xFF;
    pattern_data_array[3] = pattern_data_3;
    test_message->pattern_data_array_length = pattern_data_array_length;
    test_message->pattern_data_array = pattern_data_array;
    xQueueSendToBack(http_to_spi_queue_handle, (void *) &test_message, portMAX_DELAY);
    xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_START);
    xEventGroupWaitBits(http_and_spi_event_group_handle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
    free(test_message->metadata);
    for (uint16_t pattern_data_array_index = 0; pattern_data_array_index < pattern_data_array_length;
      ++pattern_data_array_index) {
      free(test_message->pattern_data_array[pattern_data_array_index]);
    }
    free(test_message->pattern_data_array);
    free(test_message);
    xEventGroupSetBits(http_and_spi_event_group_handle, HW_BIT_HTTP_MSG_MEM_FREE);
  }
}

#ifdef __cplusplus
}
#endif
