#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "ibfs.h"
int ibfs_mount(const char *disk_path,ibfs_context *text)
{
  text->disk_file=fopen(disk_path,"rb");
  if(!text->disk_file)
  {
    printf("failed to open disk\n");
    return -1;
  }
  if(fread(&text->sb,sizeof(superblock),1,text->disk_file)!=1)
  {
    fprintf(stderr,"failed to read superblock.\n");
    fclose(text->disk_file);
    return -1;
  }
  if(text->sb.magic!=0xDEADBEEF)
  {
    fprintf(stderr,"Error:Not valid filesystem");
    fclose(text->disk_file);
    return -1;
  }
  printf("File system mounted successfully.\n");
  return 0;
}
void ibfs_unmount(ibfs_context* text)
{
  if(text && text->disk_file)
  {
    fclose(text->disk_file);
    printf("Filesystem unmounted.\n");
  }
}
void print_fs_info(ibfs_context* text)
{
  printf("-FileSystem Information-\n");
  printf("version: %u\n",text->sb.version);
  printf("Block Size: %u\n",text->sb.block_size);
  printf("Block Count: %u\n",text->sb.block_count);
  printf("Inode Size: %u\n",text->sb.inode_count);
  printf("Root Inode: %u\n",text->sb.root_inode);
}
int main(int argc,char *argv[])
{
  if(argc !=2)
  {fprintf(stderr,"Usage: %s\n",argv[0]);
  return 1;
  }
  ibfs_context text;
  if(ibfs_mount(argv[1],&text)!=0)
  {return 1;}
  print_fs_info(&text);
  ibfs_unmount(&text);
  return 0;
}