#pragma once
#include "ibfs.h"

uint32_t alloc_data_block(IBFS_Context* ctx);
void free_data_block(IBFS_Context* ctx, uint32_t block_num);