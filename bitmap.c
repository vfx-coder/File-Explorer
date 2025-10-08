#include"bitmap.h"
#include"io.h"
#include<stdio.h>
#define inode_bitmap_block 1 //inode bitmpa is in block 1
int alloc_inode_num(ibfs_context* text)
{
  char block_buffer[Block_Size];
  if(read_block(text,inode_bitmap_block,block_buffer)!=0)
  {
    fprintf(stderr,"Failed to read inode bitmap block\n");
    return -1;
  }
  for(uint32_t i=0;i<text->sb.inode_count;++i)
  {
    uint32_t byte_index=i/8;
    uint32_t bit_index=i%8;

  if(!((block_buffer[byte_index]>>bit_index)&1))
  {
    block_buffer[byte_index] |=(1<<bit_index);
    if(write_block(text,inode_bitmap_block,block_buffer)){
    fprintf(stderr,"failed to write upadted inode bitmap block");
    return -1 ;}
    return i;
  }
    
  }
  fprintf(stderr,"Error: No free inodes available.\n");
  return -1;
}
void free_inode_num(ibfs_context* text,uint32_t inode_num){
  char block_buffer[Block_Size];
  if(read_block(text,inode_bitmap_block,block_buffer)!=0)
  {
    fprintf(stderr,"Failed to read inode bitmap block for free\n");
    return ;
  }
  uint32_t byte_index=inode_num/8;
  uint32_t bit_index=inode_num%8;
  block_buffer[byte_index]&=~(1<< bit_index);
  if(write_block(text,inode_bitmap_block,block_buffer))
}