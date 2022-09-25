#pragma once
#include "param.h"
#include "inode.h"

// Write a new direcotry entry (name, inum) to the directory dp
// return 0 on success, return -1 on failed
// called inside op and ip->lock locked
int dirlink(struct inode* dp, char* name, uint inum);

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
// called inside op and ip->lock locked
struct inode* dirlookup(struct inode* dp, char* name, uint* poff);

struct inode* path2inode(char* path);

struct inode* path2parentinode(char* path, char* name);
