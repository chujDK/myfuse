#pragma once
#include <sys/types.h>

#define FSMAGIC 0x636a6673
#define BSIZE 1024

// Disk layout:
// [ boot block (skip) | super block | log | inode blocks |
//                                           free bit map | data blocks]
//
// mkfs will adjust the each section's size and store into the super block.
// as the super block describes the disk layout:
struct superblock {
  uint magic;       // Must be FSMAGIC
  uint size;        // Size of file system image (blocks)
  uint nblocks;     // Number of data blocks
  uint ninodes;     // Number of inodes.
  uint nlog;        // Number of log blocks
  uint logstart;    // Block number of first log block
  uint inodestart;  // Block number of first inode block
  uint bmapstart;   // Block number of first free map block
};

#define NDIRECT 10
#define NINDIRECT1 (BSIZE / sizeof(uint))
#define NINDIRECT2 ((BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint)))
#define NINDIRECT3 \
  ((BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint)))
#define NINDIRECT (NINDIRECT1 + NINDIRECT2 + NINDIRECT3)
#define NSUBDIRECT 3

#define NBLOCKADDR (NDIRECT + NSUBDIRECT)
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;              // File type
  short major;             // Major device number (T_DEVICE only)
  short minor;             // Minor device number (T_DEVICE only)
  short nlink;             // Number of links to inode in file system
  uint size;               // Size of file (bytes)
  uint addrs[NBLOCKADDR];  // Data block addresses  short type;
};

// Inodes per block.
#define IPB (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB (BSIZE * 8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures
struct dirent {
  ushort inum;
#define DIRSIZE (0x10 - sizeof(ushort))
  char name[DIRSIZE];
};