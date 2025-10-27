#pragma once
#include "ibfs.h"

int read_block(IBFS_Context* ctx, uint32_t block_num, void* buffer);
int write_block(IBFS_Context* ctx, uint32_t block_num, const void* buffer);