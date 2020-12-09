#ifndef HW_DEF_H
#define HW_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

#include "esp8266/eagle_soc.h"

/*
 * Max size of a single SPI transfer on the ESP8266
 */
#define HW_DATA_CHUNK_SIZE (64)
/*
 * Useful for getting the minimum number of data chunks needed to transmit some stuff
 * Primarily used with sizeof() for structs and xStreamBufferBytesAvailable() for the dynamic data stream buffer
 */
#define HW_DATA_CHUNK_COUNT(x) (((x) + (HW_DATA_CHUNK_SIZE - 1)) / HW_DATA_CHUNK_SIZE)
/*
 * Bits used by the HTTP and SPI event group
 */
#define HW_BIT_SPI_TRANS_END BIT(0) // SPI transfer is complete; safe to free message data memory
#define HW_BIT_HTTP_MSG_MEM_FREE BIT(1) // Message data memory from the HTTP task has been freed
#define HW_BIT_SPI_TRANS_START BIT(2) // Ready to start SPI transfer
#define HW_BIT_SPI_TRANS_CONTINUE BIT(3) // Intermediate block in SPI transfer, after waiting for the GPIO interrupt
/*
 * Type of message in the queue
 */
enum xHwMessageType {
  NO_TYPE = 0, // Unused at present
  STATIC_PATTERN_DATA, // Used in conjunction with struct xHwStaticData
  DYNAMIC_PATTERN_DATA // Reads commands from a stream buffer and sends them to the NeoPixel device
};
/*
 * Patterns understood by the NeoPixel device via SPI
 * These are selected on the HTML document
 * Most of the implementations on the NeoPixel device are stolen from the Adafruit_NeoPixel examples
 */
enum xHwPattern {
  NO_PATTERN = 0, // Used to turn off the strip
  FILL_COLOR,
  THEATRE_CHASE,
  RAINBOW_SOLID, // Strip changes color uniformly
  RAINBOW_WAVE // Strip changes color in a shifting gradient
};
/*
 * Contains information about the message, such as type and whatever
 * Useful for transmitting to the NeoPixel device more than anything
 */
struct xHwMessageMetadata {
  char pcName[16]; // Why this is important is beyond me, but I'm leaving it for now
  enum xHwMessageType xType;
};
/*
 * A struct for assigning a pattern to a segment of the NeoPixel strip
 * Note that it's possible to have different sections of the strip have different patterns,
 * so long as you send multiple queue messages in a row
 */
struct xHwStaticData {
  enum xHwPattern xPattern;
  uint16_t usPixelIndexStart; // Inclusive, since it's zero-indexed
  uint16_t usPixelIndexEnd; // Also inclusive for the same reason
  uint32_t ulDelay; // In milliseconds; used to control the speed of effects
  uint32_t ulColor; // Mostly used for fill color and whatnot
};
/*
 * Packed structure for transferring dynamic pattern data
 */
struct xHwDynamicData {
  uint32_t ulPixelIndex;
  uint32_t ulColor; // This could've been 24-bit but then that would've negated RGBW support
};
/*
 * The message for communicating between the HTTP server and the SPI master task
 */
struct xHwMessage {
  struct xHwMessageMetadata *pxMetadata;
  union {
//    struct xHwStaticData *pattern_data;
//    struct {
//      struct xHwStaticData **pattern_data_array;
//      uint32_t pattern_data_array_length;
//    };
    StreamBufferHandle_t *pxStreamBufferHandle;
  };
};

#ifdef __cplusplus
}
#endif

#endif // HW_DEF_H
