#pragma once
#include "ibfs.h" 

#define MAX_FILENAME_LENGTH 28

typedef struct BPlusTreeKey {
    uint32_t parent_inode_id;
    uint32_t name_hash;
    char name[MAX_FILENAME_LENGTH];
} BPlusTreeKey;

#define BPTREE_ORDER 102

typedef struct BPlusTreeNode {
    uint32_t is_leaf;
    uint32_t num_keys;
    BPlusTreeKey keys[BPTREE_ORDER];
    uint32_t children[BPTREE_ORDER + 1];
    uint32_t next_leaf_block; 
} BPlusTreeNode;

int bpt_search(IBFS_Context* ctx, uint32_t root_block_num, BPlusTreeKey* key, uint32_t* value_out);
int bpt_insert(IBFS_Context* ctx, uint32_t* root_block_num_ptr, BPlusTreeKey* key, uint32_t value);
int bpt_delete(IBFS_Context* ctx, uint32_t* root_block_num_ptr, BPlusTreeKey* key);
uint32_t hash_name(const char* name);
int bpt_iterate(IBFS_Context* ctx, uint32_t root_block_num,
                uint32_t target_parent_inode_id,
                void (*callback)(BPlusTreeKey* key, uint32_t value, void* user_data),
                void* user_data);