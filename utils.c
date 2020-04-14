#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include "ext2.h"

uint8_t *getDisk(char *path) {
  int fd = open(path, O_RDWR);
  uint8_t *disk =
      mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (disk == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }
  return disk;
}

struct ext2_super_block *getSuperblock(uint8_t *disk) {
  struct ext2_super_block *superblock =
      (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
  return superblock;
}

uint8_t *getBlockFromBlockNum(uint8_t *disk, int blockNum) {
  return disk + blockNum * EXT2_BLOCK_SIZE;
}

struct ext2_group_desc *getGroupDescriptorTable(uint8_t *disk) {
  // https://www.nongnu.org/ext2-doc/ext2.html
  // The block group descriptor table starts on the first block following the
  // superblock. This would be the third block on a 1KiB block file system, or
  // the second block for 2KiB and larger block file systems. Shadow copies of
  // the block group descriptor table are also stored with every copy of the
  // superblock.
  int blockNum = 2;
  uint8_t *groupDescriptorTable = getBlockFromBlockNum(disk, blockNum);
  return (struct ext2_group_desc *)groupDescriptorTable;
}

struct ext2_inode *getInodeTable(uint8_t *disk) {
  struct ext2_group_desc *groupDescriptorTable = getGroupDescriptorTable(disk);
  unsigned int inodeTableBlockNum = groupDescriptorTable->bg_inode_table;
  uint8_t *inodeTable = getBlockFromBlockNum(disk, inodeTableBlockNum);
  return (struct ext2_inode *)inodeTable;
}

struct ext2_inode *getInodeFromInodeIndex(uint8_t *disk,
                                          unsigned int inodeIndex) {
  struct ext2_inode *inodeTable = getInodeTable(disk);
  struct ext2_inode *inode = inodeTable + inodeIndex;
  return inode;
}

struct ext2_inode *getInodeFromInodeNum(uint8_t *disk, unsigned int inodeNum) {
  return getInodeFromInodeIndex(disk, inodeNum - 1);
}

void printFileNameFromDirEntry(struct ext2_dir_entry_2 *entry) {
  unsigned int nameLen = entry->name_len;
  printf("%.*s\n", nameLen, entry->name);
}

bool isDotDir(struct ext2_dir_entry_2 *entry) {
  unsigned int len = entry->name_len;
  char *name = entry->name;
  return (len == 1 && (strncmp(name, ".", 1) == 0)) ||
         (len == 2 && (strncmp(name, "..", 2) == 0));
}

bool isDotDirOrLostAndFound(struct ext2_dir_entry_2 *entry) {
  char lostAndFound[20] = "lost+found";
  int len = strlen(lostAndFound);
  return isDotDir(entry) || (entry->name_len == len &&
                             strncmp(entry->name, lostAndFound, len) == 0);
}

bool doesEntryMatchName(struct ext2_dir_entry_2 *entry, char *name) {
  return entry->inode != 0 && strlen(name) == entry->name_len &&
         strncmp(entry->name, name, entry->name_len) == 0;
}

// find the entry of given name within a block
// keep in mind that if the block is not a hit, NULL will be returned
struct ext2_dir_entry_2 *findDirEntryFromBlockNum(uint8_t *disk,
                                                  unsigned int blockNum,
                                                  char *dirOrFileName, int op) {
  uint8_t *block = getBlockFromBlockNum(disk, blockNum);
  uint8_t *cursor = block;
  struct ext2_dir_entry_2 *cursorEntry;
  struct ext2_dir_entry_2 *prevCursorEntry = NULL;

  while (cursor < block + EXT2_BLOCK_SIZE) {
    cursorEntry = (struct ext2_dir_entry_2 *)cursor;
    // TODO: change to switch
    if (op == OP_PRINT_ALL) {
      printFileNameFromDirEntry(cursorEntry);
    } else if (op == OP_PRINT_NODOTS && !isDotDir(cursorEntry)) {
      printFileNameFromDirEntry(cursorEntry);
    } else if (op == OP_DELETE_REC && !isDotDirOrLostAndFound(cursorEntry)) {
      if (dirOrFileName == NULL ||
          doesEntryMatchName(cursorEntry, dirOrFileName)) {
        deleteAllFilesFromDirEntryRec(disk, cursorEntry, prevCursorEntry);
      }
    } else if (op == OP_DELETE_FILE &&
               doesEntryMatchName(cursorEntry, dirOrFileName)) {
      deleteDirEntry(disk, cursorEntry, prevCursorEntry);
    } else if (op == OP_FIND &&
               doesEntryMatchName(cursorEntry, dirOrFileName)) {
      return cursorEntry;
    }
    prevCursorEntry = cursorEntry;
    unsigned short entryLen = cursorEntry->rec_len;
    cursor += entryLen;
  }

  // no entry found within this block
  return NULL;
}

struct ext2_dir_entry_2 *findIndirectDirEntry(uint8_t *disk,
                                              unsigned int blockNum,
                                              char *dirOrFileName,
                                              int recursionCount, int op) {
  unsigned int *block = (unsigned int *)getBlockFromBlockNum(disk, blockNum);
  unsigned int maxPtrCount = EXT2_BLOCK_SIZE / sizeof(unsigned int);
  unsigned int i;
  unsigned int directBlockNum;
  for (i = 0; (i < maxPtrCount) && ((directBlockNum = block[i]) != 0); ++i) {
    struct ext2_dir_entry_2 *entry;
    if (recursionCount == 1) {
      entry = findDirEntryFromBlockNum(disk, directBlockNum, dirOrFileName, op);
    } else {
      entry = findIndirectDirEntry(disk, directBlockNum, dirOrFileName,
                                   recursionCount - 1, op);
    }
    if (entry != NULL) {
      return entry;
    }
  }

  // miss
  return NULL;
}

// trust me this is a state of art function
// use it, you will save troubles
struct ext2_dir_entry_2 *findSubDirEntry(uint8_t *disk,
                                         struct ext2_inode *inode,
                                         char *dirOrFileName, int op) {
  int i;
  for (i = 0; i < 15 && (inode->i_block)[i] != 0; i++) {
    unsigned int blockNum = inode->i_block[i];
    struct ext2_dir_entry_2 *entry;
    if (i < 12) {
      entry = findDirEntryFromBlockNum(disk, blockNum, dirOrFileName, op);
    } else {
      entry = findIndirectDirEntry(disk, blockNum, dirOrFileName, i - 11, op);
    }
    if (entry != NULL) {
      return entry;
    }
  }

  if (op == OP_FIND) {
    perror("entry not found");
    exit(ENOENT);
  }

  return NULL;
}

struct ext2_inode *getRootDirInode(uint8_t *disk) {
  int rootDirInodeIndex = 1;
  struct ext2_inode *rootInode =
      getInodeFromInodeIndex(disk, rootDirInodeIndex);
  return rootInode;
}

// get the dir entry for the given absolute path
// can be either dir or file
// path need to exist
struct ext2_dir_entry_2 *getDirEntryFromAbsolutePath(uint8_t *disk, char *path,
                                                     bool returnEnclosingDir) {
  if (path[0] != '/') {
    perror("Invalid dir path\n");
    exit(ENOENT);
  }

  int pathLen = strlen(path);
  char *pathCopy = malloc((pathLen + 1) * sizeof(char));
  strncpy(pathCopy, path, pathLen);
  // get the root
  struct ext2_inode *rootInode = getRootDirInode(disk);
  struct ext2_dir_entry_2 *rootDirEntry =
      findSubDirEntry(disk, rootInode, ".", OP_FIND);

  struct ext2_dir_entry_2 *cursor = rootDirEntry;
  struct ext2_dir_entry_2 *prevCursor;

  char *dirOrFileName = strtok(pathCopy, "/");

  while (dirOrFileName) {
    if (cursor->file_type != EXT2_FT_DIR) {
      free(pathCopy);
      perror("not a directory");
      exit(ENOTDIR);
    } else {
      unsigned int prevCursorInodeNum = cursor->inode;
      struct ext2_inode *prevCursorInode =
          getInodeFromInodeNum(disk, prevCursorInodeNum);

      prevCursor = cursor;
      cursor = findSubDirEntry(disk, prevCursorInode, dirOrFileName, OP_FIND);
      dirOrFileName = strtok(NULL, "/");
    }
  }

  free(pathCopy);

  if (returnEnclosingDir) {
    if (prevCursor != NULL) {
      return prevCursor;
    } else {
      perror("Root has no enclosing dir\n");
      exit(ENOENT);
    }
  }
  return cursor;
}

uint8_t *getInodeBitmap(uint8_t *disk) {
  struct ext2_group_desc *groupDescriptorTable = getGroupDescriptorTable(disk);
  unsigned int bitmapBlockNum = groupDescriptorTable->bg_inode_bitmap;
  uint8_t *inodeBitmap = getBlockFromBlockNum(disk, bitmapBlockNum);
  return inodeBitmap;
}

uint8_t *getBlockBitmap(uint8_t *disk) {
  struct ext2_group_desc *groupDescriptorTable = getGroupDescriptorTable(disk);
  unsigned int bitmapBlockNum = groupDescriptorTable->bg_block_bitmap;
  uint8_t *blockBitmap = getBlockFromBlockNum(disk, bitmapBlockNum);
  return blockBitmap;
}

// release inode or block from corresponding bitmap
// num is not index here
void releaseBitmap(uint8_t *bitmap, unsigned int num) {
  unsigned int index = num - 1;
  unsigned int byteLoc = index / 8;
  unsigned int bitLoc = index % 8;
  bitmap[byteLoc] &= (~(1 << bitLoc));
}

void releaseBlockFromBlockNum(uint8_t *disk, unsigned int blockNum) {
  uint8_t *bitmap = getBlockBitmap(disk);
  releaseBitmap(bitmap, blockNum);
  getGroupDescriptorTable(disk)->bg_free_blocks_count++;
  struct ext2_super_block *superblock = getSuperblock(disk);
  superblock->s_free_blocks_count++;
}

// recursionCount is a counter for recursion
// used to handle doubly and triply linked blocks
void releaseIndirectBlocks(uint8_t *disk, unsigned int blockNum,
                           int recursionCount) {
  // let's be careful here
  unsigned int *block = (unsigned int *)getBlockFromBlockNum(disk, blockNum);
  // first level indirect block
  unsigned int maxPtrCount = EXT2_BLOCK_SIZE / sizeof(unsigned int);

  unsigned int i;
  unsigned int directBlockNum;
  for (i = 0; i < maxPtrCount && ((directBlockNum = block[i]) != 0); i++) {
    if (recursionCount == 1) {
      releaseBlockFromBlockNum(disk, directBlockNum);
    } else {
      releaseIndirectBlocks(
          disk, directBlockNum,
          recursionCount - 1);  // dont use -- here, still need orig value
    }
    // free self
  }
  releaseBlockFromBlockNum(disk, blockNum);
}

void releaseBlocks(uint8_t *disk, struct ext2_inode *inode) {
  int i;
  unsigned int blockNum;
  for (i = 0; i < 15 && ((blockNum = inode->i_block[i]) != 0); i++) {
    if (i < 12) {
      // singly linked blocks
      releaseBlockFromBlockNum(disk, blockNum);
    } else {
      releaseIndirectBlocks(disk, blockNum, i - 11);  // recursion magic here
    }
  }
}

void releaseInodeFromInodeNum(uint8_t *disk, unsigned int inodeNum) {
  struct ext2_inode *inode = getInodeFromInodeNum(disk, inodeNum);
  inode->i_links_count--;

  if (inode->i_links_count == 0) {
    unsigned int timestamp = (unsigned int)time(NULL);
    inode->i_dtime = timestamp;
    // order is important here
    releaseBlocks(disk, inode);
    // free inode bitmap
    uint8_t *bitmap = getInodeBitmap(disk);
    releaseBitmap(bitmap, inodeNum);
    getGroupDescriptorTable(disk)->bg_free_inodes_count++;
    getSuperblock(disk)->s_free_inodes_count++;
  }
}

// use with great caution
void deleteDirEntry(uint8_t *disk, struct ext2_dir_entry_2 *entry,
                               struct ext2_dir_entry_2 *prevEntry) {
  releaseInodeFromInodeNum(disk, entry->inode);
  // reclaim dir entry
  // gotta be careful here!
  // Note from https://www.nongnu.org/ext2-doc/ext2.html#ifdir-rec-len
  // "Since this value cannot be negative, when a file is removed the
  // previous record within the block has to be modified to point to the
  // next valid record within the block or to the end of the block when
  // no other directory entry is present. If the first entry within the
  // block is removed, a blank record will be created and point to the
  // next directory entry or to the end of the block."

  if (prevEntry == NULL) {
    // first case, first entry within the block
    // create blank record and point to next dir entry
    // im not super sure this is the correct way to do this
    entry->inode = 0;  // mark the entry as blank
  } else {
    // second case, there is a prev entry
    // slightly easier here, mark the previous entry to cover this entry
    prevEntry->rec_len += entry->rec_len;
  }
  // and we are done!
  // }
}

void deleteAllFilesFromDirEntryRec(uint8_t *disk,
                                   struct ext2_dir_entry_2 *entry,
                                   struct ext2_dir_entry_2 *prevEntry) {
  struct ext2_inode *inode = getInodeFromInodeNum(disk, entry->inode);
  if (entry->file_type == EXT2_FT_DIR) {
    // inplicit recursion here
    findSubDirEntry(disk, inode, NULL, OP_DELETE_REC);
  }
  deleteDirEntry(disk, entry, prevEntry);
}

// delete just one file, doesnt take dir(s)!
void deleteFile(uint8_t *disk, struct ext2_dir_entry_2 *dirEntry,
                char *filename) {
  struct ext2_inode *inode = getInodeFromInodeNum(disk, dirEntry->inode);
  findSubDirEntry(disk, inode, filename, OP_DELETE_FILE);
}

void deleteFileFromFilePath(uint8_t *disk, char *filePath) {
  if (filePath[strlen(filePath) - 1] == '/') {
    fprintf(stderr, "%s is a directory\n", filePath);
    exit(EISDIR);
  }
  struct ext2_dir_entry_2 *enclosingDir =
      getDirEntryFromAbsolutePath(disk, filePath, true);
  char *filename = basename(filePath);
  deleteFile(disk, enclosingDir, filename);
}

void printContentsFromDirectory(uint8_t *disk, char *dirPath, bool printDots) {
  struct ext2_dir_entry_2 *entry =
      getDirEntryFromAbsolutePath(disk, dirPath, false);
  struct ext2_inode *inode = getInodeFromInodeNum(disk, entry->inode);
  int op = printDots ? OP_PRINT_ALL : OP_PRINT_NODOTS;
  findSubDirEntry(disk, inode, NULL, op);
}

/**
 * Create '.' and '..' for our new directory given its inode
 */
void createDirForNewDir(struct ext2_inode *dirInode, int parentInode,
                        struct ext2_dir_entry_2 *newDirent, int curInodenum,
                        uint8_t *disk) {
  char *curDirname = ".";
  char *prevDirname = "..";
  // each dirent must start at an address thats a multiple of 4
  int dirSize = sizeof(struct ext2_dir_entry_2);
  // so we need to adjust the rec_lec to satisfy
  int curRecLen = adjustReclen(dirSize + strlen(curDirname));
  // allocate next available block for our new dir
  int availableBlock = allocateBlock(disk);
  struct ext2_dir_entry_2 *curDir =
      (struct ext2_dir_entry_2 *)getBlockFromBlockNum(disk, availableBlock);
  // update the blocks of our new directory
  dirInode->i_block[0] = availableBlock;
  // update the structure of the "."
  // the inode num is the available inodenum we found
  curDir->inode = curInodenum;
  curDir->rec_len = curRecLen;
  curDir->name_len = strlen(curDirname);
  // both "." and ".." are directory
  curDir->file_type = EXT2_FT_DIR;
  strcpy(curDir->name, curDirname);
  // update the structure of the ".."
  struct ext2_dir_entry_2 *prevDir;
  prevDir = (struct ext2_dir_entry_2 *)getBlockFromBlockNumAndBlockPos(
      disk, availableBlock, curDir->rec_len);
  // note the inode num here is the inodenum
  // of the parent dir
  prevDir->inode = parentInode;
  // the rec_len of ".." is  1024 - the rec_len of the curr_dir
  prevDir->rec_len = 1024 - curDir->rec_len;
  prevDir->name_len = strlen(prevDirname);
  prevDir->file_type = EXT2_FT_DIR;
  strcpy(prevDir->name, prevDirname);
}

/*
    Similar to getDirEntryFromAbsolutePath(), instead of exit when no dir
   section found, return -1 and when the section is found return the inodenum of
   the section (i.e. find inodenum for level1 --> 12, if no such level1 exists
   --> -1)
 */
int findDirent(int inode, char *fileName, uint8_t *disk) {
  struct ext2_inode *curInode = getInodeFromInodeNum(disk, inode);
  struct ext2_dir_entry_2 *curDir;
  unsigned long blockPos = 0;
  char curDirName[EXT2_NAME_LEN + 1];
  for (int i = 0; i < 12; i++) {
    while (blockPos < EXT2_BLOCK_SIZE) {
      // get the entries of the current inode
      curDir = (struct ext2_dir_entry_2 *)getBlockFromBlockNumAndBlockPos(
          disk, curInode->i_block[i], blockPos);
      strcpy(curDirName, curDir->name);
      // check if the name matches with the given name
      if (strcmp(curDirName, fileName) == 0) {
        // return the inode num if the name matches
        return curDir->inode;
      }
      blockPos += curDir->rec_len;
    }
  }
  // if no matches found, return -1
  return -1;
}

/*
    Return a new dirent for our srcFile given the inodenum of the
    dirent we will be creating the new dirent at , and the entry name
 */
struct ext2_dir_entry_2 *createDirent(int inodeNum, char *entryName,
                                      uint8_t *disk) {
  struct ext2_inode *inode = getInodeFromInodeNum(disk, inodeNum);
  struct ext2_dir_entry_2 *prevDirent;
  struct ext2_dir_entry_2 *curDirent;
  // each dirent must start at an address thats a multiple of 4
  int dirSize = sizeof(struct ext2_dir_entry_2);
  // so we need to adjust the rec_lec to satisfy
  int actualLen = adjustReclen(dirSize + strlen(entryName));
  int i = 0;
  unsigned long blockPos = 0;
  // Loop through the target directory's blocks find a block
  // to create our new dirent
  while (i < 12 && inode->i_block[i]) {
    while (blockPos < EXT2_BLOCK_SIZE) {
      // get the current dirent
      curDirent = (struct ext2_dir_entry_2 *)getBlockFromBlockNumAndBlockPos(
          disk, inode->i_block[i], blockPos);
      // get the position of the next dirent
      blockPos += curDirent->rec_len;
    }
    prevDirent = curDirent;
    // record the rec_len of the dirent
    int prevDirentLen = adjustReclen(dirSize + prevDirent->name_len);
    // check if there are enough rec_len left for our new dirent in current
    // entry
    if (actualLen <= (prevDirent->rec_len - prevDirentLen)) {
      // get the next dirent by add the rec_len to current dirent
      curDirent = (struct ext2_dir_entry_2 *)((unsigned char *)curDirent +
                                              prevDirentLen);
      // Update therec_len for the current dirent
      curDirent->rec_len = prevDirent->rec_len - prevDirentLen;
      // Update therec_len for the previous dirent
      prevDirent->rec_len = prevDirentLen;
      break;
    }
    // if not we will go to the next dirent and start over
    // until we find a place for our new dirent
    else {
      i++;
    }
  }
  return curDirent;
}

/*
    Return the block given the block num and block position
 */
uint8_t *getBlockFromBlockNumAndBlockPos(uint8_t *disk, int blockNum,
                                         unsigned long blockPos) {
  return disk + blockNum * EXT2_BLOCK_SIZE + blockPos;
}

/*
    Returns the adjusted rec_len of dirent, which is rounded to the
    next multiple of 4
 */
int adjustReclen(int recLen) {
  if (recLen % 4 != 0) {
    recLen = 4 * ((recLen / 4) + 1);
  }
  return recLen;
}

/*
    Find the 1st avaliable block from the block bitmap
    return the block num
    note: if the availableBlock = 0, there are no available block
 */
unsigned int allocateBlock(uint8_t *disk) {
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  struct ext2_group_desc *bgd = getGroupDescriptorTable(disk);
  unsigned char *bm = getBlockBitmap(disk);
  int availableBlock = 0;
  int blockIndex;
  int index = 0;
  // go over all the blocks in the block bitmap
  // and find the first available block
  for (int i = 0; i < sb->s_blocks_count; i++) {
    unsigned c = bm[i / 8];
    // when the available bolck found
    // update the block num and break the loop
    if ((c & (1 << index)) == 0 && i > 10) {
      availableBlock = i + 1;
      break;
    }
    if (++index == 8) (index = 0);
  }
  // if availableBlock is not changed then there are no free blocks
  // i.e. all block are in use
  if (availableBlock == 0){
      fprintf(stderr, "No available inode found!\n");
      exit(ENOENT);
  }
  // Set the corresponding bit in block bitmap to 1
  blockIndex = availableBlock - 1;
  int byteIndex = blockIndex / 8;
  int bitOffset = blockIndex % 8;
  // mark the bit to 1 as we will be using it
  bm[byteIndex] |= 1 << bitOffset;
  // update the block counters
  sb->s_free_blocks_count--;
  bgd->bg_free_blocks_count--;
  return availableBlock;
}

/*
    Allocate the blocks needed for the content of the source file
    and update the correct number of blocks for inode
    given the inodenum of the target dir
 */
void allocateBlocksForFileContent(int inodeNum, uint8_t *disk) {
  struct ext2_inode *inode = getInodeFromInodeNum(disk, inodeNum);
  int *indirectBlocks = 0;
  int i = 0;
  // each time allocated a block, we increase the bytes_allocated by
  // EXT2_BLOCK_SIZE(1024)
  for (int bytesNeed = 0; bytesNeed < inode->i_size;
       bytesNeed += EXT2_BLOCK_SIZE) {
    // i_block 0-11 point directly to the first 12 data blocks of the file
    // which when i is less than 12 we allocate blocks normally
    if (i < 12) {
      // allocate direct blocks
      // update the i_block
      inode->i_block[i] = allocateBlock(disk);
      // update the i_blocks
      // each time the blocks increases by 2
      // 1024/512
      inode->i_blocks += 2;
      // go to the next direct block
      i++;
    } else if (i == 12) {
      // i_block 12 points to a single indirect block
      // so when i is 12, we allocate blocks pointing to the indirect block
      // if no indirect_blocks allocated we need to allocate one
      if (!indirectBlocks) {
        inode->i_block[i] = allocateBlock(disk);
        inode->i_blocks += 2;
        indirectBlocks = (int *)getBlockFromBlockNum(disk, inode->i_block[i]);
      }
      // allocate blocks pointing to the indirect block
      *indirectBlocks = allocateBlock(disk);
      inode->i_blocks += 2;
      indirectBlocks++;
    }
  }
}

/*
    Write the given file contents to the data blocks of the inode given
    the content and the inodenum
 */
void writeContent(int inodeNum, char *fileContents, uint8_t *disk) {
  struct ext2_inode *inode = getInodeFromInodeNum(disk, inodeNum);
  int contentSize = inode->i_size;
  int *indirectBlock = 0;
  int blocks = 0;
  unsigned char *curBlock;
  for (int bytesWritten = 0; bytesWritten < contentSize;
       bytesWritten += EXT2_BLOCK_SIZE) {
    if (blocks < 12) {
      // get the direct block we allocated from the target inode
      curBlock = getBlockFromBlockNum(disk, inode->i_block[blocks]);
      // go to the next direct block
      blocks++;
    } else if (blocks == 12) {
      // we need to use the i_block[12] to store the contents
      // larger than the 12 direct blocks
      if (!indirectBlock) {
        // get the indrect blocknum we allocated for the targte inode
        indirectBlock =
            (int *)getBlockFromBlockNum(disk, inode->i_block[blocks]);
      }
      indirectBlock++;
      // get the indirect block
      curBlock = getBlockFromBlockNum(disk, *indirectBlock);
    }
    // copy the content of size 1024 into the current block
    memmove(curBlock, fileContents + bytesWritten, EXT2_BLOCK_SIZE);
  }
}

void deleteDirFromPath(uint8_t *disk, char *path) {
  bool returnEnclosingDir = true;
  char* name;
  if (strlen(path) == 1 && strncmp(path, "/", 1) == 0) {
    returnEnclosingDir = false;
    name=NULL;
  } else {
    name = basename(path);
  }
  struct ext2_dir_entry_2 *entry =
      getDirEntryFromAbsolutePath(disk, path, returnEnclosingDir);
  unsigned int inodeNum = entry->inode;
  struct ext2_inode *inode = getInodeFromInodeNum(disk, inodeNum);
  findSubDirEntry(disk, inode, name, OP_DELETE_REC);
}

/*
    Similar to allocateBlock except this is finding an available inode
    Find the 1st avaliable inode from the inode bitmap
    return the inode num
    note: if the availableInode = 0, there are no available inode
 */
int allocateInode(uint8_t *disk) {
  // if no such dir exists, we can find an available inode
    struct ext2_group_desc *bgd = getGroupDescriptorTable(disk);
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    unsigned char *bmi = getInodeBitmap(disk);
    int availableInode = 0;
    int inodeIndex;
    int index = 0;
    // go over all the inodes in the inode bit map
    // and find the first available inode
    for (int i = 0; i < sb->s_inodes_count; i++) {
        unsigned c = bmi[i / 8];                     
        if ((c & (1 << index)) == 0 && i > 10) {    
            availableInode = i + 1;
            break;
        }
        if (++index == 8) (index = 0); 
    }
    // if availableInode is not changed then there are no free inode
    // i.e. all inodes are in use
    if (availableInode == 0){
        fprintf(stderr, "No available inode found!\n");
        exit(ENOENT);
    }
    // Set the corresponding bit in bitmap to 1
    // as we will use it to place the src file
    inodeIndex = availableInode - 1;
    int byteIndex = inodeIndex / 8;
    int bitOffset = inodeIndex % 8;
    bmi[byteIndex] |= 1 << bitOffset;
    // update the inode counters
    sb->s_free_inodes_count--;
    bgd->bg_free_inodes_count--;
    return availableInode;
}

/*
  Set the structure of the new dirent
 */
void setStructForDirent(struct ext2_dir_entry_2 *newDirent, int inodeNum, char *fileName, unsigned char type){
  newDirent->inode = inodeNum;
  newDirent->name_len = strlen(fileName);
  newDirent->file_type = type;
  strcpy(newDirent->name, fileName);
}

/*
  Set the structure of the inode
 */
void setStructForInode(struct ext2_inode *targetInode, unsigned short type, int inodeSize, int iBlocks, int linksCount){
  targetInode->i_mode = type;
  targetInode->i_size = inodeSize;
  targetInode->i_ctime = time(NULL);
  targetInode->i_atime = time(NULL);
  targetInode->i_mtime = time(NULL);
  targetInode->i_blocks = iBlocks;
  memset(targetInode->i_block, 0, 15 * sizeof(unsigned int));
  targetInode->i_links_count = linksCount;
}
