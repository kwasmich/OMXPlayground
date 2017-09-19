#ifndef simpleJPEG_h
#define simpleJPEG_h


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


bool jpegIsJPEG(const uint8_t * const in_JPEG_DATA);
bool jpegDecode(uint8_t **out_image, uint32_t *out_width, uint32_t *out_height, uint32_t *out_numChannels, const uint8_t * const in_JPEG_DATA, const size_t in_JPEG_SIZE, const bool in_FLIP_Y);
bool jpegEncode(uint8_t ** const out_jpegData, size_t * out_jpegSize, uint8_t * const in_IMAGE, const uint32_t in_WIDTH, const uint32_t in_HEIGHT, const uint32_t in_NUM_CHANNELS, const uint32_t in_QUALITY);
bool jpegIsJPEGFile(const char * const in_FILE_PATH);
bool jpegRead(uint8_t ** const out_image, uint32_t * const out_width, uint32_t * const out_height, uint32_t * const out_numChannels, const char * const in_FILE_PATH, const bool in_FLIP_Y);
bool jpegWrite(const char * const in_FILE_PATH, uint8_t * const in_IMAGE, const uint32_t in_WIDTH, const uint32_t in_HEIGHT, const uint32_t in_NUM_CHANNELS, const uint32_t in_QUALITY);
void jpegFree(uint8_t ** const in_out_image);


#endif /* simpleJPEG_h */
