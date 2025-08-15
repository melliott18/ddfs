#include "../src/ddfs.h"
#include "../src/ddfs_bitmap.h"
#include "../src/ddfs_inode.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./makefs-ddfs <image-file>\n");
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

    uint8_t disk_already_formatted = 0;

    if (sb->info.fs_magic_num == DDFS_MAGIC_NUM) {
        printf("Disk %s already formatted with ddfs.\n", argv[1]);
        printf("Do you wish to continue? [y/n] ");
        char answer;
        scanf("%c", &answer);

        if (answer == 'n' || answer == 'N') {
            close(fd);
            free(sb);
            return EXIT_SUCCESS;
        } else if (!(answer == 'y' || answer == 'Y')) {
            close(fd);
            free(sb);
            return EXIT_SUCCESS;
        }

        disk_already_formatted = 1;
    }

    erase_ifree_blocks(fd);
    erase_bfree_blocks(fd);
    erase_inode_store(fd);

    // Write superblock (block 0)
    sb = write_superblock(fd);
    
    if (sb == NULL) {
        perror("write_superblock()");
        close(fd);
        free(sb);
        return EXIT_FAILURE;
    }

    initialize_ddfs(fd);
    free(sb);

    if (disk_already_formatted) {
        printf("Disk %s has been reformatted.\n", argv[1]);
    } else {
        printf("Disk %s has been formatted.\n", argv[1]);
    }
    
    return EXIT_SUCCESS;
}
