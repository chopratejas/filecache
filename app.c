#include <stdio.h>
#include "file-cache.h"


int main()
{
  printf("Hello world!\n");
  char *file[4];
  file[0] = "file";
  file[1] = "file2";
  file[2] = "file3";
  file[3] = "file4";

  file_cache *cache = file_cache_construct(4);

  file_cache_pin_files(cache, (const char **)file, 3);
  file_cache_unpin_files(cache, (const char **) file, 2);
  file_cache_evict(cache); 
  //file_cache_pin_files(cache, (const char **)file, 4);

  printf("Destroying cache!\n");
  file_cache_destroy(cache);

  return 0;
}
