#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include "utils.h"

int main(int argc, char *argv[]){
    // check if the user inputs correct number of arguements
    if (argc != 4){
        fprintf(stderr, 
            "Usage: %s <image file name> <source file> <absolute target path>\n",
                argv[0]);
        exit(1);
    }
    // initialize the disk
    uint8_t *disk = getDisk(argv[1]);
    char *srcPath = argv[2];
    char *targetPath = argv[3];
    char *basec, *fileName;
    // open the src file
    FILE *srcFile = fopen(srcPath, "r");
    struct stat FileAttrib;
    // check if the file is opened successfully
    if (srcFile == NULL){
        perror(srcPath);
        return ENOENT;
    }

    // check if we can get the src file attributes
    if (stat(srcPath, &FileAttrib) < 0){
        perror(srcPath);
    }

    // path not start with a '/' is not absolute
    if (targetPath[0] != '/'){
        fprintf(stderr, "%s Please use absolute path!\n", targetPath);
        return ENOENT;
    }
    
    // get the size of the src file
    int srcFileSize = FileAttrib.st_size;
    // check if the source file is a regular file since we only support file copys
    if (!S_ISREG(FileAttrib.st_mode)){
        fprintf(stderr, "%s is not a file!\n", argv[2]);
        return ENOENT;
    }

    // get the inode number for given target path
    // if no inode if found on the given target path, the program will exit with ENOENT
    int inodeNum = getDirEntryFromAbsolutePath(disk, targetPath, false)->inode;
    // get the type of the target path
    int isDir = S_ISDIR(getInodeFromInodeNum(disk,inodeNum)->i_mode);
    // check if the destination is a directory otherwise we cannot copy the file
    if (!isDir){
        fprintf(stderr, "%s is not a directory!\n", argv[3]);
        return ENOENT;
    }
    // if no error occurs above, we can proceed to find the inode
    struct ext2_inode *targetInode = getInodeFromInodeNum(disk,inodeNum);
    // get the file name from the given source path
    basec = strdup(srcPath);
    fileName = basename(basec);
    // check if the target location already has a file with the same name
    if (findDirent(inodeNum, fileName, disk) != -1) {
        fprintf(stderr, 
            "%s already exists in the directory!\n", fileName);
        return ENOENT;
    }
    // if no such file exists in the target directory, we can find an available inode
    int availableInode = allocateInode(disk);
    // create a new dirent on the target loaction on disk for the file
    // given the inodenum of the target location/directory
    struct ext2_dir_entry_2 *newDirent;
    newDirent = createDirent(inodeNum, fileName, disk);
    // Set the structure of the new dirent we created
    setStructForDirent(newDirent, availableInode, fileName, EXT2_FT_REG_FILE);
    // get the corresponding inode
    targetInode = getInodeFromInodeNum(disk,availableInode);
    // set the structure of the inode for the file we copied
    setStructForInode(targetInode, EXT2_S_IFREG, srcFileSize, 0, 1);
    // allocate the blocks needed for the file content
    allocateBlocksForFileContent(availableInode, disk);
    // write the content from src file to target file
    char fileContents[srcFileSize + 1];
    fread(fileContents, srcFileSize + 1, 1, srcFile);
    writeContent(availableInode, fileContents, disk);
    fclose(srcFile);
    return 0;
}
