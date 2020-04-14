#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "ext2.h"
#include "utils.h"

int main(int argc, char *argv[]){
    char *target_file;
    int symlink = 0;
    // get disk from given image
    uint8_t *disk = getDisk(argv[1]);
    // get the inode table
    struct ext2_inode *root = getRootDirInode(disk);
    struct ext2_dir_entry_2 *source_dire;
    struct ext2_dir_entry_2 *dest_dire;
    unsigned int d_num;
    unsigned int s_num;
    // program takes three command line arguments
    // creating a link from the first specified file to the second specified path
    if(argc != 4 && argc != 5){
        fprintf(stderr, "usage: ./ln <disk image> [-s] <source absolute pate> <dest absolute path>\n");
        exit(1);
    }
    // store path from stdin and check whether path is valid
    if(argc == 5 && strcmp(argv[2], "-s") == 0){
        if(strcmp(argv[3], argv[4]) == 0){
            perror("cannot link at the same directory\n");
            exit(1);
        }
        // flag changed if "-s" is given
        symlink = 1;
        target_file = basename(argv[4]);
        if(strlen(argv[4]) > 0 && argv[4][strlen(argv[4]) - 1] == '/'){
            argv[4][strlen(argv[4]) - 1] = '\0';
        }
        // get entry from the directory of two absolute path
        source_dire = getDirEntryFromAbsolutePath(disk, dirname(argv[3]), false);
        dest_dire = getDirEntryFromAbsolutePath(disk, dirname(argv[4]), false);
        // get the inode number
        d_num = dest_dire->inode;
        s_num = source_dire->inode;
        // error handler for source absolute path
        // if the target file dose not exist then return an appropriate error message
        if(findDirent(s_num, basename(argv[3]), disk) == -1){
            perror("source file does no exist\n");
            return(ENOENT);
        }else{
            // if target file is a directory then return an appropriate error message
            struct ext2_dir_entry_2 *checker = getDirEntryFromAbsolutePath(disk, argv[3], false);
            if(checker->file_type == EXT2_FT_DIR){
                perror("elther location must be a file\n");
                return (EISDIR);
            }
        }
        // error handler for dest absolute path
        // if target file exists, return an appropriate error
        if(findDirent(d_num, basename(argv[4]), disk) != -1){
            // target file is a directory
            struct ext2_dir_entry_2 *check = getDirEntryFromAbsolutePath(disk, argv[4], false);
            if(check ->file_type == EXT2_FT_DIR){
                perror("elther location must be a file not directory\n");
                return (EISDIR);
            }else{
                // target file has been created
                perror("link name already exists\n");
                return(EEXIST);
                }  
            }
        }

    if(argc == 4){
        if(strcmp(argv[2], argv[3]) == 0){
            perror("cannot link at the same directory\n");
            exit(1);
        }
        target_file = basename(argv[3]);
        if(strlen(argv[3]) > 0 && argv[3][strlen(argv[3]) - 1] == '/'){
            argv[3][strlen(argv[3]) - 1] = '\0';
        }
        source_dire = getDirEntryFromAbsolutePath(disk, dirname(argv[2]), false);
        dest_dire = getDirEntryFromAbsolutePath(disk, dirname(argv[3]), false);
        d_num = dest_dire->inode;
        s_num = source_dire->inode;
        if(findDirent(s_num, basename(argv[2]), disk) == -1){
            perror("source file does no exist\n");
            return(ENOENT);
        }else{
            struct ext2_dir_entry_2 *checker = getDirEntryFromAbsolutePath(disk, argv[2], false);
            if(checker->file_type == EXT2_FT_DIR){
                perror("elther location must be a file\n");
                return (EISDIR);
            }
        }
        if(findDirent(d_num, basename(argv[3]), disk) != -1){
            struct ext2_dir_entry_2 *check = getDirEntryFromAbsolutePath(disk, argv[3], false);
            if(check ->file_type == EXT2_FT_DIR){
                perror("elther location must be a file not directory\n");
                return (EISDIR);
        }else{
            perror("link name already exists\n");
            return(EEXIST);
            }   
        }
    }
    if(argc == 5 && strcmp(argv[3], "-s") == 0){
        if(strcmp(argv[2], argv[4]) == 0){
            perror("cannot link at the same directory\n");
            exit(1);
        }
        // flag changed if "-s" is given
        symlink = 1;
        target_file = basename(argv[4]);
        if(strlen(argv[4]) > 0 && argv[4][strlen(argv[4]) - 1] == '/'){
            argv[4][strlen(argv[4]) - 1] = '\0';
        }
        // get entry from the directory of two absolute path
        source_dire = getDirEntryFromAbsolutePath(disk, dirname(argv[2]), false);
        dest_dire = getDirEntryFromAbsolutePath(disk, dirname(argv[4]), false);
        // get the inode number
        d_num = dest_dire->inode;
        s_num = source_dire->inode;
        // error handler for source absolute path
        // if the target file dose not exist then return an appropriate error message
        if(findDirent(s_num, basename(argv[2]), disk) == -1){
            perror("source file does no exist\n");
            return(ENOENT);
        }else{
            // if target file is a directory then return an appropriate error message
            struct ext2_dir_entry_2 *checker = getDirEntryFromAbsolutePath(disk, argv[2], false);
            if(checker->file_type == EXT2_FT_DIR){
                perror("elther location must be a file\n");
                return (EISDIR);
            }
        }
        // error handler for dest absolute path
        // if target file exists, return an appropriate error
        if(findDirent(d_num, basename(argv[4]), disk) != -1){
            // target file is a directory
            struct ext2_dir_entry_2 *check = getDirEntryFromAbsolutePath(disk, argv[4], false);
            if(check ->file_type == EXT2_FT_DIR){
                perror("elther location must be a file not directory\n");
                return (EISDIR);
            }else{
                // target file has been created
                perror("link name already exists\n");
                return(EEXIST);
                }
            }
        }
    
    if(argc == 5 && strcmp(argv[4], "-s") == 0){
        if(strcmp(argv[2], argv[3]) == 0){
            perror("cannot link at the same directory\n");
            exit(1);
        }
        // flag changed if "-s" is given
        symlink = 1;
        target_file = basename(argv[3]);
        if(strlen(argv[3]) > 0 && argv[3][strlen(argv[3]) - 1] == '/'){
            argv[3][strlen(argv[3]) - 1] = '\0';
        }
        // get entry from the directory of two absolute path
        source_dire = getDirEntryFromAbsolutePath(disk, dirname(argv[2]), false);
        dest_dire = getDirEntryFromAbsolutePath(disk, dirname(argv[3]), false);
        // get the inode number
        d_num = dest_dire->inode;
        s_num = source_dire->inode;
        // error handler for source absolute path
        // if the target file dose not exist then return an appropriate error message
        if(findDirent(s_num, basename(argv[2]), disk) == -1){
            perror("source file does no exist\n");
            return(ENOENT);
        }else{
            // if target file is a directory then return an appropriate error message
            struct ext2_dir_entry_2 *checker = getDirEntryFromAbsolutePath(disk, argv[2], false);
            if(checker->file_type == EXT2_FT_DIR){
                perror("elther location must be a file\n");
                return (EISDIR);
            }
        }
        // error handler for dest absolute path
        // if target file exists, return an appropriate error
        if(findDirent(d_num, basename(argv[3]), disk) != -1){
            // target file is a directory
            struct ext2_dir_entry_2 *check = getDirEntryFromAbsolutePath(disk, argv[3], false);
            if(check ->file_type == EXT2_FT_DIR){
                perror("elther location must be a file not directory\n");
                return (EISDIR);
            }else{
                // target file has been created
                perror("link name already exists\n");
                return(EEXIST);
                }
            }
        }
    
    //get the inode number from source file and dest directory
    struct ext2_inode *source_inode = (struct ext2_inode *)(root + (s_num - 1)); 
    struct ext2_dir_entry_2 *new_dirent;

    //deal with the case if option "-s" is given (soft link)
    // soft link create a new node
    if(symlink == 1){
        int filesize = strlen(argv[3]);
        int free_inode = 0;
        //find a available inode position
        free_inode = allocateInode(disk);
        // create a new inode
        struct ext2_inode *new_inode = (struct ext2_inode *)(root + free_inode - 1);
        // initialize attributes of new inode
        new_inode->i_uid = 0;
        new_inode->i_size = filesize;
        new_inode->i_blocks = source_inode->i_blocks / 2;
        new_inode->i_dtime = 0;
        new_inode->i_ctime = time(NULL);
        for (int i = 0 ; i < 15 ; i++) {
            new_inode->i_block[i] = 0;
        }
        new_inode->i_gid = 0;
        new_inode->osd1 = 0;
        new_inode->i_file_acl = 0;
        new_inode->i_dir_acl = 0;
        new_inode->i_faddr = 0;
        new_inode->i_links_count = 1; 
        new_inode->i_mode = EXT2_S_IFLNK;
        //create new dirent for soft link
        new_dirent = createDirent(d_num, target_file, disk);
        setStructForDirent(new_dirent, free_inode, target_file, EXT2_FT_SYMLINK);
        //allocate a block in dest directory for contents of source file
        allocateBlocksForFileContent(free_inode, disk);
        // write the source file path inside the data block
        writeContent(free_inode, argv[3], disk);

    }else{
        //deal with the case that "-s" isn't given (hard link)
        //create a new dirent for link file
        new_dirent = createDirent(d_num, target_file, disk);
        // update structure of new dirent
        setStructForDirent(new_dirent, s_num, target_file, EXT2_FT_REG_FILE);
        // increment source_inode link count
        source_inode->i_links_count += 1;
  }
}
