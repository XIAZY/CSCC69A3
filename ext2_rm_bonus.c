#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "ext2.h"
#include "utils.h"
int main(int argc, char* argv[]) {
  char* diskPath;
  char* filePath;
  bool isRecursive = false;
  if (argc == 3) {
    diskPath = argv[1];
    filePath = argv[2];
  } else if (argc == 4 && strncmp(argv[2], "-r", 2) == 0) {
    diskPath = argv[1];
    filePath = argv[3];
    isRecursive = true;
  } else {
    fprintf(stderr, "usage: %s image [-r] file\n", argv[0]);
    exit(1);
  }

  uint8_t* disk = getDisk(diskPath);

  struct ext2_dir_entry_2* entry =
      getDirEntryFromAbsolutePath(disk, filePath, false);

  if (isRecursive && entry->file_type == EXT2_FT_DIR) {
    deleteDirFromPath(disk, filePath);
  } else {
    deleteFileFromFilePath(disk, filePath);
  }
}