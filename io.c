#include "io.h"
int read_block(ibfs_context* text, uint32_t block_num,void* buffer)
{
  if(fseek(text->disk_file,block_num* Block_Size,SEEK_SET)!=1)
  {
    printf("Failed to read block");
    return -1;
  }
  return 0;
}
int write_block(ibfs_context *text,uint32_t block_num,void *buffer)
{
  if(fseek(text->disk_file,block_num* Block_Size,SEEK_SET)!=1)
  {
    printf("Failed to write block");
    return -1;
  }
  return 0;
}