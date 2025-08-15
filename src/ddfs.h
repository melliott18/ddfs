#ifndef ddfs_H
#define	ddfs_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/condvar.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/tree.h>
#include <sys/vnode.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/disk.h>
#include <sys/endian.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DDFS_BLOCK_SIZE 4096
#define DDFS_MAGIC_NUM 0xBA5ED

struct ddfs_sb_info {
    uint32_t fs_magic_num; // Magic number
    uint64_t fs_media_size; // Total number of bytes
    uint32_t fs_block_size; // Block size
    uint32_t fs_block_count; // Total number of blocks
    uint32_t fs_ifree_block_count; // Number of free inode bitmap blocks
    uint32_t fs_bfree_block_count; // Number of free block bitmap blocks
    uint32_t fs_istore_block_count; // Number of inode store blocks
    uint32_t fs_data_block_count; // Number of data blocks
    uint32_t fs_inode_size; // Size of an inode
    uint32_t fs_inode_count; // Total number of inodes
    uint32_t fs_ifree_count; // Free inode count
    uint32_t fs_bfree_count; // Free block count
    uint32_t fs_istore_offset; // Inode store offset
    uint32_t fs_data_offset; // Data offset
    uint32_t fs_uid; // Filesystem uid
    char fs_name[12]; // Filesystem name
    char fs_volume_name[12]; // Volume name
};

struct ddfs_superblock {
    struct ddfs_sb_info info; // Superblock information
    char padding[4000]; // Padding to match block size
};

extern inline uint32_t div_ceil(uint32_t a, uint32_t b);

extern int64_t get_disk_media_size(int fd);

extern int64_t get_disk_block_size(int fd);

extern int64_t get_disk_sector_size(int fd);

extern int read_block(int fd, void *buffer, uint32_t block_number);

extern int write_block(int fd, void *buffer, uint32_t block_number);

extern struct ddfs_superblock *read_superblock(int fd);

extern struct ddfs_superblock *write_superblock(int fd);

extern int erase_disk(int fd);

extern int erase_superblock(int fd);

extern int erase_ifree_blocks(int fd);

extern int erase_bfree_blocks(int fd);

extern int erase_inode_store(int fd);

extern int64_t get_next_free_block(int fd);

extern int set_block_bit(int fd, uint32_t block_number);

extern int clear_block_bit(int fd, uint32_t block_number);

extern int get_block_bit(int fd, uint32_t block_number);

extern int initialize_ddfs(int fd);

extern void file_name_to_key(char *file_name, uint8_t key[20]);

extern uint64_t key_hash(uint8_t key[20], uint64_t size);

extern void hash_block(uint8_t block[DDFS_BLOCK_SIZE], 
    uint8_t **result);

extern void shift_bits_right(uint8_t *key, uint8_t len, uint32_t shift);

extern int create_kv_pair(int fd, uint8_t key[20], uint8_t *value);

extern int delete_kv_pair(int fd, uint8_t key[20]);

extern int get_value(int fd, uint8_t key[20], uint8_t *value);

extern int rename_key(int fd, uint8_t old_key[20], uint8_t new_key[20]);

extern int modify_value(int fd, uint8_t key[20], uint8_t *value);

extern int block_exists(int fd, uint8_t *value);

#endif
