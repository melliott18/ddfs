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

#define KVFS_BLOCK_SIZE 4096
#define KVFS_MAGIC_NUM 0xBA5ED

struct kvfs_sb_info {
    int32_t fs_magic_num; // Magic number
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
    int32_t fs_uid; // Filesystem uid
    int32_t fs_name; // Filesystem name
    int32_t fs_volume_name; // Volume name
    unsigned long *fs_ifree_bitmap; // In-memory free inodes bitmap
    unsigned long *fs_bfree_bitmap; // In-memory free blocks bitmap
};

struct kvfs_superblock {
    struct kvfs_sb_info info;
    char padding[4000]; // Padding to match block size
};

struct kvfs_inode_info {
    uint32_t i_number;    // Inode number
    uint32_t i_uid;       // Owner id
    uint32_t i_size;      // Size in bytes
    uint16_t i_ref_count; // Reference count
    time_t i_mod_time;    // Modification time
    uint32_t i_block_ptr; // Block pointer
};

struct kvfs_inode {
    struct kvfs_inode_info info;
    char padding[32]; // Padding to match 64 bytes
};

static inline uint32_t div_ceil(uint32_t a, uint32_t b) {
    uint32_t ret = a / b;
    
    if (a % b) {
        return ret + 1;
    }

    return ret;
}

static int64_t get_disk_media_size(int fd) {
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

static int64_t get_disk_block_size(int fd) {
    // Get block size in bytes
    struct stat stat_buf;
    int ret = fstat(fd, &stat_buf);

    if (ret) {
        char str[80];
        perror("fstat");
        close(fd);
        return EXIT_FAILURE;
    }

    return stat_buf.st_blksize;
}

static int64_t get_disk_sector_size(int fd) {
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

static int read_block(int fd, void *buffer, uint32_t block_number) {
    lseek(fd, block_number * KVFS_BLOCK_SIZE, SEEK_SET);
    int ret = read(fd, buffer, KVFS_BLOCK_SIZE);
    lseek(fd, 0, SEEK_SET);
    return ret;
}

static int write_block(int fd, void *buffer, uint32_t block_number) {
    lseek(fd, block_number * KVFS_BLOCK_SIZE, SEEK_SET);
    int ret = write(fd, buffer, KVFS_BLOCK_SIZE);
    lseek(fd, 0, SEEK_SET);
    return ret;
}

static struct kvfs_superblock *read_superblock(int fd) {
    struct kvfs_superblock *sb = malloc(KVFS_BLOCK_SIZE);
    
    if (sb == NULL) {
        return NULL;
    }

    int ret = read_block(fd, sb, 0);
    
    if (ret != KVFS_BLOCK_SIZE) {
        free(sb);
        return NULL;
    }

    return sb;
}

static struct kvfs_superblock *write_superblock(int fd) {
    struct kvfs_superblock *sb = malloc(KVFS_BLOCK_SIZE);
    
    if (sb == NULL) {
        return NULL;
    }

    memset(sb, 0, KVFS_BLOCK_SIZE);

    uint64_t media_size = get_disk_media_size(fd);
    uint32_t block_count = media_size / KVFS_BLOCK_SIZE;
    uint32_t inode_count = block_count;
    uint32_t inode_size = sizeof(struct kvfs_inode);
    uint32_t inodes_per_block = KVFS_BLOCK_SIZE / inode_size;
    uint32_t ifree_block_count = div_ceil(inode_count, KVFS_BLOCK_SIZE * 8);
    uint32_t bfree_block_count = div_ceil(block_count, KVFS_BLOCK_SIZE * 8);
    uint32_t istore_block_count = div_ceil(inode_count, inodes_per_block);
    uint32_t data_block_count = block_count - ifree_block_count
        - bfree_block_count - istore_block_count - 1;
    uint32_t istore_offset = KVFS_BLOCK_SIZE
        * (ifree_block_count + bfree_block_count + 1);
    uint32_t data_offset = istore_offset + 
        (istore_block_count * KVFS_BLOCK_SIZE);

    sb->info = (struct kvfs_sb_info) {
        .fs_magic_num = htole32(KVFS_MAGIC_NUM),
        .fs_media_size = htole64(media_size),
        .fs_block_size = htole32(KVFS_BLOCK_SIZE),
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
        .fs_data_offset = htole32(data_offset)
        //int32_t fs_uid; // Filesystem uid
        //int32_t fs_name; // Filesystem name
        //int32_t fs_volume_name; // Volume name
    };

    int ret = write_block(fd, sb, 0);
    
    if (ret != KVFS_BLOCK_SIZE) {
        free(sb);
        return NULL;
    }

    return sb;
}

static int erase_disk(int fd) {
    int64_t media_size = get_disk_media_size(fd);
    int32_t block_count = div_ceil(media_size, KVFS_BLOCK_SIZE);
    void *buffer = malloc(KVFS_BLOCK_SIZE);
    memset(buffer, 0, KVFS_BLOCK_SIZE);
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

static int erase_superblock(int fd) {
    void *buffer = malloc(KVFS_BLOCK_SIZE);

    if (!buffer) {
        return EXIT_FAILURE;
    }

    memset(buffer, 0, KVFS_BLOCK_SIZE);

    int ret = write_block(fd, buffer, 0);

    free(buffer);
    return ret;
}

static int erase_ifree_blocks(int fd) {
    void *buffer = malloc(KVFS_BLOCK_SIZE);

    if (!buffer) {
        return EXIT_FAILURE;
    }

    struct kvfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        free(buffer);
        return NULL;
    }

    memset(buffer, 0, KVFS_BLOCK_SIZE);

    int ret;

    for (uint32_t i = 1; i < le32toh(sb->info.fs_ifree_block_count) + 1; i++) {
        ret = write_block(fd, buffer, i);
        
        if (ret != KVFS_BLOCK_SIZE) {
            free(buffer);
            return EXIT_FAILURE;
        }
    }

    free(buffer);
    return EXIT_SUCCESS;
}

static int erase_bfree_blocks(int fd) {
    void *buffer = malloc(KVFS_BLOCK_SIZE);

    if (!buffer) {
        return EXIT_FAILURE;
    }

    struct kvfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        free(buffer);
        return NULL;
    }

    memset(buffer, 0, KVFS_BLOCK_SIZE);

    int ret;
    uint32_t bfree_block_offset = sb->info.fs_ifree_block_count + 1;
    uint32_t istore_block_offset = sb->info.fs_istore_offset / KVFS_BLOCK_SIZE;

    for (uint32_t i = bfree_block_offset; i < istore_block_offset; i++) {
        ret = write_block(fd, buffer, i);
        
        if (ret != KVFS_BLOCK_SIZE) {
            free(buffer);
            return EXIT_FAILURE;
        }
    }

    free(buffer);
    return EXIT_SUCCESS;
}

static int erase_inode_store(int fd) {
    void *buffer = malloc(KVFS_BLOCK_SIZE);

    if (!buffer) {
        return EXIT_FAILURE;
    }

    memset(buffer, 0, KVFS_BLOCK_SIZE);
    struct kvfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        free(buffer);
        return NULL;
    }

    int ret;
    int istore_block_offset = sb->info.fs_istore_offset / KVFS_BLOCK_SIZE;

    for (uint32_t i = istore_block_offset; 
        i < (le32toh(sb->info.fs_istore_block_count) + 
        istore_block_offset); i++) {
        ret = write_block(fd, buffer, i);
        
        if (ret != KVFS_BLOCK_SIZE) {
            free(buffer);
            return EXIT_FAILURE;
        }
    }

    free(buffer);
    return EXIT_SUCCESS;
}

static struct kvfs_inode *initialize_inode(int fd, uint32_t inode_number, 
    uint32_t block_ptr) {
    struct kvfs_inode *inode = 
        (struct kvfs_inode*)malloc(sizeof(struct kvfs_inode));

    if (inode == NULL) {
        return NULL;
    }

    struct kvfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        free(inode);
        return NULL;
    }

    memset(inode, 0, sizeof(struct kvfs_inode));

    inode->info = (struct kvfs_inode_info) {
        .i_number = inode_number,
        .i_uid = 0,
        .i_size = sb->info.fs_inode_size,
        .i_ref_count = 1,
        .i_mod_time = time(NULL),
        .i_block_ptr = block_ptr
    };

    uint32_t inode_offset = sb->info.fs_istore_offset + 
        (inode_number * sb->info.fs_inode_size);
    printf("Inode offset: %d\n", inode_offset);
    uint32_t inode_block_number = div_ceil(inode_offset, KVFS_BLOCK_SIZE);
    printf("Inode block number: %d\n", inode_block_number);
    uint32_t inode_buffer_offset = inode_offset % inode_block_number;
    printf("Inode buffer offset: %d\n", inode_buffer_offset);
    void *buffer = malloc(KVFS_BLOCK_SIZE);
    memset(buffer, 0, KVFS_BLOCK_SIZE);
    int ret = read_block(fd, buffer, inode_block_number);
    memcpy(buffer + inode_buffer_offset, inode, sb->info.fs_inode_size);
    ret = write_block(fd, buffer, inode_block_number);
    printf("Inode size: %d\n", inode->info.i_size);
    printf("ret: %d\n", ret);
    printf("Inode block pointer: %d\n", inode->info.i_block_ptr);
    printf("\n");
    
    if (ret != KVFS_BLOCK_SIZE) {
        free(sb);
        return NULL;
    }
    
    return inode;
}

static struct kvfs_inode *free_inode(int fd, uint32_t inode_number, 
    uint32_t block_ptr) {
    struct kvfs_inode *inode = 
        (struct kvfs_inode*)malloc(sizeof(struct kvfs_inode));

    if (inode == NULL) {
        return NULL;
    }

    struct kvfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        free(inode);
        return NULL;
    }

    memset(inode, 0, sizeof(struct kvfs_inode));

    uint32_t inode_offset = sb->info.fs_istore_offset + 
        (inode_number * sb->info.fs_inode_size);
    printf("Inode offset: %d\n", inode_offset);
    uint32_t inode_block_number = div_ceil(inode_offset, KVFS_BLOCK_SIZE);
    printf("Inode block number: %d\n", inode_block_number);
    uint32_t inode_buffer_offset = inode_offset % inode_block_number;
    printf("Inode buffer offset: %d\n", inode_buffer_offset);
    void *buffer = malloc(KVFS_BLOCK_SIZE);
    memset(buffer, 0, KVFS_BLOCK_SIZE);
    int ret = read_block(fd, buffer, inode_block_number);
    memcpy(buffer + inode_buffer_offset, inode, sb->info.fs_inode_size);
    ret = write_block(fd, buffer, inode_block_number);
    printf("Inode size: %d\n", inode->info.i_size);
    printf("ret: %d\n", ret);
    printf("Inode block pointer: %d\n", inode->info.i_block_ptr);
    printf("\n");
    
    if (ret != KVFS_BLOCK_SIZE) {
        free(sb);
        return NULL;
    }
    
    return inode;
}

static struct kvfs_inode *get_inode(int fd, uint32_t inode_number, 
    uint32_t block_ptr) {
    struct kvfs_inode *inode = 
        (struct kvfs_inode*)malloc(sizeof(struct kvfs_inode));

    if (inode == NULL) {
        return NULL;
    }

    struct kvfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        free(inode);
        return NULL;
    }

    memset(inode, 0, sizeof(struct kvfs_inode));

    uint32_t inode_offset = sb->info.fs_istore_offset + 
        (inode_number * sb->info.fs_inode_size);
    printf("Inode offset: %d\n", inode_offset);
    uint32_t inode_block_number = div_ceil(inode_offset, KVFS_BLOCK_SIZE);
    printf("Inode block number: %d\n", inode_block_number);
    uint32_t inode_buffer_offset = inode_offset % inode_block_number;
    printf("Inode buffer offset: %d\n", inode_buffer_offset);
    void *buffer = malloc(KVFS_BLOCK_SIZE);
    memset(buffer, 0, KVFS_BLOCK_SIZE);
    int ret = read_block(fd, buffer, inode_block_number);
    memcpy(inode, buffer + inode_buffer_offset, sb->info.fs_inode_size);
    printf("Inode size: %d\n", inode->info.i_size);
    printf("ret: %d\n", ret);
    printf("Inode block pointer: %d\n", inode->info.i_block_ptr);
    printf("\n");
    
    if (ret != KVFS_BLOCK_SIZE) {
        free(sb);
        return NULL;
    }
    
    return inode;
}

static int initialize_superblock_inode(int fd) {
    struct kvfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return NULL;
    }

    if (initialize_inode(fd, 0, 0) == NULL) {
        free(sb);
        return EXIT_FAILURE;
    }

    free(sb);
    return EXIT_SUCCESS;
}

static int initialize_ifree_inodes(int fd) {
    struct kvfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    for (uint32_t i = 1; i < sb->info.fs_ifree_count + 1; i++) {
        if (initialize_inode(fd, i, i) == NULL) {
            free(sb);
            return EXIT_FAILURE;
        }
    }

    free(sb);
    return EXIT_SUCCESS;
}

static int initialize_bfree_inodes(int fd) {
    struct kvfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    uint32_t bfree_offset = KVFS_BLOCK_SIZE * 
        (sb->info.fs_ifree_block_count + 1);

    for (uint32_t i = bfree_offset; i < sb->info.fs_istore_offset; i++) {
        if (initialize_inode(fd, i, i) == NULL) {
            free(sb);
            return EXIT_FAILURE;
        }
    }

    free(sb);
    return EXIT_SUCCESS;
}

static int initialize_istore_inodes(int fd) {
    struct kvfs_superblock *sb = read_superblock(fd);

    if (sb == NULL) {
        return EXIT_FAILURE;
    }

    for (uint32_t i = sb->info.fs_istore_offset; 
        i < sb->info.fs_data_offset; i++) {
        if (initialize_inode(fd, i, i) == NULL) {
            free(sb);
            return EXIT_FAILURE;
        }
    }

    free(sb);
    return EXIT_SUCCESS;
}

static int initialize_kvfs(int fd) {
    // Write superblock (block 0)
    struct kvfs_superblock *sb = write_superblock(fd);
    
    if (sb == NULL) {
        perror("write_superblock()");
        free(sb);
        return EXIT_FAILURE;
    }
    
    int ret;
    
    ret = initialize_superblock_inode(fd);

    if (ret) {
        return EXIT_FAILURE;
    }

    ret = initialize_ifree_inodes(fd);

    if (ret) {
        return EXIT_FAILURE;
    }

    ret = initialize_bfree_inodes(fd);

    if (ret) {
        return EXIT_FAILURE;
    }

    ret = initialize_istore_inodes(fd);

    if (ret) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: makefs-kvfs <image-file>\n");
        return EXIT_FAILURE;
    }

    int fd = open(argv[1], O_RDWR, 0);

    if (fd == -1) {
        char str[80];
        int n = sprintf(str, "open(): %s", argv[1]);
        perror(str);
        close(fd);
        return EXIT_FAILURE;
    }

    // Write superblock (block 0)
    struct kvfs_superblock *sb = write_superblock(fd);
    
    if (sb == NULL) {
        perror("write_superblock()");
        close(fd);
        free(sb);
        return EXIT_FAILURE;
    }

    // Read superblock (block 0)
    sb = read_superblock(fd);
    
    if (sb == NULL) {
        perror("read_superblock()");
        close(fd);
        free(sb);
        return EXIT_FAILURE;
    }

    printf("Magic number: %d\n", sb->info.fs_magic_num);
    printf("Media size: %lu\n", sb->info.fs_media_size);
    printf("Block size: %d\n", sb->info.fs_block_size);
    printf("Block count: %d\n", sb->info.fs_block_count);
    printf("ifree block count: %d\n", sb->info.fs_ifree_block_count);
    printf("bfree block count: %d\n", sb->info.fs_bfree_block_count);
    printf("istore block count: %d\n", sb->info.fs_istore_block_count);
    printf("Data block count: %d\n", sb->info.fs_data_block_count);
    printf("inode size: %d\n", sb->info.fs_inode_size);
    printf("inode count: %d\n", sb->info.fs_inode_count);
    printf("ifree count: %d\n", sb->info.fs_ifree_count);
    printf("bfree count: %d\n", sb->info.fs_bfree_count);
    printf("istore offset: %d\n", sb->info.fs_istore_offset);
    printf("Data offset: %d\n", sb->info.fs_data_offset);
    printf("\n");

    struct kvfs_inode *inode = initialize_inode(fd, 0, 0);

    if (inode == NULL) {
        perror("inode()");
        close(fd);
        free(sb);
        return EXIT_FAILURE;
    }

    printf("Inode number: %d\n", inode->info.i_number);
    printf("Inode uid: %d\n", inode->info.i_uid);
    printf("Inode size: %d\n", inode->info.i_size);
    printf("Inode reference count: %d\n", inode->info.i_ref_count);
    printf("Inode modification time: %d\n", inode->info.i_mod_time);
    printf("Inode block pointer: %d\n", inode->info.i_block_ptr);
    printf("\n");

    //erase_superblock(fd);
    inode = get_inode(fd, 0, 0);

    if (inode == NULL) {
        perror("inode()");
        close(fd);
        free(sb);
        return EXIT_FAILURE;
    }

    printf("Inode number: %d\n", inode->info.i_number);
    printf("Inode uid: %d\n", inode->info.i_uid);
    printf("Inode size: %d\n", inode->info.i_size);
    printf("Inode reference count: %d\n", inode->info.i_ref_count);
    printf("Inode modification time: %d\n", inode->info.i_mod_time);
    printf("Inode block pointer: %d\n", inode->info.i_block_ptr);
    printf("\n");

    inode = initialize_inode(fd, 4129, 4129);

    if (inode == NULL) {
        perror("inode()");
        close(fd);
        free(sb);
        return EXIT_FAILURE;
    }

    printf("Inode number: %d\n", inode->info.i_number);
    printf("Inode uid: %d\n", inode->info.i_uid);
    printf("Inode size: %d\n", inode->info.i_size);
    printf("Inode reference count: %d\n", inode->info.i_ref_count);
    printf("Inode modification time: %d\n", inode->info.i_mod_time);
    printf("Inode block pointer: %d\n", inode->info.i_block_ptr);
    printf("\n");

    //erase_inode_store(fd);
    inode = get_inode(fd, 4129, 4129);

    if (inode == NULL) {
        perror("inode()");
        close(fd);
        free(sb);
        return EXIT_FAILURE;
    }

    printf("Inode number: %d\n", inode->info.i_number);
    printf("Inode uid: %d\n", inode->info.i_uid);
    printf("Inode size: %d\n", inode->info.i_size);
    printf("Inode reference count: %d\n", inode->info.i_ref_count);
    printf("Inode modification time: %d\n", inode->info.i_mod_time);
    printf("Inode block pointer: %d\n", inode->info.i_block_ptr);
    printf("\n");

    

    return EXIT_SUCCESS;
}
