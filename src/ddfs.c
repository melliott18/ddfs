#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/refcount.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/syscallsubr.h>
#include <sys/tree.h>
#include <sys/vnode.h>
#include <machine/atomic.h>
#include <vm/uma.h>

#include "ddfs.h"
#include "ddfs_bitmap.h"
#include "ddfs_inode.h"

inline uint32_t div_ceil(uint32_t a, uint32_t b) {
    uint32_t ret = a / b;
    
    if (a % b) {
        return ret + 1;
    }

    return ret;
}

int64_t get_disk_media_size(int fd) {
    // Get media size in bytes
    uint64_t media_size = 0;
    int ret = ioctl(fd, DIOCGMEDIASIZE, &media_size);

    if (ret == -1) {
        perror("ioctl");
        close(fd);
        return ret;
    }

    return media_size;
}

int64_t get_disk_block_size(int fd) {
    // Get block size in bytes
    struct stat stat_buf;
    int ret = fstat(fd, &stat_buf);

    if (ret) {
        perror("fstat");
        close(fd);
        return EXIT_FAILURE;
    }

    return stat_buf.st_blksize;
}

int64_t get_disk_sector_size(int fd) {
    // Get sector size in bytes
    long int sector_size = 0;
    int ret = ioctl(fd, DIOCGSECTORSIZE, &sector_size);

    if (ret == -1) {
        perror("ioctl");
        close(fd);
        return ret;
    }

    return sector_size;
}

int read_block(int fd, void *buffer, uint32_t block_number) {
    lseek(fd, block_number * DDFS_BLOCK_SIZE, SEEK_SET);
    int ret = read(fd, buffer, DDFS_BLOCK_SIZE);
    lseek(fd, 0, SEEK_SET);
    return ret;
}

int write_block(int fd, void *buffer, uint32_t block_number) {
    lseek(fd, block_number * DDFS_BLOCK_SIZE, SEEK_SET);
    int ret = write(fd, buffer, DDFS_BLOCK_SIZE);
    lseek(fd, 0, SEEK_SET);
    return ret;
}

struct ddfs_superblock *read_superblock(int fd) {
    struct ddfs_superblock *sb = malloc(DDFS_BLOCK_SIZE);
    
    if (sb == NULL) {
        return NULL;
    }

    int ret = read_block(fd, sb, 0);
    
    if (ret != DDFS_BLOCK_SIZE) {
        free(sb);
        return NULL;
    }

    return sb;
}

struct ddfs_superblock *write_superblock(int fd) {
    struct ddfs_superblock *sb = malloc(DDFS_BLOCK_SIZE);
    
    if (sb == NULL) {
        return NULL;
    }

    memset(sb, 0, DDFS_BLOCK_SIZE);

    uint64_t media_size = get_disk_media_size(fd);
    uint32_t block_count = media_size / DDFS_BLOCK_SIZE;
    uint32_t inode_count = block_count;
    uint32_t inode_size = sizeof(struct ddfs_inode);
    uint32_t inodes_per_block = DDFS_BLOCK_SIZE / inode_size;
    uint32_t ifree_block_count = div_ceil(inode_count, DDFS_BLOCK_SIZE * 8);
    uint32_t bfree_block_count = div_ceil(block_count, DDFS_BLOCK_SIZE * 8);
    uint32_t istore_block_count = div_ceil(inode_count, inodes_per_block);
    uint32_t data_block_count = block_count - ifree_block_count
        - bfree_block_count - istore_block_count - 1;
    uint32_t istore_offset = DDFS_BLOCK_SIZE
        * (ifree_block_count + bfree_block_count + 1);
    uint32_t data_offset = istore_offset + 
        (istore_block_count * DDFS_BLOCK_SIZE);

    sb->info = (struct ddfs_sb_info) {
        .fs_magic_num = htole32(DDFS_MAGIC_NUM),
        .fs_media_size = htole64(media_size),
        .fs_block_size = htole32(DDFS_BLOCK_SIZE),
        .fs_block_count = htole32(block_count),
        .fs_ifree_block_count = htole32(ifree_block_count),
        .fs_bfree_block_count = htole32(bfree_block_count),
        .fs_istore_block_count = htole32(istore_block_count),
        .fs_data_block_count = htole32(data_block_count),
        .fs_ifree_count = htole32(inode_count - 1),
        .fs_bfree_count = htole32(block_count - 1),
        .fs_inode_size = htole32(inode_size),
        .fs_inode_count = htole32(inode_count),
        .fs_istore_offset = htole32(istore_offset),
        .fs_data_offset = htole32(data_offset),
        .fs_uid = htole32(getuid())
        //int32_t fs_volume_name; // Volume name
    };

    sb->info.fs_name[0] = 'k';
    sb->info.fs_name[1] = 'v';
    sb->info.fs_name[2] = 'f';
    sb->info.fs_name[3] = 's';
    sb->info.fs_name[4] = '\0';

    int ret = write_block(fd, sb, 0);
    
    if (ret != DDFS_BLOCK_SIZE) {
        free(sb);
        return NULL;
    }

    return sb;
}

int erase_disk(int fd) {
    int64_t media_size = get_disk_media_size(fd);
    uint32_t block_count = div_ceil(media_size, DDFS_BLOCK_SIZE);
    void *buffer = malloc(DDFS_BLOCK_SIZE);
    memset(buffer, 0, DDFS_BLOCK_SIZE);
    int ret;

    for (uint32_t i = 0; i < block_count; i++) {
        ret = write_block(fd, buffer, i);

        if (ret == -1) {
            perror("write");
            close(fd);
            return ret;
        }
    }

    return EXIT_SUCCESS;
}

int erase_superblock(int fd) {
    void *buffer = malloc(DDFS_BLOCK_SIZE);

    if (!buffer) {
        return EXIT_FAILURE;
    }

    memset(buffer, 0, DDFS_BLOCK_SIZE);

    int ret = write_block(fd, buffer, 0);

    free(buffer);
    return ret;
}

// Erase free inode tracker blocks
int erase_ifree_blocks(int fd) {
    void *buffer = malloc(DDFS_BLOCK_SIZE);

    if (!buffer) {
        return EXIT_FAILURE;
    }

    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        free(buffer);
        return EXIT_FAILURE;
    }

    memset(buffer, 0, DDFS_BLOCK_SIZE);

    int ret;

    for (uint32_t i = 1; i < le32toh(sb->info.fs_ifree_block_count) + 1; i++) {
        ret = write_block(fd, buffer, i);
        
        if (ret != DDFS_BLOCK_SIZE) {
            free(buffer);
            return EXIT_FAILURE;
        }
    }

    free(buffer);
    return EXIT_SUCCESS;
}

// Erase free block tracker blocks
int erase_bfree_blocks(int fd) {
    void *buffer = malloc(DDFS_BLOCK_SIZE);

    if (!buffer) {
        return EXIT_FAILURE;
    }

    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        free(buffer);
        return EXIT_FAILURE;
    }

    memset(buffer, 0, DDFS_BLOCK_SIZE);

    int ret;
    uint32_t bfree_block_offset = sb->info.fs_ifree_block_count + 1;
    uint32_t istore_block_offset = sb->info.fs_istore_offset / DDFS_BLOCK_SIZE;

    for (uint32_t i = bfree_block_offset; i < istore_block_offset; i++) {
        ret = write_block(fd, buffer, i);
        
        if (ret != DDFS_BLOCK_SIZE) {
            free(buffer);
            return EXIT_FAILURE;
        }
    }

    free(buffer);
    return EXIT_SUCCESS;
}

int erase_inode_store(int fd) {
    void *buffer = malloc(DDFS_BLOCK_SIZE);

    if (!buffer) {
        return EXIT_FAILURE;
    }

    memset(buffer, 0, DDFS_BLOCK_SIZE);
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        free(buffer);
        return EXIT_FAILURE;
    }

    int ret;
    int istore_block_offset = sb->info.fs_istore_offset / DDFS_BLOCK_SIZE;

    for (uint32_t i = istore_block_offset; 
        i < (le32toh(sb->info.fs_istore_block_count) + 
        istore_block_offset); i++) {
        ret = write_block(fd, buffer, i);
        
        if (ret != DDFS_BLOCK_SIZE) {
            free(buffer);
            return EXIT_FAILURE;
        }
    }

    free(buffer);
    return EXIT_SUCCESS;
}

int64_t get_next_free_block(int fd) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    uint64_t start_bit = ((sb->info.fs_ifree_block_count + 1) * 
        DDFS_BLOCK_SIZE) * 8;
    uint64_t end_bit = (sb->info.fs_istore_offset * 8) - 1;

    for (uint64_t i = start_bit; i <= end_bit; i++) {
        if (get_bit(fd, i) == 0) {
            free(sb);
            return i;
        }
    }

    free(sb);
    return -1;
}

// Indicate that a block has been allocated
int set_block_bit(int fd, uint32_t block_number) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    uint32_t bit_number = (DDFS_BLOCK_SIZE * 8) + block_number;

    free(sb);
    return set_bit(fd, bit_number);
}

// Indicate that a block has been freed
int clear_block_bit(int fd, uint32_t block_number) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    uint32_t bit_number = (DDFS_BLOCK_SIZE * 8) + block_number;

    free(sb);
    return clear_bit(fd, bit_number);
}

// Get the allocation status of a block
int get_block_bit(int fd, uint32_t block_number) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    uint32_t bit_number = (DDFS_BLOCK_SIZE * 8) + block_number;

    free(sb);
    return get_bit(fd, bit_number);
}

int initialize_ddfs(int fd) {
    // Write superblock (block 0)
    struct ddfs_superblock *sb = write_superblock(fd);
    
    if (sb == NULL) {
        perror("write_superblock()");
        free(sb);
        return EXIT_FAILURE;
    }
    
    // Initialize the superblock inode (inode 0)
    int ret = initialize_superblock_inode(fd);

    if (ret) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// Convert a 40 hex character filename to a 160-bit value
void file_name_to_key(char *file_name, uint8_t key[20]) {
    int temp;
    
    for (uint8_t i = 0; i < 20; i++) {
        sscanf(file_name + 2 * i, "%02x", &temp);
        key[i] = (uint8_t)temp;
    }

    return;
}

// Murmurhash
uint64_t key_hash(uint8_t key[20], uint64_t size) {
    uint64_t result = 0;

    for (uint8_t i = 0; i < 20; i++) {
        result ^= key[i];
        result ^= result >> 33;
        result *= 0xff51afd7ed558ccdL;
        result ^= result >> 33;
        result *= 0xc4ceb9fe1a85ec53L;
        result ^= result >> 33;
    }

    return result % size;
}

// Murmurhash
void hash_block(uint8_t block[DDFS_BLOCK_SIZE], uint8_t **result) {
    uint8_t index;
    
    for (uint16_t i = 0; i < DDFS_BLOCK_SIZE; i++) {
        index = i % 20;
        //printf("%d ", index);
        (*result)[index] ^= block[i];
        (*result)[index] ^= (*result)[index] >> 5;
        (*result)[index] *= 0xff51afd7ed558ccdL;
        (*result)[index] ^= (*result)[index] >> 5;
        (*result)[index] *= 0xc4ceb9fe1a85ec53L;
        (*result)[index] ^= (*result)[index] >> 5;
    }

    return;
}

// Shift the bits in a key to the right by a specified amount
void shift_bits_right(uint8_t *key, uint8_t len, uint32_t shift) {
    uint8_t i = 0;
    uint8_t start = shift / 8;
    uint8_t rest = shift % 8;
    uint8_t previous = 0;
    uint8_t value;

    for (i = 0; i < len; i++) {
        if (start <= i) {
            previous = *(key + (i - start));
        }

        value = (previous << (8 - rest)) | *(key + (i + start)) >> rest;
        *(key + (i + start)) = value;
    }

    return;
}

int create_kv_pair(int fd, uint8_t key[20], uint8_t *value) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    uint32_t inode_number = key_hash(key, sb->info.fs_inode_count);
    int64_t inode_bit = get_inode_bit(fd, inode_number);

    if (inode_bit == -1) {
        free(sb);
        return EXIT_FAILURE;
    }

    uint8_t *result = calloc(20, sizeof(uint8_t));
    uint8_t arr[20];
    hash_block(value, &result);

    for (uint16_t c = 0; c < 20; c++) {
        arr[c] = result[c];
    }
    
    uint32_t block_ptr = key_hash(arr, sb->info.fs_block_count);

    if (inode_bit == 0) {
        if (initialize_inode(fd, inode_number, key, block_ptr) == NULL) {
            free(sb);
            return EXIT_FAILURE;
        }
    }

    int ret;

    if (block_exists(fd, value)) {
        ret = increment_reference_count(fd, inode_number);
    } else {
        ret = write_block(fd, value, block_ptr);
    }

    if (ret != DDFS_BLOCK_SIZE) {
        free(sb);
        return EXIT_FAILURE;
    }
    
    free(sb);
    return EXIT_SUCCESS;
}

int delete_kv_pair(int fd, uint8_t key[20]) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    struct ddfs_inode *inode = 
        (struct ddfs_inode*)malloc(sizeof(struct ddfs_inode));

    if (inode == NULL) {
        free(sb);
        return EXIT_FAILURE;
    }

    memset(inode, 0, sizeof(struct ddfs_inode));
    
    uint32_t inode_number = key_hash(key, sb->info.fs_inode_count);
    inode = get_inode(fd, inode_number);

    if (inode == NULL) {
        free(sb);
        free(inode);
        return EXIT_FAILURE;
    }

    void *value = malloc(DDFS_BLOCK_SIZE);
    memset(value, 0, DDFS_BLOCK_SIZE);

    uint8_t key_bool = 0;
    int ret;

    for (uint8_t i = 0; i < 20; i++) {
        if (key[i] == inode->info.i_key[i]) {
            key_bool = 1;
        } else {
            key_bool = 0;
            break;
        }
    }
    
    if (key_bool == 1) {
        if (get_reference_count(fd, inode_number) > 0) {
            ret = decrement_reference_count(fd, inode_number);
        } else {
            inode = free_inode(fd, inode_number);
        
            if (inode == NULL) {
                free(sb);
                free(inode);
                return EXIT_FAILURE;
            }
            
            ret = write_block(fd, value, inode->info.i_block_ptr);

            if (ret != DDFS_BLOCK_SIZE) {
                free(sb);
                free(inode);
                return EXIT_FAILURE;
            }
        }

        free(sb);
        free(inode);
        return EXIT_SUCCESS;
    }
    
    free(sb);
    free(inode);
    free(value);
    return EXIT_SUCCESS;
}

int get_value(int fd, uint8_t key[20], uint8_t *value) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    struct ddfs_inode *inode = 
        (struct ddfs_inode*)malloc(sizeof(struct ddfs_inode));

    if (inode == NULL) {
        free(sb);
        return EXIT_FAILURE;
    }

    memset(inode, 0, sizeof(struct ddfs_inode));
    memset(value, 0, DDFS_BLOCK_SIZE);

    uint32_t inode_number = key_hash(key, sb->info.fs_inode_count);
    int64_t inode_bit = get_inode_bit(fd, inode_number);
    inode = get_inode(fd, inode_number);

    if (inode_bit == -1) {
        free(sb);
        free(inode);
        return EXIT_FAILURE;
    }

    if (inode_bit == 1) {
        uint8_t key_bool = 0;
        int ret;

        for (uint8_t i = 0; i < 20; i++) {
            if (key[i] == inode->info.i_key[i]) {
                key_bool = 1;
            } else {
                key_bool = 0;
                break;
            }
        }
        
        if (key_bool == 1) {
            ret = read_block(fd, value, inode->info.i_block_ptr);

            if (ret != DDFS_BLOCK_SIZE) {
                free(sb);
                free(inode);
                return EXIT_FAILURE;
            }

            free(sb);
            free(inode);
            return EXIT_SUCCESS;
        }
    }

    free(sb);
    free(inode);
    return EXIT_FAILURE;
}

int rename_key(int fd, uint8_t old_key[20], uint8_t new_key[20]) {
    void *value = malloc(DDFS_BLOCK_SIZE);

    if (value == NULL) {
        return EXIT_FAILURE;
    }

    if (get_value(fd, old_key, value) != 0) {
        free(value);
        return EXIT_FAILURE;
    }

    if (delete_kv_pair(fd, old_key) != 0) {
        free(value);
        return EXIT_FAILURE;
    }
    
    if (create_kv_pair(fd, new_key, value) != 0) {
        free(value);
        return EXIT_FAILURE;
    }

    free(value);
    return EXIT_SUCCESS;
}

int modify_value(int fd, uint8_t key[20], uint8_t *value) {
    if (delete_kv_pair(fd, key) != 0) {
        return EXIT_FAILURE;
    }
    
    if (create_kv_pair(fd, key, value) != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int block_exists(int fd, uint8_t *value) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return 0;
    }
    
    uint8_t *result1 = calloc(20, sizeof(uint8_t));
    uint8_t *result2 = calloc(20, sizeof(uint8_t));
    uint8_t arr1[20];
    uint8_t arr2[20];

    hash_block(value, &result1);
    
    for (uint16_t i = 0; i < 20; i++) {
        arr1[i] = result1[i];
    }

    uint32_t block_ptr = key_hash(arr1, sb->info.fs_block_count);

    void *test_value = malloc(DDFS_BLOCK_SIZE);

    if (test_value == NULL) {
        return EXIT_FAILURE;
    }

    int ret = read_block(fd, test_value, block_ptr);

    if (ret != DDFS_BLOCK_SIZE) {
        free(sb);
        return 0;
    }

    hash_block(test_value, &result2);

    for (uint16_t j = 0; j < 20; j++) {
        arr2[j] = result2[j];

        if (arr1[j] != arr2[j]) {
            return 0;
        }
    }

    free(sb);
    return 1;
}
