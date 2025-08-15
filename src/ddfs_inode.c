#include <stdio.h>
#include <stdlib.h>

#include "ddfs_inode.h"
#include "ddfs_bitmap.h"

int increment_reference_count(int fd, uint32_t inode_number) {
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

    inode = get_inode(fd, inode_number);

    if (inode->info.i_ref_count != UINT16_MAX) {
        inode->info.i_ref_count++;
    }

    uint32_t inode_offset = sb->info.fs_istore_offset + 
        (inode_number * sb->info.fs_inode_size);
    uint32_t inode_block_number = inode_offset / DDFS_BLOCK_SIZE;
    uint32_t inode_buffer_offset = inode_offset % inode_block_number;
    void *buffer = malloc(DDFS_BLOCK_SIZE);
    memset(buffer, 0, DDFS_BLOCK_SIZE);
    int ret = read_block(fd, buffer, inode_block_number);
    memcpy(buffer + inode_buffer_offset, inode, sb->info.fs_inode_size);
    ret = write_block(fd, buffer, inode_block_number);
    
    if (ret != DDFS_BLOCK_SIZE) {
        free(sb);
        free(inode);
        return EXIT_FAILURE;
    }
    
    free(sb);
    free(inode);
    return EXIT_SUCCESS;
}

int decrement_reference_count(int fd, uint32_t inode_number) {
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

    inode = get_inode(fd, inode_number);

    if (inode->info.i_ref_count != 0) {
        inode->info.i_ref_count--;
    }

    uint32_t inode_offset = sb->info.fs_istore_offset + 
        (inode_number * sb->info.fs_inode_size);
    uint32_t inode_block_number = inode_offset / DDFS_BLOCK_SIZE;
    uint32_t inode_buffer_offset = inode_offset % inode_block_number;
    void *buffer = malloc(DDFS_BLOCK_SIZE);
    memset(buffer, 0, DDFS_BLOCK_SIZE);
    int ret = read_block(fd, buffer, inode_block_number);
    memcpy(buffer + inode_buffer_offset, inode, sb->info.fs_inode_size);
    ret = write_block(fd, buffer, inode_block_number);
    
    if (ret != DDFS_BLOCK_SIZE) {
        free(sb);
        free(inode);
        return EXIT_FAILURE;
    }
    
    free(sb);
    free(inode);
    return EXIT_SUCCESS;
}

int get_reference_count(int fd, uint32_t inode_number) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return -1;
    }

    struct ddfs_inode *inode = 
        (struct ddfs_inode*)malloc(sizeof(struct ddfs_inode));

    if (inode == NULL) {
        free(sb);
        return -1;
    }

    inode = get_inode(fd, inode_number);
    int reference_count = inode->info.i_ref_count;
    
    free(sb);
    free(inode);
    return reference_count;
}

struct ddfs_inode *initialize_inode(int fd, uint32_t inode_number, 
    uint8_t key[20], uint32_t block_ptr) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return NULL;
    }

    struct ddfs_inode *inode = 
        (struct ddfs_inode*)malloc(sizeof(struct ddfs_inode));

    if (inode == NULL) {
        free(sb);
        return NULL;
    }

    memset(inode, 0, sizeof(struct ddfs_inode));

    inode->info = (struct ddfs_inode_info) {
        .i_number = inode_number,
        .i_uid = getuid(),
        .i_size = sb->info.fs_inode_size,
        .i_ref_count = 1,
        .i_mod_time = time(NULL),
        .i_block_ptr = block_ptr
    };

    for (uint8_t i = 0; i < 20; i++) {
        inode->info.i_key[i] = key[i];
    }

    uint32_t inode_offset = sb->info.fs_istore_offset + 
        (inode_number * sb->info.fs_inode_size);
    uint32_t inode_block_number = inode_offset / DDFS_BLOCK_SIZE;
    uint32_t inode_buffer_offset = inode_offset % inode_block_number;
    void *buffer = malloc(DDFS_BLOCK_SIZE);
    memset(buffer, 0, DDFS_BLOCK_SIZE);
    int ret = read_block(fd, buffer, inode_block_number);
    memcpy(buffer + inode_buffer_offset, inode, sb->info.fs_inode_size);
    ret = write_block(fd, buffer, inode_block_number);
    
    if (ret != DDFS_BLOCK_SIZE) {
        free(sb);
        free(inode);
        return NULL;
    }

    if (set_inode_bit(fd, inode_number) != 0) {
        free(sb);
        free(inode);
        return NULL;
    }
    
    free(sb);
    free(inode);
    return inode;
}

struct ddfs_inode *free_inode(int fd, uint32_t inode_number) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return NULL;
    }

    struct ddfs_inode *inode = 
        (struct ddfs_inode*)malloc(sizeof(struct ddfs_inode));

    if (inode == NULL) {
        free(sb);
        return NULL;
    }

    memset(inode, 0, sizeof(struct ddfs_inode));

    uint32_t inode_offset = sb->info.fs_istore_offset + 
        (inode_number * sb->info.fs_inode_size);
    uint32_t inode_block_number = inode_offset / DDFS_BLOCK_SIZE;
    uint32_t inode_buffer_offset = inode_offset % inode_block_number;
    void *buffer = malloc(DDFS_BLOCK_SIZE);
    memset(buffer, 0, DDFS_BLOCK_SIZE);
    int ret = read_block(fd, buffer, inode_block_number);
    memcpy(buffer + inode_buffer_offset, inode, sb->info.fs_inode_size);
    ret = write_block(fd, buffer, inode_block_number);
    
    if (ret != DDFS_BLOCK_SIZE) {
        free(sb);
        free(inode);
        return NULL;
    }

    if (clear_inode_bit(fd, inode_number) != 0) {
        free(sb);
        free(inode);
        return NULL;
    }
    
    free(sb);
    free(inode);
    return inode;
}

struct ddfs_inode *get_inode(int fd, uint32_t inode_number) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return NULL;
    }

    struct ddfs_inode *inode = 
        (struct ddfs_inode*)malloc(sizeof(struct ddfs_inode));

    if (inode == NULL) {
        free(sb);
        return NULL;
    }

    memset(inode, 0, sizeof(struct ddfs_inode));

    uint32_t inode_offset = sb->info.fs_istore_offset + 
        (inode_number * sb->info.fs_inode_size);
    uint32_t inode_block_number = inode_offset / DDFS_BLOCK_SIZE;
    uint32_t inode_buffer_offset = inode_offset % inode_block_number;
    void *buffer = malloc(DDFS_BLOCK_SIZE);
    memset(buffer, 0, DDFS_BLOCK_SIZE);
    int ret = read_block(fd, buffer, inode_block_number);
    memcpy(inode, buffer + inode_buffer_offset, sb->info.fs_inode_size);
    
    if (ret != DDFS_BLOCK_SIZE) {
        free(sb);
        free(inode);
        return NULL;
    }

    if (get_inode_bit(fd, inode_number) != 1) {
        free(sb);
        free(inode);
        return NULL;
    }
    
    free(sb);
    return inode;
}

int64_t get_next_free_inode(int fd) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    uint64_t start_bit = DDFS_BLOCK_SIZE * 8;
    uint64_t end_bit = (((sb->info.fs_ifree_block_count + 1) * 
        DDFS_BLOCK_SIZE) * 8) - 1;

    for (uint64_t i = start_bit; i <= end_bit; i++) {
        if (get_bit(fd, i) == 0) {
            free(sb);
            return i;
        }
    }

    free(sb);
    return -1;
}

int set_inode_bit(int fd, uint32_t inode_number) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    uint32_t bit_number = (DDFS_BLOCK_SIZE * 8) + inode_number;

    free(sb);
    return set_bit(fd, bit_number);
}

int clear_inode_bit(int fd, uint32_t inode_number) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    uint32_t bit_number = (DDFS_BLOCK_SIZE * 8) + inode_number;

    free(sb);
    return clear_bit(fd, bit_number);
}

int get_inode_bit(int fd, uint32_t inode_number) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    uint32_t bit_number = (DDFS_BLOCK_SIZE * 8) + inode_number;

    free(sb);
    return get_bit(fd, bit_number);
}

int initialize_superblock_inode(int fd) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    if (clear_inode_bit(fd, DDFS_BLOCK_SIZE) != 0) {
        return EXIT_FAILURE;
    }

    int64_t free_inode_bit = DDFS_BLOCK_SIZE * 8;
    int64_t free_block_bit = DDFS_BLOCK_SIZE * 8;
    uint8_t key[20];
    memset(key, 0, 20);
    
    if (initialize_inode(fd, free_inode_bit, key, free_block_bit) == NULL) {
        free(sb);
        return EXIT_FAILURE;
    }

    if (set_bit(fd, free_inode_bit) != 0) {
        free(sb);
        return EXIT_FAILURE;
    }

    free(sb);
    return EXIT_SUCCESS;
}

int initialize_ifree_inodes(int fd) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    int64_t free_inode_bit;
    int64_t free_block_bit;

    uint8_t key[20];
    memset(key, 0, 20);

    for (uint32_t i = 1; i < sb->info.fs_ifree_count + 1; i++) {
        free_inode_bit = get_next_free_inode(fd);
        free_block_bit = get_next_free_block(fd);
    
        if (initialize_inode(fd, free_inode_bit, key, free_block_bit) == NULL) {
            free(sb);
            return EXIT_FAILURE;
        }

        if (set_bit(fd, free_inode_bit) != 0) {
            free(sb);
            return EXIT_FAILURE;
        }

        if (set_bit(fd, free_block_bit) != 0) {
            free(sb);
            return EXIT_FAILURE;
        }
    }

    free(sb);
    return EXIT_SUCCESS;
}

int initialize_bfree_inodes(int fd) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    uint32_t bfree_offset = DDFS_BLOCK_SIZE * 
        (sb->info.fs_ifree_block_count + 1);

    uint8_t key[20];
    memset(key, 0, 20);

    for (uint32_t i = bfree_offset; i < sb->info.fs_istore_offset; i++) {
        if (initialize_inode(fd, i, key, i) == NULL) {
            free(sb);
            return EXIT_FAILURE;
        }
    }

    free(sb);
    return EXIT_SUCCESS;
}

int initialize_istore_inodes(int fd) {
    struct ddfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    int64_t free_inode_bit;
    int64_t free_block_bit;
    uint8_t key[20];
    memset(key, 0, 20);

    for (uint32_t i = sb->info.fs_istore_offset; 
        i < sb->info.fs_data_offset; i++) {
        free_inode_bit = get_next_free_inode(fd);
        free_block_bit = get_next_free_block(fd);
        
        if (initialize_inode(fd, free_inode_bit, key, free_block_bit) == NULL) {
            free(sb);
            return EXIT_FAILURE;
        }

        if (set_bit(fd, free_inode_bit) != 0) {
            free(sb);
            return EXIT_FAILURE;
        }

        if (set_bit(fd, free_block_bit) != 0) {
            free(sb);
            return EXIT_FAILURE;
        }
    }

    free(sb);
    return EXIT_SUCCESS;
}
