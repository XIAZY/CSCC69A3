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
  if (argc == 3) {
    diskPath = argv[1];
    filePath = argv[2];
  } else {
    fprintf(stderr, "usage: %s image file\n", argv[0]);
    exit(1);
  }

  uint8_t* disk = getDisk(diskPath);

  deleteFileFromFilePath(disk, filePath);
}