#include "io.h"
#include <stdio.h>
#include "ibfs.h" 

int read_block(IBFS_Context* ctx, uint32_t block_num, void* buffer) {
    if (!ctx || !ctx->disk_file || !buffer) return -1; 

    if (ctx->sb.block_count > 0 && block_num >= ctx->sb.block_count) {
        fprintf(stderr, "Error: Attempt to read block %u beyond disk boundary (%u)\n",
                block_num, ctx->sb.block_count);
        return -1;
    }

    if (fseek(ctx->disk_file, (long)block_num * BLOCK_SIZE, SEEK_SET) != 0) {
        perror("Error seeking for read");
        return -1;
    }
    size_t blocks_read = fread(buffer, BLOCK_SIZE, 1, ctx->disk_file);
    if (blocks_read != 1) {
        fprintf(stderr, "Error: Failed to read block %u", block_num);
        if (feof(ctx->disk_file)) {
             fprintf(stderr, " (Unexpected end of file - is the disk large enough?)\n");
        } else if (ferror(ctx->disk_file)) {
             perror(" (fread error)");
        } else {
             fprintf(stderr, " (Read %zu blocks instead of 1)\n", blocks_read);
        }
        return -1;
    }
    return 0; 
}

int write_block(IBFS_Context* ctx, uint32_t block_num, const void* buffer) {
    if (!ctx || !ctx->disk_file || !buffer) return -1; 

     if (ctx->sb.block_count > 0 && block_num >= ctx->sb.block_count) {
        fprintf(stderr, "Error: Attempt to write block %u beyond disk boundary (%u)\n",
                block_num, ctx->sb.block_count);
        return -1;
    }

    if (fseek(ctx->disk_file, (long)block_num * BLOCK_SIZE, SEEK_SET) != 0) {
        perror("Error seeking for write");
        return -1;
    }
    size_t blocks_written = fwrite(buffer, BLOCK_SIZE, 1, ctx->disk_file);
    if (blocks_written != 1) {
        perror("Error writing block");
        fprintf(stderr, " (Attempted to write block %u, wrote %zu)\n", block_num, blocks_written);
        return -1;
    }
    return 0; 
}