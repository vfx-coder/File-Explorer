#include<ibfs.h>
int read_block(ibfs_context* text,uint32_t block_num,void *buffer);
int write_block(ibfs_context* text,uint32_t block_num,void* buffer);
