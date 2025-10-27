#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>   
#include <stdbool.h>
#include "ibfs.h"
#include "inode.h"
#include "bplustree.h"
#include "block.h"   
#include "bitmap.h"  

static void print_entry_callback(BPlusTreeKey* key, uint32_t value, void* user_data);
static int ibfs_mkdir(IBFS_Context* ctx, uint32_t parent_inode_num, const char* name);
static int ibfs_rmdir(IBFS_Context* ctx, uint32_t parent_inode_num, const char* name);
static int ibfs_rm(IBFS_Context* ctx, uint32_t parent_inode_num, const char* name);
static bool is_directory_empty(IBFS_Context* ctx, uint32_t dir_inode_num);
int ibfs_mount(const char* disk_path, IBFS_Context* ctx);
void ibfs_unmount(IBFS_Context* ctx);


int ibfs_mount(const char* disk_path, IBFS_Context* ctx) {
    if (!disk_path || !ctx) return -1;
    ctx->disk_file = fopen(disk_path, "rb+");
    if (!ctx->disk_file) {
        perror("Error opening disk file");
        return -1;
    }
    if (fread(&ctx->sb, sizeof(Superblock), 1, ctx->disk_file) != 1) {
        fprintf(stderr, "Error: could not read superblock.\n");
        fclose(ctx->disk_file);
        return -1;
    }
    if (ctx->sb.magic != IBFS_MAGIC_NUMBER) {
        fprintf(stderr, "Error: Magic number mismatch. Not an IBFS disk?\n");
        fclose(ctx->disk_file);
        return -1;
    }
    if (ctx->sb.block_size != BLOCK_SIZE || ctx->sb.block_count == 0 || ctx->sb.inode_count == 0 || ctx->sb.root_inode >= ctx->sb.inode_count) {
         fprintf(stderr, "Error: Superblock contains invalid parameters.\n");
         fclose(ctx->disk_file);
         return -1;
    }
    fseek(ctx->disk_file, 0, SEEK_SET);
    return 0;
}

void ibfs_unmount(IBFS_Context* ctx) {
    if (ctx && ctx->disk_file) {
        fclose(ctx->disk_file);
        ctx->disk_file = NULL;
    }
}

static void print_entry_callback(BPlusTreeKey* key, uint32_t value, void* user_data) {
    IBFS_Context* ctx = (IBFS_Context*)user_data;
    Inode entry_inode;
    char time_buf[30];

    if (inode_read(ctx, value, &entry_inode) == 0) {
        struct tm *tm_info = localtime(&entry_inode.mtime);
        if (tm_info) {
            strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", tm_info);
        } else {
            strncpy(time_buf, "-------- -- --:--", sizeof(time_buf)-1);
            time_buf[sizeof(time_buf)-1] = '\0';
        }
        bool is_dir = (entry_inode.mode & S_IFDIR) == S_IFDIR;
        printf("%c %5u %10llu %s %s%s\n",
               is_dir ? 'd' : '-',
               entry_inode.links_count,
               (unsigned long long)entry_inode.size,
               time_buf,
               key->name,
               is_dir ? "/" : "");
    } else {
        printf("?       ?          ? ----- -- --:-- %s (Error reading inode %u)\n", key->name, value);
    }
}

static int ibfs_mkdir(IBFS_Context* ctx, uint32_t parent_inode_num, const char* name) {
    if (!name || strlen(name) == 0 || strlen(name) >= MAX_FILENAME_LENGTH || strcmp(name,".") == 0 || strcmp(name,"..") == 0) {
        fprintf(stderr, "mkdir Error: Invalid directory name '%s'.\n", name ? name : "");
        return -1;
    }

    BPlusTreeKey search_key;
    search_key.parent_inode_id = parent_inode_num;
    search_key.name_hash = hash_name(name);
    strncpy(search_key.name, name, MAX_FILENAME_LENGTH -1);
    search_key.name[MAX_FILENAME_LENGTH - 1] = '\0';
    uint32_t found_inode;
    if (bpt_search(ctx, ctx->sb.root_bpt_block, &search_key, &found_inode) == 0) {
        fprintf(stderr, "mkdir Error: '%s' already exists.\n", name);
        return -1;
    }

    printf("Allocating inode for '%s'...\n", name);
    int new_inode_num = inode_alloc(ctx, S_IFDIR);
    if (new_inode_num < 0) return -1;
    printf("Allocated inode %d.\n", new_inode_num);

    BPlusTreeKey new_key = search_key;

    printf("Inserting key into B+ Tree (parent=%u, name='%s', inode=%d)...\n",
           parent_inode_num, name, new_inode_num);
    uint32_t old_bpt_root = ctx->sb.root_bpt_block;
    if (bpt_insert(ctx, &ctx->sb.root_bpt_block, &new_key, new_inode_num) != 0) {
        fprintf(stderr, "mkdir Error: Failed to insert entry into B+ Tree.\n");
        free_inode_num(ctx, new_inode_num);
        return -1;
    }
    printf("B+ Tree insertion successful.\n");

    if (ctx->sb.root_bpt_block != old_bpt_root) {
        printf("B+ Tree root changed, updating superblock...\n");
        if (fseek(ctx->disk_file, 0, SEEK_SET) != 0) { perror("mkdir Error: Seek superblock"); return -1;}
        if (fwrite(&ctx->sb, sizeof(Superblock), 1, ctx->disk_file) != 1) { perror("mkdir Error: Write superblock"); return -1;}
        printf("Superblock updated on disk.\n");
    }
    return 0; 
}

typedef struct {
    bool is_empty;
} IsEmptyCheckData;

static void check_empty_callback(BPlusTreeKey* key, uint32_t value, void* user_data) {
    IsEmptyCheckData* data = (IsEmptyCheckData*)user_data;
    data->is_empty = false;
}

static bool is_directory_empty(IBFS_Context* ctx, uint32_t dir_inode_num) {
    IsEmptyCheckData check_data = { .is_empty = true };
    bpt_iterate(ctx, ctx->sb.root_bpt_block, dir_inode_num, check_empty_callback, &check_data);
    return check_data.is_empty;
}

static int ibfs_rmdir(IBFS_Context* ctx, uint32_t parent_inode_num, const char* name) {
    if (!name || strlen(name) == 0 || strlen(name) >= MAX_FILENAME_LENGTH || strcmp(name,".") == 0 || strcmp(name,"..") == 0) {
        fprintf(stderr, "rmdir Error: Invalid directory name '%s'.\n", name ? name : "");
        return -1;
    }

    BPlusTreeKey search_key;
    search_key.parent_inode_id = parent_inode_num;
    search_key.name_hash = hash_name(name);
    strncpy(search_key.name, name, MAX_FILENAME_LENGTH - 1);
    search_key.name[MAX_FILENAME_LENGTH - 1] = '\0';
    uint32_t target_inode_num;

    if (bpt_search(ctx, ctx->sb.root_bpt_block, &search_key, &target_inode_num) != 0) {
        fprintf(stderr, "rmdir Error: Directory '%s' not found.\n", name);
        return -1;
    }

    Inode target_inode;
    if (inode_read(ctx, target_inode_num, &target_inode) != 0) {
        fprintf(stderr, "rmdir Error: Failed to read inode %u for '%s'.\n", target_inode_num, name);
        return -1;
    }
    if ((target_inode.mode & S_IFDIR) != S_IFDIR) {
        fprintf(stderr, "rmdir Error: '%s' is not a directory.\n", name);
        return -1;
    }

    printf("Checking if directory '%s' (inode %u) is empty...\n", name, target_inode_num);
    if (!is_directory_empty(ctx, target_inode_num)) {
        fprintf(stderr, "rmdir Error: Directory '%s' is not empty.\n", name);
        return -1;
    }
    printf("Directory is empty.\n");

    printf("Deleting entry '%s' from parent inode %u...\n", name, parent_inode_num);
    uint32_t old_bpt_root = ctx->sb.root_bpt_block;
    if (bpt_delete(ctx, &ctx->sb.root_bpt_block, &search_key) != 0) {
        fprintf(stderr, "rmdir Error: Failed to delete entry from B+ Tree.\n");
        return -1;
    }
     printf("B+ Tree entry deleted.\n");

    if (ctx->sb.root_bpt_block != old_bpt_root) {
        printf("B+ Tree root changed, updating superblock...\n");
        if (fseek(ctx->disk_file, 0, SEEK_SET) != 0) { perror("rmdir Error: Seek superblock"); return -1;}
        if (fwrite(&ctx->sb, sizeof(Superblock), 1, ctx->disk_file) != 1) { perror("rmdir Error: Write superblock"); return -1;}
        printf("Superblock updated.\n");
    }

    printf("Freeing inode %u...\n", target_inode_num);
    free_inode_num(ctx, target_inode_num);

    return 0;
}

static int ibfs_rm(IBFS_Context* ctx, uint32_t parent_inode_num, const char* name) {
    if (!name || strlen(name) == 0 || strlen(name) >= MAX_FILENAME_LENGTH || strcmp(name,".") == 0 || strcmp(name,"..") == 0) {
        fprintf(stderr, "rm Error: Invalid file name '%s'.\n", name ? name : "");
        return -1;
    }

    BPlusTreeKey search_key;
    search_key.parent_inode_id = parent_inode_num;
    search_key.name_hash = hash_name(name);
    strncpy(search_key.name, name, MAX_FILENAME_LENGTH - 1);
    search_key.name[MAX_FILENAME_LENGTH - 1] = '\0';
    uint32_t target_inode_num;

    if (bpt_search(ctx, ctx->sb.root_bpt_block, &search_key, &target_inode_num) != 0) {
        fprintf(stderr, "rm Error: File '%s' not found.\n", name);
        return -1;
    }

    Inode target_inode;
    if (inode_read(ctx, target_inode_num, &target_inode) != 0) {
        fprintf(stderr, "rm Error: Failed to read inode %u for '%s'.\n", target_inode_num, name);
        return -1;
    }
    if ((target_inode.mode & S_IFDIR) == S_IFDIR) {
        fprintf(stderr, "rm Error: '%s' is a directory. Use rmdir.\n", name);
        return -1;
    }

    printf("Deleting entry '%s' from parent inode %u...\n", name, parent_inode_num);
    uint32_t old_bpt_root = ctx->sb.root_bpt_block;
    if (bpt_delete(ctx, &ctx->sb.root_bpt_block, &search_key) != 0) {
        fprintf(stderr, "rm Error: Failed to delete entry from B+ Tree.\n");
        return -1;
    }
     printf("B+ Tree entry deleted.\n");

    if (ctx->sb.root_bpt_block != old_bpt_root) {
        printf("B+ Tree root changed, updating superblock...\n");
        if (fseek(ctx->disk_file, 0, SEEK_SET) != 0) { perror("rm Error: Seek superblock"); return -1;}
        if (fwrite(&ctx->sb, sizeof(Superblock), 1, ctx->disk_file) != 1) { perror("rm Error: Write superblock"); return -1;}
        printf("Superblock updated.\n");
    }

    printf("Freeing data blocks for inode %u...\n", target_inode_num);
    for (int i = 0; i < 12; i++) {
        if (target_inode.direct_blocks[i] != 0) {
            printf("  - Freeing block %u\n", target_inode.direct_blocks[i]);
            free_data_block(ctx, target_inode.direct_blocks[i]);
        }
    }

    printf("Freeing inode %u...\n", target_inode_num);
    free_inode_num(ctx, target_inode_num);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <disk_filename> <command> [path]\n", argv[0]);
        fprintf(stderr, "Commands: ls, mkdir, rmdir, rm, test\n");
        return 1;
    }
    const char* disk_path = argv[1];
    const char* command = argv[2];
    const char* path_arg = (argc == 4) ? argv[3] : NULL;

    IBFS_Context ctx;
    memset(&ctx, 0, sizeof(IBFS_Context));

    if (ibfs_mount(disk_path, &ctx) != 0) {
        fprintf(stderr, "Failed to mount filesystem '%s'.\n", disk_path);
        return 1;
    }
    printf("File system '%s' mounted successfully.\n", disk_path);
    int result = 0;

    if (strcmp(command, "ls") == 0) {
        const char* ls_path = path_arg ? path_arg : "/";
        printf("--- Listing directory: %s ---\n", ls_path);
        uint32_t target_inode_num;
        if (strcmp(ls_path, "/") == 0) {
            target_inode_num = ctx.sb.root_inode;
        } else {
            fprintf(stderr, "Error: Path resolution not implemented. Only '/' supported for ls.\n");
            result = 1;
        }
        if (result == 0) {
            printf("Type Lnk      Size Mod Time        Name\n");
            printf("---- --- ---------- --------------- --------\n");
            if (bpt_iterate(&ctx, ctx.sb.root_bpt_block, target_inode_num, print_entry_callback, &ctx) != 0) {
                 result = 1;
            }
        }
        printf("--- ls Complete ---\n");

    } else if (strcmp(command, "mkdir") == 0) {
        if (!path_arg) { fprintf(stderr, "mkdir Error: Path argument required.\n"); result = 1; }
        else {
            printf("--- Attempting to create directory: %s ---\n", path_arg);
            if (path_arg[0] != '/' || strcmp(path_arg, "/") == 0 || strchr(path_arg + 1, '/') != NULL) {
                 fprintf(stderr, "Error: Invalid path. Only /dirname supported.\n");
                 result = 1;
             } else {
                const char* new_dir_name = path_arg + 1;
                if (ibfs_mkdir(&ctx, ctx.sb.root_inode, new_dir_name) != 0) {
                    result = 1;
                } else {
                    printf("Directory '%s' created successfully.\n", path_arg);
                }
            }
             printf("--- mkdir Complete ---\n");
        }

    } else if (strcmp(command, "rmdir") == 0) {
         if (!path_arg) { fprintf(stderr, "rmdir Error: Path argument required.\n"); result = 1; }
         else {
            printf("--- Attempting to remove directory: %s ---\n", path_arg);
            if (path_arg[0] != '/' || strcmp(path_arg, "/") == 0 || strchr(path_arg + 1, '/') != NULL) {
                 fprintf(stderr, "Error: Invalid path. Only /dirname supported.\n");
                 result = 1;
             } else {
                const char* dir_name = path_arg + 1;
                if (ibfs_rmdir(&ctx, ctx.sb.root_inode, dir_name) != 0) {
                    result = 1;
                }
            }
             printf("--- rmdir Complete ---\n");
         }

    } else if (strcmp(command, "rm") == 0) {
         if (!path_arg) { fprintf(stderr, "rm Error: Path argument required.\n"); result = 1; }
         else {
            printf("--- Attempting to remove file: %s ---\n", path_arg);
            if (path_arg[0] != '/' || strcmp(path_arg, "/") == 0 || strchr(path_arg + 1, '/') != NULL) {
                 fprintf(stderr, "Error: Invalid path. Only /filename supported.\n");
                 result = 1;
             } else {
                const char* file_name = path_arg + 1;
                if (ibfs_rm(&ctx, ctx.sb.root_inode, file_name) != 0) {
                    result = 1;
                } else {
                    printf("File '%s' removed successfully.\n", path_arg);
                }
            }
             printf("--- rm Complete ---\n");
         }

    } else if (strcmp(command, "test") == 0) {
        printf("--- Running B+ Tree Search Test ---\n");
        BPlusTreeKey search_key;
        search_key.parent_inode_id = ctx.sb.root_inode;
        search_key.name_hash = hash_name("readme.txt");
        strncpy(search_key.name, "readme.txt", MAX_FILENAME_LENGTH - 1);
        search_key.name[MAX_FILENAME_LENGTH - 1] = '\0';
        uint32_t found_inode;
        if (bpt_search(&ctx, ctx.sb.root_bpt_block, &search_key, &found_inode) == 0) {
             printf("TEST SUCCESS: Found 'readme.txt'! Mapped to inode %u\n", found_inode);
             if (found_inode != 1) printf("  - !!! ERROR: Expected inode 1 !!!\n");
        } else {
             printf("TEST FAILED: Could not find 'readme.txt'.\n");
        }
         printf("--- Test Complete ---\n");
    } else {
        fprintf(stderr, "Error: Unknown command '%s'.\n", command);
        result = 1;
    }

    ibfs_unmount(&ctx);
    printf("Filesystem unmounted.\n");
    return result;
}