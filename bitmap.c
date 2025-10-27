#include "bitmap.h"
#include "block.h" 
#include "io.h"
#include <stdio.h>
#include <string.h>
#include "ibfs.h"  

#define INODE_BITMAP_BLOCK 1
#define DATA_BITMAP_BLOCK 2

int alloc_inode_num(IBFS_Context* ctx)
{
    char block_buffer[BLOCK_SIZE];
    if (read_block(ctx, INODE_BITMAP_BLOCK, block_buffer) != 0)
    {
        fprintf(stderr, "alloc_inode_num: Failed to read inode bitmap block\n");
        return -1;
    }

    if (ctx->sb.inode_count == 0) {
        fprintf(stderr, "alloc_inode_num: Error - inode_count in context is zero.\n");
        return -1;
    }

    for (uint32_t i = 0; i < ctx->sb.inode_count; ++i)
    {
        uint32_t byte_index = i / 8;
        uint32_t bit_index = i % 8;

        if (byte_index >= BLOCK_SIZE) {
            fprintf(stderr, "alloc_inode_num: Error - inode_count %u exceeds bitmap block size.\n", ctx->sb.inode_count);
            return -1; 
        }

        if (!((block_buffer[byte_index] >> bit_index) & 1))
        {
            block_buffer[byte_index] |= (1 << bit_index); 

            if (write_block(ctx, INODE_BITMAP_BLOCK, block_buffer) != 0) {
                fprintf(stderr, "alloc_inode_num: failed to write updated inode bitmap block\n");
                block_buffer[byte_index] &= ~(1 << bit_index);
                return -1;
            }
            return i; 
        }
    }
    fprintf(stderr, "Error: No free inodes available.\n");
    return -1; 
}

void free_inode_num(IBFS_Context* ctx, uint32_t inode_num) {
     if (inode_num >= ctx->sb.inode_count) {
        fprintf(stderr, "free_inode_num: Error - inode number %u out of range (max %u).\n",
                inode_num, ctx->sb.inode_count - 1);
        return;
    }

    char block_buffer[BLOCK_SIZE];
    if (read_block(ctx, INODE_BITMAP_BLOCK, block_buffer) != 0)
    {
        fprintf(stderr, "free_inode_num: Failed to read inode bitmap block\n");
        return;
    }
    uint32_t byte_index = inode_num / 8;
    uint32_t bit_index = inode_num % 8;

    if (!((block_buffer[byte_index] >> bit_index) & 1)) {
       fprintf(stderr, "Warning: Attempt to free already free inode %u.\n", inode_num);
    }

    block_buffer[byte_index] &= ~(1 << bit_index); 
    if (write_block(ctx, INODE_BITMAP_BLOCK, block_buffer) != 0)
    {
        fprintf(stderr, "free_inode_num: failed to write updated inode bitmap\n");
    }
}

uint32_t alloc_data_block(IBFS_Context* ctx)
{
    char block_buffer[BLOCK_SIZE];
    if (read_block(ctx, DATA_BITMAP_BLOCK, block_buffer) != 0)
    {
        fprintf(stderr, "alloc_data_block: Failed to read data bitmap block\n");
        return 0;
    }

    if (ctx->sb.block_count == 0) {
        fprintf(stderr, "alloc_data_block: Error - block_count in context is zero.\n");
        return 0;
    }

    for (uint32_t i = 3; i < ctx->sb.block_count; i++)
    {
        uint32_t byte_index = i / 8;
        uint32_t bit_index = i % 8;

        if (byte_index >= BLOCK_SIZE) {
            fprintf(stderr, "alloc_data_block: Error - block_count %u exceeds bitmap block size.\n", ctx->sb.block_count);
            return 0;
        }

        if (!((block_buffer[byte_index] >> bit_index) & 1))
        {
            block_buffer[byte_index] |= (1 << bit_index); 
            if (write_block(ctx, DATA_BITMAP_BLOCK, block_buffer) != 0)
            {
                fprintf(stderr, "alloc_data_block: Failed to write updated data bitmap\n");
                 block_buffer[byte_index] &= ~(1 << bit_index);
                return 0;
            }
            return i;
        }
    }
    fprintf(stderr, "Error: No free data blocks available.\n");
    return 0;
}

void free_data_block(IBFS_Context* ctx, uint32_t block_num){
    if (block_num < 3 || block_num >= ctx->sb.block_count) {
        fprintf(stderr, "free_data_block: Error - block number %u out of valid range (3-%u).\n",
                block_num, ctx->sb.block_count - 1);
        return;
    }

    char block_buffer[BLOCK_SIZE];
    if (read_block(ctx, DATA_BITMAP_BLOCK, block_buffer) != 0)
    {
        fprintf(stderr, "free_data_block: Failed to read data bitmap block\n");
        return;
    }
    uint32_t byte_index = block_num / 8;
    uint32_t bit_index = block_num % 8;

    if (!((block_buffer[byte_index] >> bit_index) & 1)) {
       fprintf(stderr, "Warning: Attempt to free already free data block %u.\n", block_num);
    }

    block_buffer[byte_index] &= ~(1 << bit_index); 
    if (write_block(ctx, DATA_BITMAP_BLOCK, block_buffer) != 0)
    {
        fprintf(stderr, "free_data_block: failed to write update data bitmap\n");
    }
}