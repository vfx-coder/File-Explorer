#pragma once
#include <stdint.h>
#include <time.h> 

#define BLOCK_SIZE 4096
#define IBFS_MAGIC_NUMBER 0xDEADBEEF

typedef struct Superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t inode_count;
    uint32_t block_count;
    uint32_t root_inode;
    uint32_t root_bpt_block;
} Superblock;

typedef struct Inode {
    uint16_t mode;       
    uint16_t links_count;
    uint64_t size;       
    time_t   atime;      
    time_t   mtime;      
    time_t   ctime;      
    uint32_t direct_blocks[12];
    uint32_t single_indirect;
} Inode;