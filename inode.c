#include "inode.h"
#include "bitmap.h"
#include "io.h"
#include <string.h>
#include <stdio.h> 
#include <time.h>  

#define INODE_TABLE_START_BLOCK 3

int inode_write(IBFS_Context* ctx, uint32_t inode_num, const Inode* inode_data)
{
    if (inode_num >= ctx->sb.inode_count) {
        fprintf(stderr, "inode_write: Error - inode number %u out of range.\n", inode_num);
        return -1;
    }

    uint32_t inodes_per_block = BLOCK_SIZE / sizeof(Inode);
    if (inodes_per_block == 0) {
         fprintf(stderr, "inode_write: Error - Inode size too large for block size.\n");
         return -1;
    }
    uint32_t block_num = INODE_TABLE_START_BLOCK + (inode_num / inodes_per_block);
    char block_buffer[BLOCK_SIZE];

    if (read_block(ctx, block_num, block_buffer) != 0)
    {
        fprintf(stderr, "inode_write: Failed to read block %u for inode %u.\n", block_num, inode_num);
        return -1;
    }

    uint32_t offset_in_block = (inode_num % inodes_per_block) * sizeof(Inode);
    memcpy(block_buffer + offset_in_block, inode_data, sizeof(Inode));

    return write_block(ctx, block_num, block_buffer);
}

int inode_read(IBFS_Context* ctx, uint32_t inode_num, Inode* inode_data)
{
     if (inode_num >= ctx->sb.inode_count) {
        fprintf(stderr, "inode_read: Error - inode number %u out of range.\n", inode_num);
        return -1;
    }

    uint32_t inodes_per_block = BLOCK_SIZE / sizeof(Inode);
     if (inodes_per_block == 0) {
         fprintf(stderr, "inode_read: Error - Inode size too large for block size.\n");
         return -1;
    }
    uint32_t block_num = INODE_TABLE_START_BLOCK + (inode_num / inodes_per_block);
    char block_buffer[BLOCK_SIZE];

    if (read_block(ctx, block_num, block_buffer) != 0) {
        return -1;
    }

    uint32_t offset_in_block = (inode_num % inodes_per_block) * sizeof(Inode);
    memcpy(inode_data, block_buffer + offset_in_block, sizeof(Inode));
    return 0;
}

int inode_alloc(IBFS_Context *ctx, uint16_t mode)
{
    int inode_num = alloc_inode_num(ctx);
    if (inode_num < 0) {
        return -1;
    }

    Inode new_inode;
    memset(&new_inode, 0, sizeof(Inode));

    time_t current_time = time(NULL);

    new_inode.mode = mode;
    new_inode.links_count = 1;
    new_inode.size = 0;
    new_inode.atime = current_time;
    new_inode.mtime = current_time;
    new_inode.ctime = current_time;

    if (inode_write(ctx, inode_num, &new_inode) != 0)
    {
        fprintf(stderr, "inode_alloc: Failed to write new inode %d after allocation.\n", inode_num);
        free_inode_num(ctx, inode_num);
        return -1;
    }
    return inode_num;
}