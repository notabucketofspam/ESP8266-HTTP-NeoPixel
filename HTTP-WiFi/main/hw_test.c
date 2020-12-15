#include "hw_test.h"

#ifdef __cplusplus
extern "C" {
#endif

static uint32_t ulWheelColor(uint32_t ulPosition);

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
//  TickType_t xFullTaskWakeTime = xTaskGetTickCount();
//  TickType_t xPreviousWakeTime = xTaskGetTickCount(); // Used in conjunction with vTaskDelayUntil()
//  const TickType_t xTimeIncrement = pdMS_TO_TICKS(100); // Convert milliseconds to FreeRTOS ticks
  const uint16_t usPixelIndexStart = 0;
  const uint16_t usPixelIndexEnd = 299;
  for (;;) {
    // Send a rainbow pattern; stolen from the Adafruit example
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
      for (uint32_t ulPixelIndex = usPixelIndexStart; ulPixelIndex < usPixelIndexEnd + 1; ++ulPixelIndex) {
        uint16_t usPixelHue = (256 * ulFirstPixelHue) + ((ulPixelIndex * (uint32_t) 65536) / usPixelIndexEnd);
        // The below is pretty much equivalent to anp_setPixelColor(),
        // in fact that's what it gets converted to on the NeoPixel device
        struct xHwDynamicData *pxDynamicData = malloc(sizeof(struct xHwDynamicData));
        pxDynamicData->ulPixelIndex = ulPixelIndex;
        pxDynamicData->ulColor = anp_ColorHSV(usPixelHue, 255, 255);
//        ESP_LOGI(__ESP_FILE__, "pixel %u, red %u, green %u, blue%u", pxDynamicData->ulPixelIndex,
//          pxDynamicData->ulColor >> 16 & 0xFF, pxDynamicData->ulColor >>  8 & 0xFF, pxDynamicData->ulColor & 0xFF);
        pxDynamicDataPointerArray[ulPixelIndex - usPixelIndexStart] = pxDynamicData;
        xStreamBufferSend(*pxTestMessage->pxStreamBufferHandle, (void *) pxDynamicData, sizeof(struct xHwDynamicData),
          portMAX_DELAY);
      }
      xQueueSendToBack(xHttpToSpiQueueHandle, (void *) &pxTestMessage, portMAX_DELAY);
      xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_START);
      xEventGroupWaitBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
      free(pxTestMessage->pxMetadata);
      // Do a loop to free each of the xHwDynamicData structs, i.e. each pixel in the strip
      for (uint16_t ulPixelIndex = usPixelIndexStart; ulPixelIndex < usPixelIndexEnd; ++ulPixelIndex) {
        free(pxDynamicDataPointerArray[ulPixelIndex - usPixelIndexStart]);
      }
      free(pxDynamicDataPointerArray);
      xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_HTTP_MSG_MEM_FREE);
//      vTaskDelayUntil(&xPreviousWakeTime, xTimeIncrement);
//      ESP_LOGI(__ESP_FILE__, "Cycle complete");
//      vTaskDelayUntil(&xPreviousWakeTime, pdMS_TO_TICKS(10000));
    }
    free(pxTestMessage);
    ESP_LOGI(__ESP_FILE__, "Task complete");
//    vTaskDelayUntil(&xFullTaskWakeTime, pdMS_TO_TICKS(300000));
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
void vHwTestSpiDynamicTaskTwo(void *arg) {
  TickType_t xPreviousWakeTime = xTaskGetTickCount();
  const TickType_t xTimeIncrement = pdMS_TO_TICKS(50);
  const uint32_t ulPixelIndexStart = 0;
  const uint32_t ulPixelIndexEnd = 299;
  for (;;) {
    // Send a rainbow pattern; stolen from the Adafruit example
    struct xHwMessage *pxTestMessage = malloc(sizeof(struct xHwMessage));
    pxTestMessage->pxStreamBufferHandle = &xHttpToSpiStreamBufferHandle;
    // Need an array so that we can free its elements later
    struct xHwDynamicData *pxDynamicDataPointerArray = calloc(( 1 + ulPixelIndexEnd - ulPixelIndexStart),
      sizeof(struct xHwDynamicData));
    for (uint32_t ulPixelIndex = ulPixelIndexStart; ulPixelIndex < ulPixelIndexEnd + 1; ++ulPixelIndex) {
      struct xHwMessageMetadata *pxTestMetadata = malloc(sizeof(struct xHwMessageMetadata));
      strcpy(pxTestMetadata->pcName, "pxTestMessage");
      pxTestMetadata->xType = DYNAMIC_PATTERN_DATA;
      pxTestMessage->pxMetadata = pxTestMetadata;
      // The below is pretty much equivalent to anp_setPixelColor(),
      // in fact that's what it gets converted to on the NeoPixel device
      struct xHwDynamicData xDynamicData;
      xDynamicData.ulPixelIndex = ulPixelIndex;
      xDynamicData.ulColor = anp_Color_RGB(0, 127, 0);
//        ESP_LOGI(__ESP_FILE__, "pixel %u, red %u, green %u, blue%u", xDynamicData.ulPixelIndex,
//          xDynamicData.ulColor >> 16 & 0xFF, xDynamicData.ulColor >>  8 & 0xFF, xDynamicData.ulColor & 0xFF);
//      ESP_LOGI(__ESP_FILE__, "point c + %u", ulPixelIndex);
      pxDynamicDataPointerArray[ulPixelIndex - ulPixelIndexStart] = xDynamicData;
//      ESP_LOGI(__ESP_FILE__, "point d + %u", ulPixelIndex);
      xStreamBufferSend(*pxTestMessage->pxStreamBufferHandle, (void *) &xDynamicData, sizeof(struct xHwDynamicData),
        portMAX_DELAY);
//      ESP_LOGI(__ESP_FILE__, "point e + %u", ulPixelIndex);
      xQueueSendToBack(xHttpToSpiQueueHandle, (void *) &pxTestMessage, portMAX_DELAY);
//      ESP_LOGI(__ESP_FILE__, "point f + %u", ulPixelIndex);
      xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_START);
//      ESP_LOGI(__ESP_FILE__, "point g + %u", ulPixelIndex);
      xEventGroupWaitBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
//      ESP_LOGI(__ESP_FILE__, "point h + %u", ulPixelIndex);
      free(pxTestMessage->pxMetadata);
      // Do a loop to free each of the xHwDynamicData structs, i.e. each pixel in the strip
//      for (uint32_t ulPixelIndex = ulPixelIndexStart; ulPixelIndex < ulPixelIndexEnd; ++ulPixelIndex) {
//        free(pxDynamicDataPointerArray[ulPixelIndex - ulPixelIndexStart]);
//      }
//      ESP_LOGI(__ESP_FILE__, "point b + %u", ulPixelIndex);
      xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_HTTP_MSG_MEM_FREE);
//      ESP_LOGI(__ESP_FILE__, "heap free: %u", esp_get_free_heap_size());
      vTaskDelayUntil(&xPreviousWakeTime, xTimeIncrement);
    }
//    ESP_LOGI(__ESP_FILE__, "point a");
    free(pxDynamicDataPointerArray);
    free(pxTestMessage);
//    ESP_LOGI(__ESP_FILE__, "Task complete");
//    vTaskDelayUntil(&xFullTaskWakeTime, pdMS_TO_TICKS(300000));
  }
}
void vHwTestSpiDynamicTaskThree(void *arg) {
  TickType_t xPreviousWakeTime = xTaskGetTickCount();
  const TickType_t xTimeIncrement = pdMS_TO_TICKS(50);
  const uint32_t ulPixelIndexStart = 0;
  const uint32_t ulPixelIndexEnd = CONFIG_HW_NEOPIXEL_COUNT - 1;
  for (;;) {
    // This time we're stealing from the wheel example, specifically rainbowCycle()
    struct xHwMessage *pxTestMessage = malloc(sizeof(struct xHwMessage));
    pxTestMessage->pxStreamBufferHandle = &xHttpToSpiStreamBufferHandle;
    struct xHwDynamicData **pxDynamicDataPointerArray = calloc(( 1 + ulPixelIndexEnd - ulPixelIndexStart),
      sizeof(struct xHwDynamicData *));
    for (uint32_t ulWheelPosition = 0; ulWheelPosition < 0x0100; ++ulWheelPosition) {
      struct xHwMessageMetadata *pxTestMetadata = malloc(sizeof(struct xHwMessageMetadata));
      strcpy(pxTestMetadata->pcName, "pxTestMessage");
      pxTestMetadata->xType = DYNAMIC_PATTERN_DATA;
      pxTestMessage->pxMetadata = pxTestMetadata;
      for (uint32_t ulPixelIndex = ulPixelIndexStart; ulPixelIndex < ulPixelIndexEnd + 1; ++ulPixelIndex) {
        struct xHwDynamicData *pxDynamicData = malloc(sizeof(struct xHwDynamicData));
        pxDynamicData->ulPixelIndex = ulPixelIndex;
        pxDynamicData->ulColor = ulWheelColor((((ulPixelIndex * 0x0100) / (1 + ulPixelIndexEnd -
          ulPixelIndexStart)) + ulWheelPosition) & 0xFF);
//        ESP_LOGI(__ESP_FILE__, "pixel %u, red %u, green %u, blue%u", pxDynamicData->ulPixelIndex,
//          pxDynamicData->ulColor >> 16 & 0xFF, pxDynamicData->ulColor >>  8 & 0xFF, pxDynamicData->ulColor & 0xFF);
        pxDynamicDataPointerArray[ulPixelIndex - ulPixelIndexStart] = pxDynamicData;
        xStreamBufferSend(*pxTestMessage->pxStreamBufferHandle, (void *) pxDynamicData, sizeof(struct xHwDynamicData),
          portMAX_DELAY);
//        ESP_LOGI(__ESP_FILE__, "SB spaces avail %u", xStreamBufferSpacesAvailable(xHttpToSpiStreamBufferHandle));
      }
      xQueueSendToBack(xHttpToSpiQueueHandle, (void *) &pxTestMessage, portMAX_DELAY);
      xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_START);
      xEventGroupWaitBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_END, pdTRUE, pdTRUE, portMAX_DELAY);
      free(pxTestMessage->pxMetadata);
      for (uint32_t ulPixelIndex = ulPixelIndexStart; ulPixelIndex < ulPixelIndexEnd + 1; ++ulPixelIndex) {
        free(pxDynamicDataPointerArray[ulPixelIndex - ulPixelIndexStart]);
      }
      xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_HTTP_MSG_MEM_FREE);
//      ESP_LOGI(__ESP_FILE__, "mem free: %u", esp_get_free_heap_size());
      vTaskDelayUntil(&xPreviousWakeTime, xTimeIncrement);
    }
    free(pxDynamicDataPointerArray);
    free(pxTestMessage);
//    ESP_LOGI(__ESP_FILE__, "Task complete");
//    vTaskDelayUntil(&xFullTaskWakeTime, pdMS_TO_TICKS(300000));
  }
}
static uint32_t ulWheelColor(uint32_t ulPosition) {
  ulPosition = 0xFF - ulPosition;
  if (ulPosition < 0x55) {
    return anp_Color_RGB(0xFF - (ulPosition * 3), 0, ulPosition * 3);
  }
  if (ulPosition < 0xAA) {
    ulPosition -= 0x55;
    return anp_Color_RGB(0, ulPosition * 3, 0xFF - (ulPosition * 3));
  }
  ulPosition -= 0xAA;
  return anp_Color_RGB(ulPosition * 3, 0xFF - (ulPosition * 3), 0);
}
void vHwPrintIpTask(void *arg) {
  tcpip_adapter_ip_info_t xIpInfoSta;
    tcpip_adapter_dhcp_status_t xDhcpcStatus;
  for (;;) {
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &xIpInfoSta);
    ESP_LOGI(__ESP_FILE__, "IP " IPSTR, IP2STR(&xIpInfoSta.ip));
    tcpip_adapter_dhcpc_get_status(TCPIP_ADAPTER_IF_STA, &xDhcpcStatus);
//    switch (xDhcpcStatus) {
//      case (TCPIP_ADAPTER_DHCP_INIT): {
//        ESP_LOGI(__ESP_FILE__, "DHCPC status: TCPIP_ADAPTER_DHCP_INIT");
//        break;
//      }
//      case (TCPIP_ADAPTER_DHCP_STARTED): {
//        ESP_LOGI(__ESP_FILE__, "DHCPC status: TCPIP_ADAPTER_DHCP_STARTED");
//        break;
//      }
//      case (TCPIP_ADAPTER_DHCP_STOPPED): {
//        ESP_LOGI(__ESP_FILE__, "DHCPC status: TCPIP_ADAPTER_DHCP_STOPPED");
//        break;
//      }
//      case (TCPIP_ADAPTER_DHCP_STATUS_MAX): {
//        ESP_LOGI(__ESP_FILE__, "DHCPC status: TCPIP_ADAPTER_DHCP_STATUS_MAX");
//        break;
//      }
//      default: {
//        ESP_LOGI(__ESP_FILE__, "DHCPC status: NaN");
//        break;
//      }
//    }
    vTaskDelay(pdMS_TO_TICKS(4000));
  }
}
char *pcHwResetReason(void) {
  switch (esp_reset_reason()) {
    case (ESP_RST_UNKNOWN): {
      return "ESP_RST_UNKNOWN";
      break;
    }
    case (ESP_RST_POWERON): {
      return "ESP_RST_POWERON";
      break;
    }
    case (ESP_RST_EXT): {
      return "ESP_RST_EXT";
      break;
    }
    case (ESP_RST_SW): {
      return "ESP_RST_SW";
      break;
    }
    case (ESP_RST_PANIC): {
      return "ESP_RST_PANIC";
      break;
    }
    case (ESP_RST_INT_WDT): {
      return "ESP_RST_INT_WDT";
      break;
    }
    case (ESP_RST_TASK_WDT): {
      return "ESP_RST_TASK_WDT";
      break;
    }
    case (ESP_RST_WDT): {
      return "ESP_RST_WDT";
      break;
    }
    case (ESP_RST_DEEPSLEEP): {
      return "ESP_RST_DEEPSLEEP";
      break;
    }
    case (ESP_RST_BROWNOUT): {
      return "ESP_RST_BROWNOUT";
      break;
    }
    case (ESP_RST_SDIO): {
      return "ESP_RST_SDIO";
      break;
    }
    case (ESP_RST_FAST_SW): {
      return "ESP_RST_FAST_SW";
      break;
    }
    default: {
      return "I have no idea lol";
      break;
    }
  }
  return "Something ain't right here";
}
void vHwPrintTimeTask(void *arg) {
  TickType_t xPreviousWakeTime = xTaskGetTickCount();
  const TickType_t xTimeIncrement = pdMS_TO_TICKS(((uint32_t) 60) * 1000);
  for (;;) {
    vTaskDelayUntil(&xPreviousWakeTime, xTimeIncrement);
    ESP_LOGI(__ESP_FILE__, "minutes %u, mem free %u", xPreviousWakeTime / configTICK_RATE_HZ / 60,
      esp_get_free_heap_size());
  }
}

#ifdef __cplusplus
}
#endif
