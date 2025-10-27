#pragma once
#include "ibfs.h"

#define S_IFDIR 0040000 

int inode_alloc(IBFS_Context* ctx, uint16_t mode);
int inode_write(IBFS_Context* ctx, uint32_t inode_num, const Inode* inode_data);
int inode_read(IBFS_Context* ctx, uint32_t inode_num, Inode* inode_data);