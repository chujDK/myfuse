#pragma once
#include "param.h"
#include "inode.h"

// Write a new direcotry entry (name, inum) to the directory dp
// return 0 on success, return -1 on failed
int dirlink(struct inode* dp, char* name, uint inum);

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode* dirlookup(struct inode* dp, char* name, uint* poff);
