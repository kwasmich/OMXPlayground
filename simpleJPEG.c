#include "simpleJPEG.h"

#include <stdio.h>

#include <jpeglib.h> // lacks header completeness

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>



bool jpegIsJPEG(const uint8_t * const in_JPEG_DATA) {
    const uint8_t magic[3] = { 0xFF, 0xD8, 0xFF };
    int result = memcmp(in_JPEG_DATA, magic, 3);
    return result == 0;
}



bool jpegDecode(uint8_t **out_image, uint32_t *out_width, uint32_t *out_height, uint32_t *out_numChannels, const uint8_t * const in_JPEG_DATA, const size_t in_JPEG_SIZE, const bool in_FLIP_Y) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, in_JPEG_DATA, in_JPEG_SIZE);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    JDIMENSION w = cinfo.image_width;
    JDIMENSION h = cinfo.image_height;
    uint32_t c = cinfo.num_components;
    uint8_t *image = (uint8_t*)malloc(w * h * c * sizeof(uint8_t));
    uint8_t *imagePtr = image;

    if (in_FLIP_Y) {
        imagePtr = image + w * (h - 1) * c;
    }

    size_t row_stride = cinfo.output_width * cinfo.output_components;

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &imagePtr, 1);

        if (in_FLIP_Y) {
            imagePtr -= row_stride;
        } else {
            imagePtr += row_stride;
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    *out_image = image;
    *out_width = w;
    *out_height = h;
    *out_numChannels = c;

    return true;
}



bool jpegEncode(uint8_t ** const out_jpegData, size_t * out_jpegSize, uint8_t * const in_IMAGE, const uint32_t in_WIDTH, const uint32_t in_HEIGHT, const uint32_t in_NUM_CHANNELS, const uint32_t in_QUALITY) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    unsigned long outsize = 0;
    uint8_t *outbuffer = NULL;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &outbuffer, &outsize);
    cinfo.image_width = in_WIDTH;
    cinfo.image_height = in_HEIGHT;

    switch (in_NUM_CHANNELS) {
            case 1:
            cinfo.input_components = 1;
            cinfo.in_color_space = JCS_GRAYSCALE;
            break;

            case 3:
            cinfo.input_components = 3;
            cinfo.in_color_space = JCS_RGB;
            break;

        default:
            fprintf(stderr, "unsupported colorspace\n");
            return false;
    }

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, in_QUALITY, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    size_t row_stride = in_WIDTH * 3;

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &in_IMAGE[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    *out_jpegData = outbuffer;
    *out_jpegSize = outsize;
    return true;
}



bool jpegRead(uint8_t ** const out_image, uint32_t * const out_width, uint32_t * const out_height, uint32_t * const out_numChannels, const char * const in_FILE_PATH, const bool in_FLIP_Y) {
    FILE * fp = fopen(in_FILE_PATH, "rb");

    if (fp == NULL) {
        fprintf(stderr, "Cannot open file \"%s\"\n", in_FILE_PATH);
        return false;
    }

    struct stat st;
    int fd = fileno(fp);
    int ret = fstat(fd, &st);

    if (ret != 0) {
        fprintf(stderr, "Cannot get file stats for \"%s\"\n", in_FILE_PATH);
        fclose(fp);
        return false;
    }

    size_t len = st.st_size;

    if (len == 0) {
        fprintf(stderr, "File \"%s\" is empty\n", in_FILE_PATH);
        fclose(fp);
        return false;
    }

    uint8_t *rawData = (uint8_t *)malloc(len);
    size_t bytes_read = fread(rawData, sizeof(uint8_t), len, fp);

    if (bytes_read != len) {
        fprintf(stderr, "Cannot read file \"%s\"\n", in_FILE_PATH);
        free(rawData);
        fclose(fp);
        return false;
    }

    fclose(fp);
    bool success = jpegDecode(out_image, out_width, out_height, out_numChannels, rawData, len, in_FLIP_Y);
    free(rawData);
    return success;
}



bool jpegWrite(const char * const in_FILE_PATH, uint8_t * const in_IMAGE, const uint32_t in_WIDTH, const uint32_t in_HEIGHT, const uint32_t in_NUM_CHANNELS, const uint32_t in_QUALITY) {
    uint8_t *data;
    size_t size;
    bool success = jpegEncode(&data, &size, in_IMAGE, in_WIDTH, in_HEIGHT, in_NUM_CHANNELS, in_QUALITY);

    if (success) {
        FILE *fp = fopen(in_FILE_PATH, "wb");

        if (fp == NULL) {
            fprintf(stderr, "Cannot open file \"%s\"\n", in_FILE_PATH);
            jpegFree(&data);
            return false;
        }

        size_t bytes_written = fwrite(data, sizeof(uint8_t), size, fp);

        if (bytes_written != size) {
            fprintf(stderr, "Cannot write file \"%s\"\n", in_FILE_PATH);
            fclose(fp);
            jpegFree(&data);
            return false;
        }

        fclose(fp);
    }
    
    return success;
}



void jpegFree(uint8_t ** const in_out_image) {
    free(*in_out_image);
    *in_out_image = NULL;
}
