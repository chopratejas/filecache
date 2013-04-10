/* This file contains the implementation of a simple file cache manager.
 * I have tried to have concurrent thread support as much as I could.
 * Typical use case would involve using of a pthread library by the application
 * to spawn threads and call appropriate functions
 * One enhancement could be to use mutexes to lock the data structure
 * corresponding to the cache.
 */

/* Design:
 *
 * File Cache:
 * The file cache is created as a struct which has several elements that
 * indicate the current state of the cache. These include the number of
 * cached entries, the total number of entries permissible and two linked
 * lists. One of the linked lists contains information of the files cached
 * The other linked list contains information of the files waiting for
 * some cached file to be evicted from the cache.
 *
 * File Node:
 * The files are represented as node elements in a linked list implementation
 * The head of the list is passed to the cache data structure. Every element
 * of the list contains information pertaining to a file, the pinning count
 * of the file, the file descriptor (initial value of -1), indicating whether
 * file is dirty or not, an integer indicating whether the file node is 
 * actively occupied by a file or not and the file buffer from where one could
 * read or write to.
 *
 * Constructing the cache involved initializing all the elements of the file
 * nodes and cache. Destructing the cache involved flushing of the file buffers
 * if they were dirty and finally freeing the entire cache.
 *
 * There is another function called file_cache_evict which can be periodically
 * called by the user/pin function to see if something got evicted from the
 * cache.
 *
 * In my implementation, I assume that no eviction is done by any other 
 * function except evict. Other functions can mark file nodes as being not 
 * occupied/pincount = 0/ not dirty, but they will not be evicted.
 *
 * Also, I could have used a condition variable for signalling and waiting
 * for an eviction inside the pin function, but I chose to avoid it. Currently
 * I busy loop, which is not the most efficient way of handling waiting inside
 * the function
 *
 * My implementation can take relative file names and resolve them to their
 * absolute name. This avoids being restricted to the current directory alone
 * and increases the scope of using this cache.
 */


#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include "file-cache.h"
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <pthread.h>

/* The size of each file is fixed to be 10kb, i.e. 10240 bytes */
#define SIZE 10240

/* To give the user the ability to debug */
static int debug_level = 1;

#define DEBUG(args...) \
  if (debug_level > 0) \
      printf (args)


/* When the cache is full and a file is requested to be pinned,
 * it is made to wait in a linked list of wait files */
struct wait_file{
  int file_des;			/* File descriptor */
  struct wait_file *next;	/* pointer to next element */
};

/* Every file in the cache is represented by a file_node, which is a 
 * linked list of files
 */
struct file_node{
  int file_des;			/* File descriptor */
  bool is_dirty;		/* indicates whether the file is dirty or not */
  int pin_count;		/* indicates the count of pinning done by 
  				 * different threads */
  bool is_occupied;		/* indicates whether the linked list node is 
  				 * currently occupied by a file or not */
  char *file_name;		/* the name of the file */
  char file[SIZE + 1];		/* Space for reading and keeping a file */
  struct file_node *next;	/* pointer to the next element */
  pthread_mutex_t mutex;	/* Mutex for file node structure */
};

/* The file_cache structure is the basic cache structure which contains 
 * information about the current state of the cache */

struct file_cache{
  int num_files;		/* Number of files currently within the cache */
  int max_files;		/* Maximum entries supported by the cache */
  struct file_node *head_file;	/* Pointer to the head element of the linked
  				 * list of files inside the cache */
  struct wait_file *waiting;	/* Pointer to the head of the linked list of
  				 * files waiting for eviction */
};

/* This function is used to initialize every file node in the cache*/

static file_node *file_node_construct (int max_entries)
{
  int i = 0;
  /* Allocate the head file of the cache */
  file_node *head;
  file_node *temp;

  /* Initialize the nodes */
  while (i < max_entries){
    file_node *new_node = malloc(sizeof(file_node));
    /* Initialize the different elements of 
     * every node */
    new_node->file_des = -1;
    new_node->is_dirty = 0;
    new_node->pin_count = 0;
    new_node->is_occupied = 0;
    new_node->file_name = (char *)malloc(PATH_MAX);
    pthread_mutex_init(&new_node->mutex, NULL);
    
    if (i==0) {
      head = new_node;
      temp = head;
    }else{
      temp->next = new_node;
      temp = new_node;
    }
    i++;
  }
  temp->next = NULL;
 
  return head;
}

/* This function initializes the file cache structure */
file_cache *file_cache_construct (int max_entries)
{
  int temp_entries = 0;
  file_cache *new_cache = malloc (sizeof(file_cache));
  /* Initially the number of files are zero */
  new_cache->num_files = 0;
  new_cache->max_files = max_entries;
  
  /* Initializing the linked list of file nodes inside the cache */
  new_cache->head_file = file_node_construct (max_entries);

  /* Since no file is waiting at this point, initialize the waiting
   * to be NULL */
  new_cache->waiting = NULL;
  return new_cache;
}

/* Destroying the file cache data structure. If any file is pinned and has its
 * dirty element set, it is flushed first and then removed */
void file_cache_destroy (file_cache *cache)
{
  file_node *head = cache->head_file;
  file_node *list = head;
  file_node *temp = head;
  int fd = 0;
  while (list->next != NULL){
    /* If the file node is dirty, flush it */
    if (list->is_dirty){
      DEBUG("Flushing file: %s\n", list->file_name);
      if (write (list->file_des, list->file, SIZE) != SIZE){
        printf("Error while flushing!\n");
	return;
      }
      
      list->is_dirty = 0;
      list->is_occupied = 0;

      cache->num_files--;
    }
    temp = list->next;
    DEBUG("Freeing file node: %s\n", list->file_name);
    pthread_mutex_destroy(&list->mutex);
    free(list->file_name);
    free(list);
    list = temp;
  }
  
  DEBUG("Freeing Cache!\n");
  free(cache);

}

/* openfile creates a file when the file does not
 * exist in the system and writes 10kb of 0's inside it */
void openfile(char *file_name)
{
  FILE *fp = fopen(file_name, "w");
  int i = 0, c = 0;
  if (fp == NULL)
    printf("Could not open file!\n");

  for (i = 0; i < SIZE; i++) {
    c = fputc('0', fp);
  }

  fclose(fp);
  return;
}
/* This function updates the node pointed by listptr with new
 * file indicated by file descriptor and file name passed. It involves
 * reading the file into the buffer and updating the appropriate structure
 * elements
 */
static void update_file_node (file_node **listptr, int fd, char *file_name)
{
  file_node *list = (file_node *)(*listptr);
  pthread_mutex_lock(&list->mutex); 
  list->file_des = fd;
  /* read the file into that node's buffer */
  if (read (fd, list->file, SIZE) < 0){
    printf("Read of file:%s from disk failed\n", file_name);
    return;
  }
  list->file[SIZE] = '\0';
	   
  /* Update cache and node list */
  list->is_occupied = 1;
  strcpy(list->file_name, file_name);
  list->pin_count++;
  pthread_mutex_unlock(&list->mutex);
  *listptr = list;
}


void file_cache_pin_files (file_cache *cache, const char **files, int num_files)
{
  /* Reading the files into memory.
   * Look at the file, check if it exists on the system by running through
   * the list
   * If it exists, increase the pin_count variable, if it does not exist,
   * check if there is space in the cache.
   * If no space block until space is available.
   *
   * If space, read the contents of the file from the memory and cache them*/

   int i = 0;
   int fd = 0;
   file_node *head = cache->head_file;
  

   /* For every file in the files array */
   for (i = 0; i < num_files; i++){
     char file_name[PATH_MAX];
     realpath((char *)(files[i]), file_name);

     DEBUG("The name of the %d th file is: %s\n", i+1, file_name);

     file_node *list = head;
     
     /* Check if file exists in cache, i.e. already pinned */
     while(list != NULL){
       if(list->file_des != -1){
         if (!(strcmp(list->file_name, file_name))){
	   if(list->is_occupied){
             DEBUG("File exists in the cache!\n");
	     /* Increase the pin_count if file already exists */
	     list->pin_count++;
	     break;
	   }
         }
       }
       list = list->next;
     }

     /* Open the file for reading */
     fd = open(file_name, O_RDONLY);
     
     /* If file does not exist, create a file */
     if (fd < 0){
        openfile(file_name);
	fd = open (file_name, O_RDONLY);
	DEBUG("New file:%s created\n", file_name);
     }

     /* Check if there is space in cache, if yes, read contents of the
      * file and cache */
     if(cache->num_files < cache->max_files){
       /* There is space */
       list = head;
       
       while(list != NULL){
         /* Find the unoccupied node */
	 if (list->is_occupied == 0){
           /* Update its struct elements */
           update_file_node(&list, fd, file_name);
	   cache->num_files++;
	   break;
	 }
         list = list->next;
       }
     } else {
       /* There is no space left */
       /* Check the cache to see if there is a non-pinned and non-dirty entry
	* evict it
	*/
       list = head;
       
       while(list != NULL){
	 
	 if (list->is_dirty == 0 && list->pin_count == 0){

           /* This element needs to be evicted, so it is replaced by the new 
	    * file and elements of the struct are updated */
           update_file_node (&list, fd, file_name);
           cache->num_files++;
	   break;
	 }
	 list = list->next;
       }/* traversing the list */
       
       /* Append itself to the list of wait files */
       DEBUG("File:%s has to wait for eviction\n", file_name);
       
       wait_file *waiting = malloc(sizeof(wait_file));
       waiting->file_des = fd;
       waiting->next = NULL;
       
       /* If this is not the first file waiting, add it to the end of the
	* linked list representing waiting files */
       if(cache->waiting == NULL){
	 cache->waiting = waiting;
       }
       else {
         wait_file *w_list = cache->waiting;
         while(w_list->next != NULL){
	   w_list = w_list->next;
	 }
	 w_list->next = waiting;
       }

       while(cache->waiting){
         while(list != NULL){
	   if(list->is_occupied == 0){
	     /* There was an element evicted, update that file node */
	     update_file_node(&list, fd, file_name);
	     cache->num_files++;
	   } 
           list = list->next;
	 }
	 list = head;
       }
     }/* No space left in cache */
   }/* Processed one file */
}

void file_cache_unpin_files (file_cache *cache, const char **files, 
				int num_files)
{
  int i = 0;
  file_node *head = cache->head_file;
  file_node *list = head;
  for (i = 0; i < num_files; i++){
    char file_name[PATH_MAX];

    /* Obtain the entire path name of the file */
    realpath((char *)(files[i]), file_name);
 
    while(list != NULL) {
      if((list->file_name != NULL) && (!strcmp(list->file_name, file_name))){
        /* Decrease the pin count by 1 if it was greater than zero, else the
	 * file was already unpinned
	 * NOTE: We do not evict the file here even if its pin count is zero.
	 * the eviction is handled only by the file_cache_evict function */
	if(list->pin_count != 0){
          DEBUG("UNPINNING: File: %s\n", file_name);
	  list->pin_count--;
	  break;
	}
      }
      list = list->next;
    }
  } /* For num_files */
}


const char *file_cache_file_data (file_cache *cache, const char *file)
{
  /* get the head of the linked list of files from the cache */
  struct file_node *head = cache->head_file;
  struct file_node *list = head;

  while(list != NULL) {
    if ((!strcmp(list->file_name, file))
         && (list->pin_count > 0)){
	 DEBUG("Reading file :%s\n", file);
      return list->file;
    }
    list = list->next;
  }
  return NULL;
}

char *file_cache_mutable_file_data (file_cache *cache, const char *file)
{
  struct file_node *head = cache->head_file;
  struct file_node *list = head;
  
  while (list != NULL) {
    if ((!strcmp(list->file_name, file))
         && (list->pin_count > 0)){
      DEBUG("Writing file: %s\n", file);
      list->is_dirty = 1;
      return list->file;
    }
    list = list->next;
  }

  return NULL;
}

int file_cache_evict (file_cache *cache)
{
  struct file_node *head = cache->head_file;
  struct file_node *list = head;
  int i = 0;
  while (list != NULL){
    if (list->is_occupied == 1){
      if ((list->pin_count == 0) && (list->is_dirty == 0)){
        DEBUG("Evicting file: %s\n", list->file_name);
        list->is_occupied = 0;
	cache->num_files--;
	i = 1;
      }
    }
    list = list->next;
  }
  return i;
}

wait_file *get_waiting_files(file_cache * cache)
{
  return cache->waiting;
}

file_node *get_file_node(file_cache *cache, char *file)
{
  file_node *head = cache->head_file;
  file_node *list = head;

  while(list != NULL){
    if(((list->file_name) != NULL) && (!strcmp(list->file_name, file)))
      return list;
    list = list->next;
  }
}

int get_num_files (file_cache *cache)
{
  return cache->num_files;
}

int get_pin_count (file_node *file)
{
  return file->pin_count;
}


