#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include "utils.h"

int main(int argc, char *argv[]){
    // check if the user inputs correct number of arguements
    if (argc != 3){
        fprintf(stderr, 
            "Usage: %s <image file name> <absolute target path>\n",
             argv[0]);
        exit(1);
    }
    char *targetPath = argv[2];
    // path not start with a '/' is not absolute
    if (targetPath[0] != '/'){
        fprintf(stderr, "%s Please use absolute path!\n", targetPath);
        return ENOENT;
    }
    // initialize the disk
    uint8_t *disk = getDisk(argv[1]);
    // get the dir name of the target path
    char *dirName = dirname(targetPath);
    // get the basename of the target path this is
    // the name of the new dir we will be creating
    char *baseName = basename(targetPath);
    // get the inode number for the dir of given target path
    int dirInodeNum = getDirEntryFromAbsolutePath(disk, dirName, false)->inode;
    // get the type of the dir given the inode
    int isDir = S_ISDIR(getInodeFromInodeNum(disk,dirInodeNum)->i_mode);
    // if the type of the parent path is not dir the new dir cannot be created
    if (!isDir){
        fprintf(stderr, "Cannot create directory in a file!");
        return ENOENT;
    }

    // check if there exist a dir with the same name 
    if (findDirent(dirInodeNum, baseName, disk) != -1){
        fprintf(stderr, "Directory %s already exists!\n", targetPath);
        return EEXIST;
    }

    // check if the name of the new directory exceeds the EXT2_NAME_LEN
    if (strlen(baseName) > EXT2_NAME_LEN){
        fprintf(stderr, "Directory name is too long\n");
        return ENOENT;
    }
    // if no such directory exists in the target directory, we can find an available inode
    int availableInode = allocateInode(disk);
    struct ext2_group_desc *bgd = getGroupDescriptorTable(disk);
    struct ext2_inode *targetInode = getInodeFromInodeNum(disk, dirInodeNum);
    struct ext2_inode *parentInode = getInodeFromInodeNum(disk, dirInodeNum);
    // create a new dirent on the target loaction on disk for the dir
    struct ext2_dir_entry_2 *newDirent;
    newDirent = createDirent(dirInodeNum, baseName, disk);
    // Set the structure of the new dirent we created
    setStructForDirent(newDirent, availableInode, baseName, EXT2_FT_DIR);
    // get the corresponding inode
    targetInode = getInodeFromInodeNum(disk,availableInode);
    // set the structure of the inode for the directory we created
    setStructForInode(targetInode, EXT2_S_IFDIR, 1024, 2, 2);
    // create the "." and ".." for our new directory -> initialize the new dir
    createDirForNewDir(targetInode, dirInodeNum, newDirent, availableInode, disk);
    // update the links count of the parent inode since we are adding a new dir in it
    parentInode->i_links_count++;
    // update the used dirs count as well
    bgd->bg_used_dirs_count++;
    return 0;
}
