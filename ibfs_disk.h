#include<stdint.h>
#define Block_Size 4096
typedef struct superblock{
  uint32_t magic; //unique no. to identify our files
  uint32_t version; //version of filesystem format
  uint32_t block_size;//size of eachh block
  uint32_t inode_count; //total number of inodes available 
  uint32_t block_count; //total number of blocks in filesystem
  uint32_t root_inode; //the inode number for root directory 
}superblock;
typedef struct Inode //All metdata for a single file or directory 
{uint16_t mode;
  uint16_t links_count;
  uint64_t size;
  uint32_t direct_blocks[12];
  uint32_t single_indirect;
}Inode;