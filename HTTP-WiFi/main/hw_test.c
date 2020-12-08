#include "hw_test.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_HW_ENABLE_STATIC_PATTERN & 0
void test_spi_pattern_task(void *arg) {
  for (;;) {
    // Set the strip to red
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    struct xHwMessage *test_message_red = malloc(sizeof(struct xHwMessage));
    struct xHwMessageMetadata *test_metadata_red = malloc(sizeof(struct xHwMessageMetadata));
    strcpy(test_metadata_red->pcName, "test_message_red");
    test_metadata_red->xType = STATIC_PATTERN_DATA;
    test_message_red->pxMetadata = test_metadata_red;
    struct xHwStaticData *test_pattern_data_red = malloc(sizeof(struct xHwStaticData));
    test_pattern_data_red->bVal = 0;
    test_pattern_data_red->bCmd = 0;
    test_pattern_data_red->bPattern = FILL_COLOR;
    test_pattern_data_red->bPixelIndexStart = 0;
    test_pattern_data_red->bPixelIndexEnd = 29;
    test_pattern_data_red->ulDelay = 0;
    test_pattern_data_red->ulColor = 0x00FF0000;
    test_message_red->pattern_data = test_pattern_data_red;
    xQueueSendToBack(xHttpToSpiQueueHandle, (void *) &test_message_red, portMAX_DELAY);
    xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_START);
    xEventGroupWaitBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
    free(test_message_red->pxMetadata);
    free(test_message_red->pattern_data);
    free(test_message_red);
    xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_HTTP_MSG_MEM_FREE);
    // Set the strip to green
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    struct xHwMessage *test_message_green = malloc(sizeof(struct xHwMessage));
    struct xHwMessageMetadata *test_metadata_green = malloc(sizeof(struct xHwMessageMetadata));
    strcpy(test_metadata_green->pcName, "test_message_green");
    test_metadata_green->xType = STATIC_PATTERN_DATA;
    test_message_green->pxMetadata = test_metadata_green;
    struct xHwStaticData *test_pattern_data_green = malloc(sizeof(struct xHwStaticData));
    test_pattern_data_green->bVal = 0;
    test_pattern_data_green->bCmd = 0;
    test_pattern_data_green->bPattern = FILL_COLOR;
    test_pattern_data_green->bPixelIndexStart = 0;
    test_pattern_data_green->bPixelIndexEnd = 29;
    test_pattern_data_green->ulDelay = 0;
    test_pattern_data_green->ulColor = 0x0000FF00;
    test_message_green->pattern_data = test_pattern_data_green;
    xQueueSendToBack(xHttpToSpiQueueHandle, (void *) &test_message_green, portMAX_DELAY);
    xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_START);
    xEventGroupWaitBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
    free(test_message_green->pxMetadata);
    free(test_message_green->pattern_data);
    free(test_message_green);
    xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_HTTP_MSG_MEM_FREE);
  }
}
#endif
void vHwTestSpiDynamicTask(void *arg) {
  TickType_t xPreviousWakeTime = xTaskGetTickCount(); // Used in conjunction with vTaskDelayUntil()
  TickType_t xFullTaskWakeTime = xTaskGetTickCount();
  const TickType_t xTimeIncrement = pdMS_TO_TICKS(30); // Convert milliseconds to FreeRTOS ticks
  const uint16_t usPixelIndexStart = 0;
  const uint16_t usPixelIndexEnd = 299;
  for (;;) {
    // Send a rainbow bPattern; stolen from the Adafruit example
    struct xHwMessage *pxTestMessage = malloc(sizeof(struct xHwMessage));
    pxTestMessage->pxStreamBufferHandle = &xHttpToSpiStreamBufferHandle;
    for (uint32_t ulFirstPixelHue = 0; ulFirstPixelHue < 5 * 256; ++ulFirstPixelHue) {
      struct xHwMessageMetadata *pxTestMetadata = malloc(sizeof(struct xHwMessageMetadata));
      strcpy(pxTestMetadata->pcName, "pxTestMessage");
      pxTestMetadata->xType = DYNAMIC_PATTERN_DATA;
      pxTestMessage->pxMetadata = pxTestMetadata;
      // Need an array so that we can free its elements later
      struct xHwDynamicData **pxDynamicDataPointerArray = malloc((usPixelIndexEnd - usPixelIndexStart) *
        sizeof(struct xHwDynamicData *));
      for (uint16_t bPixelIndex = usPixelIndexStart; bPixelIndex < usPixelIndexEnd; ++bPixelIndex) {
        uint16_t usPixelHue = (256 * ulFirstPixelHue) + ((bPixelIndex * (uint32_t) 65536) / usPixelIndexEnd);
        // The below is pretty much equivalent to anp_setPixelColor(),
        // in fact that's what it gets converted to on the NeoPixel device
        struct xHwDynamicData *pxDynamicData = malloc(sizeof(struct xHwDynamicData));
        pxDynamicData->bPixelIndex = bPixelIndex;
        pxDynamicData->ulColor = anp_ColorHSV(usPixelHue, 255, 255);
        pxDynamicDataPointerArray[bPixelIndex - usPixelIndexStart] = pxDynamicData;
        xStreamBufferSend(*pxTestMessage->pxStreamBufferHandle, (void *) pxDynamicData, sizeof(pxDynamicData),
          portMAX_DELAY);
      }
      xQueueSendToBack(xHttpToSpiQueueHandle, (void *) &pxTestMessage, portMAX_DELAY);
      xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_START);
      xEventGroupWaitBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
      free(pxTestMessage->pxMetadata);
      // Do a loop to free each of the xHwDynamicData structs, i.e. each pixel in the strip
      for (uint16_t bPixelIndex = usPixelIndexStart; bPixelIndex < usPixelIndexEnd; ++bPixelIndex) {
        free(pxDynamicDataPointerArray[bPixelIndex - usPixelIndexStart]);
      }
      free(pxDynamicDataPointerArray);
      xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_HTTP_MSG_MEM_FREE);
      vTaskDelayUntil(&xPreviousWakeTime, xTimeIncrement);
    }
    free(pxTestMessage);
    ESP_LOGI(__ESP_FILE__, "Task complete");
    vTaskDelayUntil(&xFullTaskWakeTime, pdMS_TO_TICKS(30000));
  }
}
#if CONFIG_HW_ENABLE_STATIC_PATTERN & 0
void test_spi_pattern_array_task(void *arg) {
//  TickType_t xPreviousWakeTime = xTaskGetTickCount();
  const uint32_t pattern_data_array_length = 4;
  for (;;) {
    struct xHwMessage *pxTestMessage = malloc(sizeof(struct xHwMessage));
    struct xHwMessageMetadata *pxTestMetadata = malloc(sizeof(struct xHwMessageMetadata));
    strcpy(pxTestMetadata->pcName, "pxTestMessage");
    pxTestMetadata->xType = STATIC_PATTERN_DATA;
    pxTestMessage->pxMetadata = pxTestMetadata;
    struct xHwStaticData **pattern_data_array = calloc(pattern_data_array_length, sizeof(struct xHwStaticData *));
    // Blank section
    struct xHwStaticData *pattern_data_0 = malloc(sizeof(struct xHwStaticData));
    pattern_data_0->bVal = 0;
    pattern_data_0->bPattern = NO_PATTERN;
    pattern_data_0->bPixelIndexStart = 0;
    pattern_data_0->bPixelIndexEnd = 7;
    pattern_data_0->ulDelay = 0;
    pattern_data_0->ulColor = 0x00;
    pattern_data_array[0] = pattern_data_0;
    // Red section
    struct xHwStaticData *pattern_data_1 = malloc(sizeof(struct xHwStaticData));
    pattern_data_1->bVal = 0;
    pattern_data_1->bPattern = NO_PATTERN;
    pattern_data_1->bPixelIndexStart = 8;
    pattern_data_1->bPixelIndexEnd = 15;
    pattern_data_1->ulDelay = 0;
    pattern_data_1->ulColor = 0xFF << 16;
    pattern_data_array[1] = pattern_data_1;
    // Green section
    struct xHwStaticData *pattern_data_2 = malloc(sizeof(struct xHwStaticData));
    pattern_data_2->bVal = 0;
    pattern_data_2->bPattern = NO_PATTERN;
    pattern_data_2->bPixelIndexStart = 16;
    pattern_data_2->bPixelIndexEnd = 23;
    pattern_data_2->ulDelay = 0;
    pattern_data_2->ulColor = 0xFF << 8;
    pattern_data_array[2] = pattern_data_2;
    // Blue section
    struct xHwStaticData *pattern_data_3 = malloc(sizeof(struct xHwStaticData));
    pattern_data_3->bVal = 0;
    pattern_data_3->bPattern = NO_PATTERN;
    pattern_data_3->bPixelIndexStart = 24;
    pattern_data_3->bPixelIndexEnd = 31;
    pattern_data_3->ulDelay = 0;
    pattern_data_3->ulColor = 0xFF;
    pattern_data_array[3] = pattern_data_3;
    pxTestMessage->pattern_data_array_length = pattern_data_array_length;
    pxTestMessage->pattern_data_array = pattern_data_array;
    xQueueSendToBack(xHttpToSpiQueueHandle, (void *) &pxTestMessage, portMAX_DELAY);
    xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_START);
    xEventGroupWaitBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
    free(pxTestMessage->pxMetadata);
    for (uint16_t pattern_data_array_index = 0; pattern_data_array_index < pattern_data_array_length;
      ++pattern_data_array_index) {
      free(pxTestMessage->pattern_data_array[pattern_data_array_index]);
    }
    free(pxTestMessage->pattern_data_array);
    free(pxTestMessage);
    xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_HTTP_MSG_MEM_FREE);
  }
}
#endif

#ifdef __cplusplus
}
#endif
