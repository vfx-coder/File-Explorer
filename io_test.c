#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ibfs.h"
#include "io.h"  

int main() {
    const char* test_filename = "io_test.disk";
    IBFS_Context ctx;
    
    ctx.disk_file = fopen(test_filename, "wb+");
    if (!ctx.disk_file) {
        perror("Failed to create test file");
        return 1;
    }

    printf("--- Running I/O Write/Read Test ---\n");

    char write_buffer[BLOCK_SIZE];
    memset(write_buffer, 'A', BLOCK_SIZE);

    printf("Writing pattern to Block 0...\n");
    if (write_block(&ctx, 0, write_buffer) != 0) {
        fprintf(stderr, "TEST FAILED: write_block returned an error.\n");
        fclose(ctx.disk_file);
        return 1;
    }
    printf("Write completed.\n");
    
    char read_buffer[BLOCK_SIZE];
    memset(read_buffer, 'B', BLOCK_SIZE);

    printf("Reading pattern from Block 0...\n");
    if (read_block(&ctx, 0, read_buffer) != 0) {
        fprintf(stderr, "TEST FAILED: read_block returned an error.\n");
        fclose(ctx.disk_file);
        return 1;
    }
    printf("Read completed.\n");

    if (memcmp(write_buffer, read_buffer, BLOCK_SIZE) == 0) {
        printf("SUCCESS! Data written and read back correctly.\n");
    } else {
        printf("TEST FAILED: Data read back does not match what was written.\n");
    }

    fclose(ctx.disk_file);
    return 0;
}