#include "hw_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sets up the GPIO pin as a falling-edge interrupt with the pin number as a parameter
 * Change to GPIO_INTR_POSEDGE, maybe
 */
#define HW_GPIO_CONFIG_DEFAULT(x) { \
  .pin_bit_mask = BIT(x), \
  .mode = GPIO_MODE_INPUT, \
  .pull_up_en = GPIO_PULLUP_DISABLE, \
  .pull_down_en = GPIO_PULLDOWN_DISABLE, \
  .intr_type = GPIO_INTR_NEGEDGE \
}
/*
 * Sets up SPI as master with clk_div selected as an input
 */
#define HW_SPI_CONFIG_DEFAULT(x) { \
  .interface.val = SPI_DEFAULT_INTERFACE, \
  .intr_enable.val = SPI_MASTER_DEFAULT_INTR_ENABLE, \
  .mode = SPI_MASTER_MODE, \
  .clk_div = x, \
  .event_cb = NULL \
}
/*
 * Direction of SPI communication.
 * Pretty much only used for spi_master_transmit()
 */
enum spi_master_mode {
  SPI_NULL = 0,
  SPI_WRITE,
  SPI_READ
};
/*
 * Send or receive data as the master
 * Assumes that the data length is 64-byte (the maximum allowed by the ESP8266 in a single transmission),
 * thus data can only be sixteen uint32_t's long
 */
static void IRAM_ATTR spi_master_transmit(enum spi_master_mode direction, uint32_t *data);
/*
 * Send the length of the subsequent transmission
 * I don't know how this is useful since spi_master_transmit() assumes a 64-byte length every time
 */
static void IRAM_ATTR spi_master_send_length(uint32_t length);
/*
 * Deals with situations when the handshake pin is raised
 */
static void IRAM_ATTR gpio_isr_handler(void *arg);
/*
 * A debug function for printing the contents of a pattern data struct
 */
//static void print_pattern_data(struct xHwStaticData *pattern_data);

static void IRAM_ATTR spi_master_transmit(enum spi_master_mode direction, uint32_t *data) {
  spi_trans_t transmission;
  uint16_t cmd;
  uint32_t addr = 0x00; // Zero because this is a data command, not a status command
  memset(&transmission, 0x00, sizeof(transmission)); // Make sure transmission is clear
  transmission.bits.val = 0;
  switch (direction) {
    case SPI_WRITE:
      cmd = SPI_MASTER_WRITE_DATA_TO_SLAVE_CMD;
      transmission.bits.mosi = 8 * HW_DATA_CHUNK_SIZE; // 8-bit by 64-byte
      transmission.mosi = data; // Not &data because data is already a pointer
      break;
    case SPI_READ:
      cmd = SPI_MASTER_READ_DATA_FROM_SLAVE_CMD;
      transmission.bits.miso = 8 * HW_DATA_CHUNK_SIZE;
      transmission.miso = data;
      break;
    default:
      break;
  }
  transmission.bits.cmd = 8 * 1; // Single byte command
  transmission.bits.addr = 8 * 1;
  transmission.cmd = &cmd; // spi_trans_t expects a pointer here
  transmission.addr = &addr;
  spi_trans(HSPI_HOST, &transmission);
}
static void IRAM_ATTR spi_master_send_length(uint32_t length) {
  spi_trans_t transmission;
  uint16_t cmd = SPI_MASTER_WRITE_STATUS_TO_SLAVE_CMD;
  memset(&transmission, 0x00, sizeof(transmission));
  transmission.bits.val = 0;
  transmission.bits.cmd = 8 * 1;
  transmission.bits.addr = 0; // Zero because this is a status command
  transmission.bits.mosi = 8 * 4; // The status registers are 32-bit
  transmission.cmd = &cmd;
  transmission.addr = NULL; // NULL because again status commands don't use the address
  transmission.mosi = &length;
  spi_trans(HSPI_HOST, &transmission);
}
static void IRAM_ATTR gpio_isr_handler(void *arg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(xSpiWriteTaskHandle, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}
//static void print_pattern_data(struct xHwStaticData *pattern_data) {
//  ESP_LOGI(__ESP_FILE__, "val: %"PRIu32, pattern_data->val);
//  ESP_LOGI(__ESP_FILE__, "delay: %"PRIu32, pattern_data->delay);
//  ESP_LOGI(__ESP_FILE__, "color: %"PRIu32, pattern_data->color);
//}
void vHwSetupSpi(void) {
  gpio_config_t io_config = HW_GPIO_CONFIG_DEFAULT(CONFIG_HW_SPI_HANDSHAKE);
  gpio_config(&io_config);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(CONFIG_HW_SPI_HANDSHAKE, gpio_isr_handler, NULL);
  spi_config_t spi_config = HW_SPI_CONFIG_DEFAULT(CONFIG_HW_SPI_CLK_DIV);
  spi_init(HSPI_HOST, &spi_config);
}
void IRAM_ATTR vHwSpiMasterWriteTask(void *arg) {
  // Divide by four because a uint32_t is 4-bytes, and 64-bytes is the max supported by ESP8266 SPI
  static uint32_t pulDataChunk[HW_DATA_CHUNK_SIZE / 4];
  memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
  static struct xHwMessage *pxMessageBuffer;
  const uint32_t ulDataChunkCountMetadata = HW_DATA_CHUNK_COUNT(sizeof(struct xHwMessageMetadata));
  for (;;) {
    xEventGroupWaitBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_START, pdTRUE, pdTRUE, portMAX_DELAY);
//    ESP_LOGI(__ESP_FILE__, "Task resume");
    while (uxQueueMessagesWaiting(xHttpToSpiQueueHandle) > 0) {
      memset(&pxMessageBuffer, 0x00, sizeof(struct xHwMessage *));
      xQueueReceive(xHttpToSpiQueueHandle, &pxMessageBuffer, portMAX_DELAY);
      if (pxMessageBuffer->pxMetadata->xType == STATIC_PATTERN_DATA) {
        #if CONFIG_HW_ENABLE_STATIC_PATTERN & 0
          // Classic way of transferring data, uncompressed with only one static pattern per transmission
          ulTaskNotifyTake(pdTRUE, 0);
          // Below: number of chunks to send, in case the relevant struct is larger than 64-bytes
          const uint32_t data_chunk_count_pattern = HW_DATA_CHUNK_COUNT(sizeof(struct xHwStaticData));
          spi_master_send_length((ulDataChunkCountMetadata + data_chunk_count_pattern) * sizeof(pulDataChunk));
          ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
          // Send pxMetadata, letting the NeoPixel device what else it's about to receive
          for (uint32_t data_index_metadata = 0; data_index_metadata < ulDataChunkCountMetadata;
            ++data_index_metadata) {
            // Explanation of all this is further down, as it's almost identical
            memcpy(pulDataChunk, (void *) (pxMessageBuffer->pxMetadata + (data_index_metadata * sizeof(pulDataChunk))),
              (size_t) fminf(sizeof(pulDataChunk), sizeof(struct xHwMessageMetadata)) -
              (data_index_metadata * sizeof(pulDataChunk)));
            spi_master_transmit(SPI_WRITE, pulDataChunk);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
          }
          // Send the actual pattern data
          for (uint32_t data_index_pattern = 0; data_index_pattern < data_chunk_count_pattern; ++data_index_pattern) {
            // Copy the minimum necessary data to pulDataChunk using fminf(),
            // important both when the pattern data struct is greater than or less than 64-bytes
            memcpy(pulDataChunk, (void *) (pxMessageBuffer->pattern_data + (data_index_pattern * sizeof(pulDataChunk))),
              (size_t) fminf(sizeof(pulDataChunk), sizeof(struct xHwStaticData)) -
              (data_index_pattern * sizeof(pulDataChunk)));
            spi_master_transmit(SPI_WRITE, pulDataChunk); // Write the actual data in small snippets
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            memset(pulDataChunk, 0x00, sizeof(pulDataChunk)); // Clear pulDataChunk
          }
          spi_master_send_length((uint32_t) 0); // Clear the status register on the other device
          print_pattern_data(pxMessageBuffer->pattern_data);
          xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_END);
          ESP_LOGI(__ESP_FILE__, "End transmission");
          xEventGroupWaitBits(xHttpAndSpiEventGroupHandle, HW_BIT_HTTP_MSG_MEM_FREE, pdTRUE, pdTRUE, portMAX_DELAY);
        #endif
        #if CONFIG_HW_ENABLE_STATIC_PATTERN & 0
          // Compressed transfer, hopefully more efficient
          ulTaskNotifyTake(pdTRUE, 0);
          spi_master_send_length(HW_DATA_CHUNK_COUNT((sizeof(struct xHwStaticData)) +
            (pxMessageBuffer->pattern_data_array_length * sizeof(struct xHwStaticData))) * sizeof(pulDataChunk));
          ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
          uint32_t data_chunk_position = 0;
          // Send pxMetadata or otherwise transfer it into pulDataChunk
          for (uint32_t data_chunk_index = 0; data_chunk_index < ulDataChunkCountMetadata; ++data_chunk_index) {
            // Get the minimum possible amount of data to transmit
            uint32_t data_transmission_amount = (uint32_t) fminf(sizeof(pulDataChunk),
              (sizeof(struct xHwMessageMetadata)) - (data_chunk_index * sizeof(pulDataChunk)));
            // Copy the struct into pulDataChunk, using data_chunk_index as an offset on pxMetadata
            memcpy(pulDataChunk, (void *) ((pxMessageBuffer->pxMetadata) + (data_chunk_index * sizeof(pulDataChunk))),
              data_transmission_amount);
            data_chunk_position += data_transmission_amount;
            // If pulDataChunk is full, send it and subsequently reset the position,
            // otherwise continue onto the next the loop
            if (data_transmission_amount == sizeof(pulDataChunk)) {
              spi_master_transmit(SPI_WRITE, pulDataChunk);
              ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
              memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
              data_chunk_position = 0;
            }
          }
          // Send pattern data, pretty much the exact same thing
          const uint32_t data_chunk_count_pattern_array =
            HW_DATA_CHUNK_COUNT(pxMessageBuffer->pattern_data_array_length * sizeof(struct xHwStaticData));
          for (uint32_t data_chunk_index = 0; data_chunk_index < data_chunk_count_pattern_array; ++data_chunk_index) {
            uint32_t data_transmission_amount = (uint32_t) fminf(sizeof(pulDataChunk),
              (pxMessageBuffer->pattern_data_array_length * sizeof(struct xHwStaticData)) - (data_chunk_index *
               sizeof(pulDataChunk)));
            memcpy(pulDataChunk, (void *) ((pxMessageBuffer->pattern_data_array) +
              (data_chunk_index * sizeof(pulDataChunk))), data_transmission_amount);
            data_chunk_position += data_transmission_amount;
            if (data_transmission_amount == sizeof(pulDataChunk)) {
              spi_master_transmit(SPI_WRITE, pulDataChunk);
              ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
              memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
              data_chunk_position = 0;
            }
          }
          // New idea: iterate over the array by each element. Will need a sub-loop within it for it to work,
          // utilizing another position variable sized to sizeof(pattern_data)
          for (uint32_t pda_index = 0; pda_index < pxMessageBuffer->pattern_data_array_length; ++pda_index) {

          }
          // In case there's a remaining chunk that isn't full, send it
          if (data_chunk_position > 0) {
            spi_master_transmit(SPI_WRITE, pulDataChunk);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
            data_chunk_position = 0;
          }
          spi_master_send_length((uint32_t) 0);
          for (uint32_t pda_index = 0; pda_index < pxMessageBuffer->pattern_data_array_length; ++pda_index) {
            print_pattern_data(pxMessageBuffer->pattern_data_array[pda_index]);
          }
          xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_END);
          ESP_LOGI(__ESP_FILE__, "End transmission");
          xEventGroupWaitBits(xHttpAndSpiEventGroupHandle, HW_BIT_HTTP_MSG_MEM_FREE, pdTRUE, pdTRUE, portMAX_DELAY);
        #endif
      } else if (pxMessageBuffer->pxMetadata->xType == DYNAMIC_PATTERN_DATA) {
        #if CONFIG_HW_ENABLE_DYNAMIC_PATTERN
          ulTaskNotifyTake(pdTRUE, 0);
          uint32_t ulInitialStreamBufferBytes =
            xStreamBufferBytesAvailable(*pxMessageBuffer->pxStreamBufferHandle);
//          ESP_LOGI(__ESP_FILE__, "ulInitialStreamBufferBytes %u", ulInitialStreamBufferBytes);
          uint32_t ulSpiSendLength = HW_DATA_CHUNK_COUNT(sizeof(struct xHwMessageMetadata) +
            ulInitialStreamBufferBytes) * sizeof(pulDataChunk);
//          ESP_LOGI(__ESP_FILE__, "ulSpiSendLength %u", ulSpiSendLength);
          spi_master_send_length(ulSpiSendLength);
//          ESP_LOGI(__ESP_FILE__, "send length %u", HW_DATA_CHUNK_COUNT(sizeof(struct xHwStaticData) +
//            ulInitialStreamBufferBytes) * sizeof(pulDataChunk));
          ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
          // Hold the current index within pulDataChunk of where the data stops; rolls over to zero every 64-bytes
//          uint32_t ulDataChunkPosition = 0;
          struct {
            uint32_t bValue: 6;
            uint32_t reserved: 26;
          } ulDataChunkPosition;
          ulDataChunkPosition.bValue = 0;
//          uint32_t ulTransmitCounter = 0;
          // Load pxMetadata into pulDataChunk, send it if need be and continue loading
          for (uint32_t ulDataChunkIndex = 0; ulDataChunkIndex < ulDataChunkCountMetadata; ++ulDataChunkIndex) {
            // Get the minimum possible amount of data to transmit
            uint32_t ulDataTransmitAmount = (uint32_t) fminf(sizeof(pulDataChunk),
              sizeof(struct xHwMessageMetadata) - (ulDataChunkIndex * sizeof(pulDataChunk)));
//            ESP_LOGI(__ESP_FILE__, "ulDataTransmitAmount in metadata %u", ulDataTransmitAmount);
            // Copy the metadata to pulDataChunk, shifting over by 64-bytes every loop if necessary
            memcpy(pulDataChunk, (void *) (pxMessageBuffer->pxMetadata) + (ulDataChunkIndex * sizeof(pulDataChunk)),
              ulDataTransmitAmount);
            ulDataChunkPosition.bValue += ulDataTransmitAmount;
//            ESP_LOGI(__ESP_FILE__, "ulDataChunkPosition in metadata %u", ulDataChunkPosition.bValue);
//            if (ulDataTransmitAmount == sizeof(pulDataChunk)) {
            if (ulDataChunkPosition.bValue == 0) {
              spi_master_transmit(SPI_WRITE, pulDataChunk);
              ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
//              ulTransmitCounter += 64;
//              ESP_LOGI(__ESP_FILE__, "ulTransmitCounter %u", ulTransmitCounter);
//              memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
//              ulDataChunkPosition.bValue = 0;
            }
          }
//          struct xHwDynamicData xDynamicData;
          uint32_t ulDataChunkCountStreamBuffer = HW_DATA_CHUNK_COUNT(ulInitialStreamBufferBytes);
//          uint32_t ulDynamicDataIndex = ulInitialStreamBufferBytes / sizeof(struct xHwDynamicData);
          uint32_t ulRemainingStreamBufferBytes = ulInitialStreamBufferBytes;
          // Send the dynamic pattern data itself
          for (uint32_t ulDataChunkIndex = 0; ulDataChunkIndex < ulDataChunkCountStreamBuffer + 1; ++ulDataChunkIndex) {
            // Minimum data to transmit; will max out at sizeof(pulDataChunk)
            uint32_t ulDataTransmitAmount = (uint32_t) fminf(sizeof(pulDataChunk) - ulDataChunkPosition.bValue,
              ulRemainingStreamBufferBytes);
//            ESP_LOGI(__ESP_FILE__, "ulDataTransmitAmount in dynamic %u", ulDataTransmitAmount);
            // Fill pulDataChunk
            xStreamBufferReceive(*pxMessageBuffer->pxStreamBufferHandle, (void *) (pulDataChunk) +
              ulDataChunkPosition.bValue, ulDataTransmitAmount, portMAX_DELAY);
//            ulDataChunkPosition += (ulDataTransmitAmount - ulDataChunkPosition);
//            memcpy(&xDynamicData, (void *) (pulDataChunk) + ulDataChunkPosition.bValue,
//              sizeof(struct xHwDynamicData));
//            ESP_LOGI(__ESP_FILE__, "pixel %u, red %u, green %u, blue%u", xDynamicData.ulPixelIndex,
//              xDynamicData.ulColor >> 16 & 0xFF, xDynamicData.ulColor >>  8 & 0xFF, xDynamicData.ulColor & 0xFF);
            ulDataChunkPosition.bValue += ulDataTransmitAmount;
//            ESP_LOGI(__ESP_FILE__, "ulDataChunkPosition in dynamic: %u", ulDataChunkPosition.bValue);
            ulRemainingStreamBufferBytes -= ulDataTransmitAmount;
//            ESP_LOGI(__ESP_FILE__, "bytes avail in dynamic %u", ulRemainingStreamBufferBytes);
            // If pulDataChunk is all the way full, send it
//            if (ulDataTransmitAmount == sizeof(pulDataChunk)) {
            if (ulDataChunkPosition.bValue == 0) {
              spi_master_transmit(SPI_WRITE, pulDataChunk);
              ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
//              ulTransmitCounter += 64;
//              ESP_LOGI(__ESP_FILE__, "ulTransmitCounter %u", ulTransmitCounter);
//              memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
//              ulDataChunkPosition.bValue = 0;
//              ulRemainingStreamBufferBytes = xStreamBufferBytesAvailable(*pxMessageBuffer->pxStreamBufferHandle);
            }
          }
//          ESP_LOGI(__ESP_FILE__, "ulDataChunkPosition in straggler %u", ulDataChunkPosition.bValue);
          // Send the straggler data chunk, if it exists
          if (ulDataChunkPosition.bValue > 0) {
            memset((void *) (pulDataChunk) + ulDataChunkPosition.bValue, 0xFF, sizeof(pulDataChunk) -
              ulDataChunkPosition.bValue);
//            ESP_LOGI(__ESP_FILE__, "0x%X", pulDataChunk[15]);
            spi_master_transmit(SPI_WRITE, pulDataChunk);
//            ulRemainingStreamBufferBytes = xStreamBufferBytesAvailable(*pxMessageBuffer->pxStreamBufferHandle);
//              ESP_LOGI(__ESP_FILE__, "bytes avail in straggler %u", ulRemainingStreamBufferBytes);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
//            ulTransmitCounter += 64;
//            ESP_LOGI(__ESP_FILE__, "ulTransmitCounter %u", ulTransmitCounter);
          }
          // End-of-transmission cleanup
          spi_master_send_length((uint32_t) 0);
          xEventGroupSetBits(xHttpAndSpiEventGroupHandle, HW_BIT_SPI_TRANS_END);
//          ESP_LOGI(__ESP_FILE__, "End transmission");
          xEventGroupWaitBits(xHttpAndSpiEventGroupHandle, HW_BIT_HTTP_MSG_MEM_FREE, pdTRUE, pdTRUE, portMAX_DELAY);
//          memset(pulDataChunk, 0x00, sizeof(pulDataChunk));
        #endif
      } else {
        // Something has gone wrong, but I don't know how to fix it at the moment
      }
    } // while loop
  }
}

#ifdef __cplusplus
}
#endif
