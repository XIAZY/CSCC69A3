#include <stdbool.h>
#include <stdlib.h>

#include "ext2.h"

#define OP_FIND 0
#define OP_DELETE_FILE 1
#define OP_PRINT_ALL 2
#define OP_PRINT_NODOTS 3
#define OP_DELETE_REC 4

uint8_t *getDisk(char *path);

struct ext2_super_block *getSuperblock(uint8_t *disk);

uint8_t *getBlockFromBlockNum(uint8_t *disk, int blockNum);

struct ext2_group_desc *getGroupDescriptorTable(uint8_t *disk);

struct ext2_inode *getInodeTable(uint8_t *disk);

struct ext2_inode *getInodeFromInodeIndex(uint8_t *disk,
                                          unsigned int inodeIndex);

struct ext2_inode *getInodeFromInodeNum(uint8_t *disk, unsigned int inodeNum);

struct ext2_dir_entry_2 *findSubDirEntry(uint8_t *disk,
                                         struct ext2_inode *inode, char *dir,
                                         int op);

struct ext2_inode *getRootDirInode(uint8_t *disk);

struct ext2_dir_entry_2 *getDirEntryFromAbsolutePath(uint8_t *disk, char *path,
                                                     bool returnEnclosingDir);

uint8_t *getInodeBitmap(uint8_t *disk);

uint8_t *getBlockBitmap(uint8_t *disk);

void releaseBitmap(uint8_t *bitmap, unsigned int num);

void releaseBlockFromBlockNum(uint8_t *disk, unsigned int blockNum);

void releaseBlocks(uint8_t *disk, struct ext2_inode *inode);

void releaseInodeFromInodeNum(uint8_t *disk, unsigned int inodeNum);

void deleteFile(uint8_t *disk, struct ext2_dir_entry_2 *dirEntry,
                char *filename);

void deleteFileFromFilePath(uint8_t *disk, char *filePath);

void deleteDirEntry(uint8_t *disk, struct ext2_dir_entry_2 *entry,
                               struct ext2_dir_entry_2 *prevEntry);

void printContentsFromDirectory(uint8_t *disk, char *dirPath, bool printDots);
int findDirent(int inode, char *split_path, uint8_t *disk);
unsigned int allocateBlock(uint8_t *disk);
struct ext2_dir_entry_2 *createDirent(int parent_inode, char *entry_name,
                                      uint8_t *disk);
int adjustReclen(int rec_len);
uint8_t *getBlockFromBlockNumAndBlockPos(uint8_t *disk, int blockNum,
                                         unsigned long blockPos);
void createDirForNewDir(struct ext2_inode *dir_inode, int parent_inode,
                        struct ext2_dir_entry_2 *new_dirent, int inodenum,
                        uint8_t *disk);
void writeContent(int inode, char *contents, uint8_t *disk);
void allocateBlocksForFileContent(int inodenum, uint8_t *disk);
char *get_file_name_from_path(char *filePath);
void deleteAllFilesFromDirEntryRec(uint8_t *disk,
                                   struct ext2_dir_entry_2 *entry,
                                   struct ext2_dir_entry_2 *prevEntry);
void deleteAllFilesFromDirPath(uint8_t *disk, char *dirPath);

void deleteDirFromPath(uint8_t* disk, char* path);
int allocateInode(uint8_t *disk);
void setStructForDirent(struct ext2_dir_entry_2 *newDirent, int inodeNum, char *fileName, unsigned char type);
void setStructForInode(struct ext2_inode *targetInode, unsigned short type, int inodeSize, int linksCount, int iBlocks);