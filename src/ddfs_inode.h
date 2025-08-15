#ifndef ddfs_INODE_H
#define	ddfs_INODE_H

#include "ddfs.h"

struct ddfs_inode_info {
    struct	vnode  *i_vnode;   // Vnode associated with this inode
    struct 	ddfsmount *i_kvmp; // ddfsmount point associated with this inode
    uint32_t i_number;         // Inode number
    uint32_t i_uid;            // Owner id
    uint32_t i_size;           // Size in bytes
    uint8_t i_key[20];         // 160-bit key
    uint16_t i_ref_count;      // Reference count
    time_t i_mod_time;         // Modification time
    uint32_t i_block_ptr;      // Block pointer
};

struct ddfs_inode {
    struct ddfs_inode_info info;
    char padding[52]; // Padding to match 128 bytes
};

extern int increment_reference_count(int fd, uint32_t inode_number);

extern int decrement_reference_count(int fd, uint32_t inode_number);

extern int get_reference_count(int fd, uint32_t inode_number);

extern struct ddfs_inode *initialize_inode(int fd, uint32_t inode_number, 
    uint8_t key[20], uint32_t block_ptr);

extern struct ddfs_inode *free_inode(int fd, uint32_t inode_number);

extern struct ddfs_inode *get_inode(int fd, uint32_t inode_number);

extern int64_t get_next_free_inode(int fd);

extern int set_inode_bit(int fd, uint32_t inode_number);

extern int clear_inode_bit(int fd, uint32_t inode_number);

extern int get_inode_bit(int fd, uint32_t inode_number);

extern int initialize_superblock_inode(int fd);

extern int initialize_ifree_inodes(int fd);

extern int initialize_bfree_inodes(int fd);

extern int initialize_istore_inodes(int fd);

#endif
