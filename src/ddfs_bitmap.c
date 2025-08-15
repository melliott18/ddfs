#include "ddfs_bitmap.h"

int8_t set_bit(int fd, uint64_t bit) {
    uint64_t byte_number = bit / 8;
    uint32_t block_number = byte_number / DDFS_BLOCK_SIZE;
    uint64_t bit_index = bit % DDFS_BLOCK_SIZE;
    uint32_t byte_index = byte_number % DDFS_BLOCK_SIZE;
    char *buffer = malloc(DDFS_BLOCK_SIZE);

    if (!buffer) {
        return EXIT_FAILURE;
    }

    memset(buffer, 0, DDFS_BLOCK_SIZE);
    int ret = read_block(fd, buffer, block_number);

    if (ret != DDFS_BLOCK_SIZE) {
        free(buffer);
        return EXIT_FAILURE;
    }

    // Set the bit at bit_index in the buffer
    buffer[byte_index] |= (1 << (bit_index % 8));

    ret = write_block(fd, buffer, block_number);

    if (ret != DDFS_BLOCK_SIZE) {
        free(buffer);
        return EXIT_FAILURE;
    }

    free(buffer);
    return EXIT_SUCCESS;
}

int8_t clear_bit(int fd, uint64_t bit) {
    uint64_t byte_number = bit / 8;
    uint32_t block_number = byte_number / DDFS_BLOCK_SIZE;
    uint64_t bit_index = bit % DDFS_BLOCK_SIZE;
    uint32_t byte_index = byte_number % DDFS_BLOCK_SIZE;
    char *buffer = malloc(DDFS_BLOCK_SIZE);

    if (!buffer) {
        return EXIT_FAILURE;
    }

    memset(buffer, 0, DDFS_BLOCK_SIZE);
    int ret = read_block(fd, buffer, block_number);

    if (ret != DDFS_BLOCK_SIZE) {
        free(buffer);
        return EXIT_FAILURE;
    }

    // Clear the bit at bit_index in the buffer
    buffer[byte_index] &= ~(1 << (bit_index % 8));

    ret = write_block(fd, buffer, block_number);

    if (ret != DDFS_BLOCK_SIZE) {
        free(buffer);
        return EXIT_FAILURE;
    }

    free(buffer);
    return EXIT_SUCCESS;
}

int8_t get_bit(int fd, uint64_t bit) {
    uint64_t byte_number = bit / 8;
    uint32_t block_number = byte_number / DDFS_BLOCK_SIZE;
    uint64_t bit_index = bit % DDFS_BLOCK_SIZE;
    uint32_t byte_index = byte_number % DDFS_BLOCK_SIZE;
    char *buffer = malloc(DDFS_BLOCK_SIZE);

    if (!buffer) {
        return -1;
    }

    memset(buffer, 0, DDFS_BLOCK_SIZE);
    int ret = read_block(fd, buffer, block_number);

    if (ret != DDFS_BLOCK_SIZE) {
        free(buffer);
        return -1;
    }

    // Get the bit at bit_index in the buffer
    int value = 1 & (buffer[byte_index] >> (bit_index % 8));

    if (ret != DDFS_BLOCK_SIZE) {
        free(buffer);
        return -1;
    }

    free(buffer);
    return value;
}
