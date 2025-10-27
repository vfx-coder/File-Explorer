#include "bplustree.h"
#include "io.h"
#include "block.h" 
#include <string.h> 
#include <stdio.h>  
#include <stdbool.h>
#include <stdlib.h> 

static void bpt_insert_into_internal(BPlusTreeNode* internal_node, BPlusTreeKey* key, uint32_t child_block_num);
static int compare_keys(BPlusTreeKey* key1, BPlusTreeKey* key2);
static void bpt_insert_into_leaf(BPlusTreeNode* leaf, BPlusTreeKey* key, uint32_t value);
static int bpt_insert_internal(IBFS_Context* ctx, uint32_t current_block_num, BPlusTreeKey* key, uint32_t value, BPlusTreeKey* promoted_key_out, uint32_t* promoted_child_out);
static uint32_t find_first_leaf_for_parent(IBFS_Context* ctx, uint32_t root_block_num, uint32_t target_parent_inode_id);
static int bpt_delete_internal(IBFS_Context* ctx, uint32_t current_block_num, BPlusTreeKey* key, bool* root_needs_update);


uint32_t hash_name(const char* name) {
    uint32_t hash = 5381;
    int c;
    if (!name) return 0;
    while ((c = *name++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static int compare_keys(BPlusTreeKey* key1, BPlusTreeKey* key2) {
    if (!key1 || !key2) return 0;
    if (key1->parent_inode_id < key2->parent_inode_id) return -1;
    if (key1->parent_inode_id > key2->parent_inode_id) return 1;
    if (key1->name_hash < key2->name_hash) return -1;
    if (key1->name_hash > key2->name_hash) return 1;
    int name_cmp = strncmp(key1->name, key2->name, MAX_FILENAME_LENGTH);
    if (name_cmp < 0) return -1;
    if (name_cmp > 0) return 1;
    return 0;
}

int bpt_search(IBFS_Context* ctx, uint32_t root_block_num, BPlusTreeKey* key, uint32_t* value_out) {
    if (root_block_num == 0) return -1;
    if (!ctx || !key || !value_out) return -1;

    char block_buffer[BLOCK_SIZE];
    BPlusTreeNode* node = (BPlusTreeNode*)block_buffer;
    uint32_t current_block_num = root_block_num;

    while (true) {
        if (read_block(ctx, current_block_num, block_buffer) != 0) {
            fprintf(stderr, "bpt_search: Failed to read block %u\n", current_block_num);
            return -1;
        }
        if (node->num_keys > BPTREE_ORDER) {
            fprintf(stderr, "bpt_search: Corrupt node %u, num_keys=%u\n", current_block_num, node->num_keys);
            return -1;
        }
        if (node->is_leaf) break;

        bool found_path = false;
        for (int i = 0; i < node->num_keys; i++) {
            if (compare_keys(key, &node->keys[i]) == -1) {
                current_block_num = node->children[i];
                found_path = true;
                break;
            }
        }
        if (!found_path) {
            current_block_num = node->children[node->num_keys];
        }
    }

    for (int i = 0; i < node->num_keys; i++) {
        if (compare_keys(key, &node->keys[i]) == 0) {
            *value_out = node->children[i];
            return 0;
        }
        if (compare_keys(key, &node->keys[i]) == -1) {
            break;
        }
    }
    return -1;
}

int bpt_insert(IBFS_Context* ctx, uint32_t* root_block_num_ptr, BPlusTreeKey* key, uint32_t value) {
    if (!ctx || !root_block_num_ptr || !key) return -1;

    if (*root_block_num_ptr == 0) {
        uint32_t new_root_block = alloc_data_block(ctx);
        if (new_root_block == 0) {
            fprintf(stderr, "bpt_insert: Failed to allocate block for new root\n");
            return -1;
        }
        char block_buffer[BLOCK_SIZE];
        BPlusTreeNode* root_node = (BPlusTreeNode*)block_buffer;
        memset(root_node, 0, BLOCK_SIZE);
        root_node->is_leaf = 1;
        root_node->num_keys = 1;
        root_node->keys[0] = *key;
        root_node->children[0] = value;
        root_node->next_leaf_block = 0;

        if (write_block(ctx, new_root_block, root_node) != 0) {
            fprintf(stderr, "bpt_insert: Failed to write new root block\n");
            free_data_block(ctx, new_root_block);
            return -1;
        }
        *root_block_num_ptr = new_root_block;
        return 0;
    }

    BPlusTreeKey promoted_key;
    uint32_t promoted_child_block_num;
    int split = bpt_insert_internal(ctx, *root_block_num_ptr, key, value, &promoted_key, &promoted_child_block_num);

    if (split == -1) return -1;

    if (split == 1) {
        uint32_t new_root_block = alloc_data_block(ctx);
        if (new_root_block == 0) {
             fprintf(stderr, "bpt_insert: Failed to allocate new root after split\n");
             return -1;
        }
        char block_buffer[BLOCK_SIZE];
        BPlusTreeNode* new_root = (BPlusTreeNode*)block_buffer;
        memset(new_root, 0, BLOCK_SIZE);
        new_root->is_leaf = 0;
        new_root->num_keys = 1;
        new_root->keys[0] = promoted_key;
        new_root->children[0] = *root_block_num_ptr; 
        new_root->children[1] = promoted_child_block_num; 

        if (write_block(ctx, new_root_block, new_root) != 0) {
             fprintf(stderr, "bpt_insert: Failed to write new root after split\n");
             free_data_block(ctx, new_root_block);
             return -1;
        }
        *root_block_num_ptr = new_root_block; 
    }
    return 0; 
}

static void bpt_insert_into_leaf(BPlusTreeNode* leaf, BPlusTreeKey* key, uint32_t value) {
    int i;
    for (i = 0; i < leaf->num_keys; i++) {
        if (compare_keys(key, &leaf->keys[i]) == -1) break;
    }
    memmove(&leaf->keys[i + 1], &leaf->keys[i], (leaf->num_keys - i) * sizeof(BPlusTreeKey));
    memmove(&leaf->children[i + 1], &leaf->children[i], (leaf->num_keys - i) * sizeof(uint32_t));
    leaf->keys[i] = *key;
    leaf->children[i] = value;
    leaf->num_keys++;
}

static int bpt_insert_internal(IBFS_Context* ctx, uint32_t current_block_num, BPlusTreeKey* key, uint32_t value, BPlusTreeKey* promoted_key_out, uint32_t* promoted_child_out) {
    static BPlusTreeKey temp_keys[BPTREE_ORDER + 1];
    static uint32_t temp_children[BPTREE_ORDER + 2];

    char block_buffer[BLOCK_SIZE];
    BPlusTreeNode* node = (BPlusTreeNode*)block_buffer;
    if (read_block(ctx, current_block_num, node) != 0) return -1;

    if (node->num_keys > BPTREE_ORDER) { 
        fprintf(stderr, "bpt_insert_internal: Corrupt node %u, num_keys=%u\n", current_block_num, node->num_keys);
        return -1;
    }

    if (node->is_leaf) {
        if (node->num_keys < BPTREE_ORDER) {
            bpt_insert_into_leaf(node, key, value);
            return write_block(ctx, current_block_num, node) == 0 ? 0 : -1;
        }
        else {
            uint32_t new_leaf_block_num = alloc_data_block(ctx);
            if (new_leaf_block_num == 0) return -1;
            char new_leaf_buffer[BLOCK_SIZE];
            BPlusTreeNode* new_leaf = (BPlusTreeNode*)new_leaf_buffer;
            memset(new_leaf, 0, BLOCK_SIZE);
            new_leaf->is_leaf = 1;

            memcpy(temp_keys, node->keys, node->num_keys * sizeof(BPlusTreeKey));
            memcpy(temp_children, node->children, node->num_keys * sizeof(uint32_t));
            int i;
            for (i = 0; i < node->num_keys; i++) {
                if (compare_keys(key, &temp_keys[i]) == -1) break;
            }
            memmove(&temp_keys[i + 1], &temp_keys[i], (node->num_keys - i) * sizeof(BPlusTreeKey));
            memmove(&temp_children[i + 1], &temp_children[i], (node->num_keys - i) * sizeof(uint32_t));
            temp_keys[i] = *key;
            temp_children[i] = value;
            int total_keys = node->num_keys + 1;

            int split_point = (total_keys + 1) / 2;
            memset(node->keys, 0, BPTREE_ORDER * sizeof(BPlusTreeKey));
            memset(node->children, 0, (BPTREE_ORDER + 1) * sizeof(uint32_t));

            memcpy(node->keys, temp_keys, split_point * sizeof(BPlusTreeKey));
            memcpy(node->children, temp_children, split_point * sizeof(uint32_t));
            node->num_keys = split_point;

            memcpy(new_leaf->keys, &temp_keys[split_point], (total_keys - split_point) * sizeof(BPlusTreeKey));
            memcpy(new_leaf->children, &temp_children[split_point], (total_keys - split_point) * sizeof(uint32_t));
            new_leaf->num_keys = total_keys - split_point;

            new_leaf->next_leaf_block = node->next_leaf_block;
            node->next_leaf_block = new_leaf_block_num;

            if (write_block(ctx, current_block_num, node) != 0) return -1;
            if (write_block(ctx, new_leaf_block_num, new_leaf) != 0) return -1;

            *promoted_key_out = new_leaf->keys[0];
            *promoted_child_out = new_leaf_block_num;
            return 1;
        }
    }
    else {
        int child_index = 0;
        for (child_index = 0; child_index < node->num_keys; child_index++) {
            if (compare_keys(key, &node->keys[child_index]) == -1) break;
        }
        uint32_t child_block_num = node->children[child_index];

        int split = bpt_insert_internal(ctx, child_block_num, key, value, promoted_key_out, promoted_child_out);

        if (split == -1) return -1;
        if (split == 0) return 0;

        if (node->num_keys < BPTREE_ORDER) {
            bpt_insert_into_internal(node, promoted_key_out, *promoted_child_out);
            return write_block(ctx, current_block_num, node) == 0 ? 0 : -1;
        }
        else { 
            uint32_t new_internal_block_num = alloc_data_block(ctx);
            if (new_internal_block_num == 0) return -1;
            char new_node_buffer[BLOCK_SIZE];
            BPlusTreeNode* new_node = (BPlusTreeNode*)new_node_buffer;
            memset(new_node, 0, BLOCK_SIZE);
            new_node->is_leaf = 0;

            memcpy(temp_keys, node->keys, node->num_keys * sizeof(BPlusTreeKey));
            memcpy(temp_children, node->children, (node->num_keys + 1) * sizeof(uint32_t));
            int i;
            for (i = 0; i < node->num_keys; i++) {
                if (compare_keys(promoted_key_out, &temp_keys[i]) == -1) break;
            }
            memmove(&temp_keys[i + 1], &temp_keys[i], (node->num_keys - i) * sizeof(BPlusTreeKey));
            memmove(&temp_children[i + 2], &temp_children[i + 1], (node->num_keys - i) * sizeof(uint32_t));
            temp_keys[i] = *promoted_key_out;
            temp_children[i + 1] = *promoted_child_out;
            int total_keys = node->num_keys + 1;

            int split_point = total_keys / 2;
            BPlusTreeKey key_to_promote = temp_keys[split_point];

            memset(node->keys, 0, BPTREE_ORDER * sizeof(BPlusTreeKey));
            memset(node->children, 0, (BPTREE_ORDER + 1) * sizeof(uint32_t));

            memcpy(node->keys, temp_keys, split_point * sizeof(BPlusTreeKey));
            memcpy(node->children, temp_children, (split_point + 1) * sizeof(uint32_t));
            node->num_keys = split_point;

            memcpy(new_node->keys, &temp_keys[split_point + 1], (total_keys - split_point - 1) * sizeof(BPlusTreeKey));
            memcpy(new_node->children, &temp_children[split_point + 1], (total_keys - split_point) * sizeof(uint32_t));
            new_node->num_keys = total_keys - split_point - 1;

            if (write_block(ctx, current_block_num, node) != 0) return -1;
            if (write_block(ctx, new_internal_block_num, new_node) != 0) return -1;

            *promoted_key_out = key_to_promote;
            *promoted_child_out = new_internal_block_num;
            return 1;
        }
    }
}

static void bpt_insert_into_internal(BPlusTreeNode* internal_node, BPlusTreeKey* key, uint32_t child_block_num) {
    int i;
    for (i = 0; i < internal_node->num_keys; i++) {
        if (compare_keys(key, &internal_node->keys[i]) == -1) break;
    }
    memmove(&internal_node->keys[i + 1], &internal_node->keys[i], (internal_node->num_keys - i) * sizeof(BPlusTreeKey));
    memmove(&internal_node->children[i + 2], &internal_node->children[i + 1], (internal_node->num_keys - i) * sizeof(uint32_t));
    internal_node->keys[i] = *key;
    internal_node->children[i + 1] = child_block_num;
    internal_node->num_keys++;
}

int bpt_delete(IBFS_Context* ctx, uint32_t* root_block_num_ptr, BPlusTreeKey* key) {
    if (!ctx || !root_block_num_ptr || *root_block_num_ptr == 0 || !key) {
        fprintf(stderr, "bpt_delete: Invalid arguments or empty tree.\n");
        return -1;
    }

    bool root_needs_update = false; 
    int result = bpt_delete_internal(ctx, *root_block_num_ptr, key, &root_needs_update);

    if (result == -1) {
        return -1; 
    }

    if (root_needs_update) {
        char block_buffer[BLOCK_SIZE];
        BPlusTreeNode* root_node = (BPlusTreeNode*)block_buffer;
        if (read_block(ctx, *root_block_num_ptr, block_buffer) != 0) {
            fprintf(stderr, "bpt_delete: Failed to read root node after potential merge.\n");
            return -1; 
        }

        if (!root_node->is_leaf && root_node->num_keys == 0) {
            uint32_t old_root_block = *root_block_num_ptr;
            *root_block_num_ptr = root_node->children[0];
            printf("B+ Tree root changed due to merge: %u -> %u\n", old_root_block, *root_block_num_ptr);
            free_data_block(ctx, old_root_block); 
        }
        else if (root_node->is_leaf && root_node->num_keys == 0) {
             printf("B+ Tree is now empty. Freeing root leaf %u.\n", *root_block_num_ptr);
             free_data_block(ctx, *root_block_num_ptr);
             *root_block_num_ptr = 0; 
        }
    }

    return 0; 
}

static int bpt_delete_internal(IBFS_Context* ctx, uint32_t current_block_num, BPlusTreeKey* key, bool* root_needs_update) {

    char block_buffer[BLOCK_SIZE];
    BPlusTreeNode* node = (BPlusTreeNode*)block_buffer;
    int key_index = -1;
    int child_descend_index = 0;

    if (read_block(ctx, current_block_num, block_buffer) != 0) {
        fprintf(stderr, "bpt_delete_internal: Failed read block %u\n", current_block_num);
        return -1;
    }
     if (node->num_keys > BPTREE_ORDER) { 
        fprintf(stderr, "bpt_delete_internal: Corrupt node %u, num_keys=%u\n", current_block_num, node->num_keys);
        return -1;
    }


    for (int i = 0; i < node->num_keys; i++) {
        int cmp = compare_keys(key, &node->keys[i]);
        if (cmp == 0) {
            key_index = i;
            child_descend_index = i + 1;
            break;
        } else if (cmp == -1) {
            child_descend_index = i;
            break;
        }
        child_descend_index = i + 1;
    }

    if (node->is_leaf) {
        if (key_index == -1) {
            fprintf(stderr, "bpt_delete_internal: Key not found in leaf %u.\n", current_block_num);
            return -1; 
        }

        printf("Deleting key '%s' from leaf node %u at index %d\n", key->name, current_block_num, key_index);
        memmove(&node->keys[key_index], &node->keys[key_index + 1], (node->num_keys - key_index - 1) * sizeof(BPlusTreeKey));
        memmove(&node->children[key_index], &node->children[key_index + 1], (node->num_keys - key_index - 1) * sizeof(uint32_t));
        node->num_keys--;
        memset(&node->keys[node->num_keys], 0, sizeof(BPlusTreeKey));
        memset(&node->children[node->num_keys], 0, sizeof(uint32_t));

        if (write_block(ctx, current_block_num, node) != 0) return -1;

        bool is_root = (current_block_num == ctx->sb.root_bpt_block);
        int min_leaf_keys = is_root ? 0 : (BPTREE_ORDER + 1) / 2;

        if (node->num_keys < min_leaf_keys) {
            fprintf(stderr, "Warning: Leaf node %u underflowed (%u keys, min %d). Merge/redistribute needed!\n",
                    current_block_num, node->num_keys, min_leaf_keys);
             *root_needs_update = true; 
            return 1; 
        }
        return 0; 

    } else {
        uint32_t child_block_num = node->children[child_descend_index];
        int child_result = bpt_delete_internal(ctx, child_block_num, key, root_needs_update);

        if (child_result == -1) return -1;
        if (child_result == 0) return 0;  
        if (child_result == 1) {
            fprintf(stderr, "Warning: Child node %u underflowed. Need to handle at parent %u. Not implemented!\n",
                    child_block_num, current_block_num);
            *root_needs_update = true; 
             int min_internal_keys = BPTREE_ORDER / 2; 
             if (node->num_keys < min_internal_keys) {
                 return 1; 
             } else {
                 return 0;
             }
        }
    }
    fprintf(stderr, "bpt_delete_internal: Unexpected state.\n");
    return -1; 
}


static uint32_t find_first_leaf_for_parent(IBFS_Context* ctx, uint32_t root_block_num, uint32_t target_parent_inode_id) {
     if (root_block_num == 0) return 0;

    char block_buffer[BLOCK_SIZE];
    BPlusTreeNode* node = (BPlusTreeNode*)block_buffer;
    uint32_t current_block_num = root_block_num;

    while (true) {
        if (read_block(ctx, current_block_num, block_buffer) != 0) {
            fprintf(stderr, "find_first_leaf_for_parent: Failed to read block %u\n", current_block_num);
            return 0;
        }
        if (node->is_leaf) return current_block_num;

        int child_index = 0;
        for (child_index = 0; child_index < node->num_keys; child_index++) {
            if (node->keys[child_index].parent_inode_id >= target_parent_inode_id) {
                break;
            }
        }
        current_block_num = node->children[child_index];
    }
}

int bpt_iterate(IBFS_Context* ctx, uint32_t root_block_num,
                uint32_t target_parent_inode_id,
                void (*callback)(BPlusTreeKey* key, uint32_t value, void* user_data),
                void* user_data)
{
    uint32_t current_leaf_block = find_first_leaf_for_parent(ctx, root_block_num, target_parent_inode_id);
    if (current_leaf_block == 0) {
        return (root_block_num == 0) ? 0 : -1;
    }

    char block_buffer[BLOCK_SIZE];
    BPlusTreeNode* leaf = (BPlusTreeNode*)block_buffer;
    bool keep_iterating = true;

    while (keep_iterating && current_leaf_block != 0) {
        if (read_block(ctx, current_leaf_block, block_buffer) != 0) {
            fprintf(stderr, "bpt_iterate: Failed to read leaf block %u\n", current_leaf_block);
            return -1;
        }
        if (!leaf->is_leaf) {
             fprintf(stderr, "bpt_iterate: Error - expected leaf node at block %u\n", current_leaf_block);
             return -1;
        }
        if (leaf->num_keys > BPTREE_ORDER) {
             fprintf(stderr, "bpt_iterate: Corrupt leaf node %u, num_keys=%u\n", current_leaf_block, leaf->num_keys);
             return -1;
        }


        for (int i = 0; i < leaf->num_keys; i++) {
            if (leaf->keys[i].parent_inode_id == target_parent_inode_id) {
                callback(&leaf->keys[i], leaf->children[i], user_data);
            } else if (leaf->keys[i].parent_inode_id > target_parent_inode_id) {
                keep_iterating = false;
                break;
            }
        }

        if (keep_iterating) {
            current_leaf_block = leaf->next_leaf_block;
        }
    }
    return 0;
}