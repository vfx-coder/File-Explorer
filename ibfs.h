#include "ibfs_disk.h"
#include<stdio.h>
typedef struct ibfs_context{
  FILE* disk_file;
  superblock sb;
}ibfs_context;
int ibfs_mount(const char *disk_path,ibfs_context* text);
void ibfs_unmount(ibfs_context* text);
void print_fs_info(ibfs_context* text);

