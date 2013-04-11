// File cache in C. The typical usage is for a
// client to call 'file_cache_pin_files()' to pin a bunch of files in the cache
// and then either read or write to their in-memory contents in the cache.
// Writing to a cache entry makes that entry 'dirty'. Before a dirty entry can
// be evicted from the cache, it must be unpinned and has to be cleaned by
// writing the corresponding data to storage.
//
// All files are assumed to have size 10KB. If a file doesn't exist to begin
// with, it should be created and filled with zeros - the size should be 10KB.
//
// 'file_cache' should be a thread-safe object that can be simultaneously
// accessed by multiple threads.


#ifndef _FILE_CACHE_H_
#define _FILE_CACHE_H_


typedef struct file_node file_node;
typedef struct file_cache file_cache;
typedef struct wait_file wait_file;


// Constructor. 'max_cache_entries' is the maximum number of files that can
// be cached at any time.
file_cache *file_cache_construct(int max_cache_entries);

// Destructor. Flushes all dirty buffers.
void file_cache_destroy(file_cache *cache);

// Pins the given files in array 'files' with size 'num_files' in the cache.
// If any of these files are not already cached, they are first read from the
// local filesystem. If the cache is full, then some existing cache entries may
// be evicted. If no entries can be evicted (e.g., if they are all pinned, or
// dirty), then this method will block until a suitable number of cache
// entries becomes available. It is OK for more than one thread to pin the
// same file, however the file should not become unpinned until both pins
// have been removed.
//
// Is is the application's responsibility to ensure that the files may
// eventually be pinned. For example, if 'max_cache_entries' is 5, an
// irresponsible client may try to pin 4 files, and then an additional 2
// files without unpinning any, resulting in the client deadlocking. The
// implementation *does not* have to handle this.

void file_cache_pin_files(file_cache *cache,
                          const char **files,
                          int num_files);

// Unpin one or more files that were previously pinned. It is ok to unpin
// only a subset of the files that were previously pinned using
// file_cache_pin_files(). It is undefined behavior to unpin a file that wasn't
// pinned.
void file_cache_unpin_files(file_cache *cache,
                            const char **files,
                            int num_files);

// Provide read-only access to a pinned file's data in the cache.
//
// It is undefined behavior if the file is not pinned, or to access the buffer
// when the file isn't pinned.
const char *file_cache_file_data(file_cache *cache, const char *file);

// Provide write access to a pinned file's data in the cache. This call marks
// the file's data as 'dirty'. The caller may update the contents of the file
// by writing to the memory pointed by the returned value.
//
// Multiple clients may have access to the data, however the cache *does not*
// have to worry about synchronizing the clients' accesses (you may assume
// the application does this correctly).
//
// It is undefined behavior if the file is not pinned, or to access the buffer
// when the file is not pinned.
char *file_cache_mutable_file_data(file_cache *cache, const char *file);

// File cache eviction function. It is used to go through the cache
// entries and evict files that can be evicted. i.e. files that
// are not dirty and are not pinned. Returns a 1 if it is able to
// evict even one file.
//
int file_cache_evict (file_cache *cache);

// Function that gives ability to the user to get the node
// data structure corresponding to the file. This assumes that the user is 
// exposed to the data structure.
file_node *get_file_node(file_cache *cache, char *file);

// It gives user the ability to look at files that are waiting to be 
// cached.
wait_file *get_waiting_files(file_cache *cache);

// The user can check the number of threads that have pinned a file to the 
// cache
int get_pin_count(file_node *file);

// This function returns the number of files currently pinned inside the
// cache
int get_num_files(file_cache *cache);

#endif  // _FILE_CACHE_H_
