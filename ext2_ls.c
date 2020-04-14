#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "ext2.h"
#include "utils.h"

int main(int argc, char* argv[]) {
  // read command from command line
  // if arg number is not 3, prompt the standard error message
  // check validity of the standard input
  bool printDots = false;
  char* diskPath;
  char* targetPath;
  if (argc == 3) {
    diskPath = argv[1];
    targetPath = argv[2];
  } else if (argc == 4 && strncmp(argv[2], "-a", 2) == 0) {
    diskPath = argv[1];
    targetPath = argv[3];
    printDots = true;
  } else {
    fprintf(stderr, "Usage: %s <name> [-a] <absolute path>\n", argv[0]);
    exit(1);
  }

  uint8_t *disk = getDisk(diskPath);

  struct ext2_dir_entry_2 *entry = getDirEntryFromAbsolutePath(disk, targetPath, false);
  if (entry->file_type != EXT2_FT_DIR) {
      puts(targetPath);
  } else {
    printContentsFromDirectory(disk, targetPath, printDots);
  }

}
