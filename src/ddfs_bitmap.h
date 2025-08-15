#ifndef ddfs_BITMAP_H
#define	ddfs_BITMAP_H

#include "ddfs.h"

extern int8_t set_bit(int fd, uint64_t bit);

extern int8_t clear_bit(int fd, uint64_t bit);

extern int8_t get_bit(int fd, uint64_t bit);

#endif
