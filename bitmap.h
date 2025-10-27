#pragma once
#include "ibfs.h"

int alloc_inode_num(IBFS_Context* ctx);
void free_inode_num(IBFS_Context* ctx, uint32_t inode_num);