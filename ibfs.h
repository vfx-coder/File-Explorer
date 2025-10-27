#pragma once
#include "ibfs_disk.h"
#include <stdio.h>

typedef struct IBFS_Context {
    FILE* disk_file;
    Superblock sb;
} IBFS_Context;

int ibfs_mount(const char* disk_path, IBFS_Context* ctx);
void ibfs_unmount(IBFS_Context* ctx);