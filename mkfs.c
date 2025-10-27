#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ibfs.h"
#include "io.h"
#include "inode.h"
#include "bplustree.h" 
#include "block.h"     

#ifdef _WIN32
#include <io.h>
#ifndef ftruncate
#define ftruncate _chsize_s
#endif
#e
#include <unistd.h> 
#endif
#define DISK_BLOCKS 4096 
#define INODE_COUNT 1024 

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <disk_filename>\n", argv[0]);
        return 1;
    }
    const char* filename = argv[1];

    FILE *disk = fopen(filename, "wb+");
    if (!disk) {
        perror("Error creating disk file");
        return 1;
    }
    long long target_size = (long long)DISK_BLOCKS * BLOCK_SIZE;
    if (ftruncate(fileno(disk), target_size) != 0) {
        perror("Error setting disk size");
        fseek(disk, 0, SEEK_END);
        long long current_size = ftell(disk);
        fprintf(stderr, " (Target: %lld bytes, Current: %lld bytes)\n", target_size, current_size);
        fclose(disk);
        return 1;
    }

    IBFS_Context temp_ctx;
    temp_ctx.disk_file = disk;
    temp_ctx.sb.inode_count = INODE_COUNT;
    temp_ctx.sb.block_count = DISK_BLOCKS;

    printf("Initializing bitmaps...\n");
    char zero_buffer[BLOCK_SIZE] = {0};
    if (write_block(&temp_ctx, 1, zero_buffer) != 0) { fclose(disk); return 1; } 
    if (write_block(&temp_ctx, 2, zero_buffer) != 0) { fclose(disk); return 1; } 

    printf("Creating root inode...\n");
    int root_inode_num = inode_alloc(&temp_ctx, S_IFDIR);
    if (root_inode_num != 0) {
        fprintf(stderr, "Error: Root inode allocation failed (expected 0, got %d).\n", root_inode_num);
        fclose(disk);
        return 1;
    }

    printf("Creating test file inode ('readme.txt')...\n");
    int test_file_inode = inode_alloc(&temp_ctx, 0);
    if (test_file_inode < 0) {
        fprintf(stderr, "Error: Failed to allocate test file inode.\n");
        fclose(disk);
        return 1;
    }
    printf("Allocated inode %d for test file.\n", test_file_inode);

    uint32_t bpt_root_block = 0;
    BPlusTreeKey test_key;
    test_key.parent_inode_id = root_inode_num;
    test_key.name_hash = hash_name("readme.txt");
    strncpy(test_key.name, "readme.txt", MAX_FILENAME_LENGTH - 1);
    test_key.name[MAX_FILENAME_LENGTH - 1] = '\0';

    printf("Inserting test file key (parent=%u, hash=%u, name='%s') into B+ Tree, mapping to inode %d...\n",
           test_key.parent_inode_id, test_key.name_hash, test_key.name, test_file_inode);

    if (bpt_insert(&temp_ctx, &bpt_root_block, &test_key, test_file_inode) != 0) {
        fprintf(stderr, "Error: Failed to insert test key into B+ Tree.\n");
        fclose(disk);
        return 1;
    }
    printf("B+ Tree insertion successful. Root is now at block %u.\n", bpt_root_block);

    printf("Writing Superblock...\n");
    Superblock sb;
    memset(&sb, 0, sizeof(Superblock));
    sb.magic = IBFS_MAGIC_NUMBER;
    sb.version = 1;
    sb.block_size = BLOCK_SIZE;
    sb.block_count = DISK_BLOCKS;
    sb.inode_count = INODE_COUNT;
    sb.root_inode = root_inode_num;
    sb.root_bpt_block = bpt_root_block;
    
    if (fseek(disk, 0, SEEK_SET) != 0) {
        perror("Error seeking to start to write superblock");
        fclose(disk);
        return 1;
    }
    if (fwrite(&sb, sizeof(Superblock), 1, disk) != 1) {
       perror("Error writing superblock");
       fclose(disk);
       return 1;
    }

    printf("Disk '%s' created and formatted successfully.\n", filename);
    fclose(disk);
    return 0;
}