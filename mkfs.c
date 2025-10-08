#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "ibfs_disk.h"
#ifdef _WIN32
#include<io.h>
#define ftruncate _chsize_s
#else 
  #include<unistd.h>
#endif
#define disk_blocks 4096
int main(int argc,char *argv[])
{
  if(argc!=2)
   {fprintf(stderr,"usage: %s \n",argv[0]);
   return 1;
  }
  FILE *disk=fopen(argv[1],"wb+");
  if(!disk)
  {printf("Error creating disk file");
  return 1;}
  if(ftruncate(fileno(disk),(long long)disk_blocks*Block_Size)!=0)
  {printf("Error setting disk size");
  fclose(disk);
  return 1;
  }
  superblock sb;
  memset(&sb,0,sizeof(superblock));
  sb.magic=0xDEADBEEF;
  sb.version=1;
  sb.block_size=Block_Size;
  sb.inode_count=1024;
  fseek(disk,0,SEEK_SET);
  fwrite(&sb,sizeof(superblock),1,disk);
  printf("Disk '%s' created \n",argv[1]);
  fclose(disk);
  return 0;
}