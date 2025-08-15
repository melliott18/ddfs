#include "../src/ddfs.h"
#include "../src/ddfs_bitmap.h"
#include "../src/ddfs_inode.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, 
            "Usage: ./test-ddfs <image-file>\n");
        return EXIT_FAILURE;
    }

    int fd = open(argv[1], O_RDWR, 0);

    if (fd == -1) {
        char str[80];
        sprintf(str, "open(): %s", argv[1]);
        perror(str);
        close(fd);
        return EXIT_FAILURE;
    }

    // Read superblock (block 0)
    struct ddfs_superblock *sb = read_superblock(fd);
    
    if (sb == NULL) {
        perror("read_superblock()");
        close(fd);
        free(sb);
        return EXIT_FAILURE;
    }

    printf("Testing read_superblock()\n");
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
    printf("File system uid: %d\n", sb->info.fs_uid);
    printf("File system name: ");
    for (int i = 0; i < 4; i++) {
        printf("%c", sb->info.fs_name[i]);
    }
    printf("\n\n");

    char *file_name = "5eee38381388b6f30efdd5c5c6f067dbf32c0bb3\0";
    uint8_t key[20];
    file_name_to_key(file_name, key);

    printf("Test key: ");
    for (int i = 0; i < 20; i++) {
        printf("%x", key[i]);
    }

    printf("\n\n");

    uint8_t *data = malloc(DDFS_BLOCK_SIZE);
    time_t t;
    srand((unsigned) time(&t));
    
    printf("Test data: \n");
    for (uint16_t i = 0; i < DDFS_BLOCK_SIZE; i++) {
        data[i] = rand();
        printf("%x", data[i]);
    }
    
    printf("\n\n");

    uint8_t *result = calloc(20, sizeof(uint8_t));
    hash_block(data, &result);

    for (uint16_t c = 0; c < 20; c++) {
        key[c] = result[c];
    }
    
    int ret = create_kv_pair(fd, key, data);

    if (ret == 0) {
        printf("Test create_kv_pair() successful\n\n");
    } else {
        printf("Test create_kv_pair() unsuccessful\n\n");
    }
    
    if (get_value(fd, key, data) == 0) {
        printf("Test get_value() successful\n\n");
        printf("Get data from ddfs: \n");
        for (uint16_t i = 0; i < DDFS_BLOCK_SIZE; i++) {
            printf("%x", data[i]);
        }
        printf("\n");
    } else {
        printf("Test get_value() unsuccessful\n\n");
    }

    printf("\n");

    uint32_t inode_number = key_hash(key, sb->info.fs_inode_count);
    int reference_count = get_reference_count(fd, inode_number);

    uint8_t arr[20];
    hash_block(data, &result);

    for (uint16_t c = 0; c < 20; c++) {
        arr[c] = result[c];
    }
    
    uint32_t block_ptr = key_hash(arr, sb->info.fs_block_count);

    ret = block_exists(fd, data);

    if (ret == 0) {
        printf("Block does not exist\n\n");
    } else {
        printf("Block exists\n\n");
    }
    
    printf("Block number: %d\n", block_ptr);
    printf("Reference count: %d\n", reference_count);
    printf("\n");

    ret = create_kv_pair(fd, key, data);

    if (ret == 0) {
        printf("Test create_kv_pair() successful\n\n");
    } else {
        printf("Test create_kv_pair() unsuccessful\n\n");
    }

    reference_count = get_reference_count(fd, inode_number);

    printf("Block number: %d\n", block_ptr);
    printf("Reference count: %d\n", reference_count);
    printf("\n");

    ret = delete_kv_pair(fd, key);

    if (ret == 0) {
        printf("Test delete_kv_pair() successful\n\n");
    } else {
        printf("Test delete_kv_pair() unsuccessful\n\n");
    }

    reference_count = get_reference_count(fd, inode_number);

    printf("Block number: %d\n", block_ptr);
    printf("Reference count: %d\n", reference_count);
    printf("\n");

    ret = delete_kv_pair(fd, key);

    if (ret == 0) {
        printf("Test delete_kv_pair() successful\n\n");
    } else {
        printf("Test delete_kv_pair() unsuccessful\n\n");
    }

    reference_count = get_reference_count(fd, inode_number);

    printf("Block number: %d\n", block_ptr);
    printf("Reference count: %d\n", reference_count);
    printf("\n");

    return EXIT_SUCCESS;
}
