//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;

  LOG_START();

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
  // ...

  LOG_STATISTICS((long)n_allocb, (long)(n_allocb/(n_malloc + n_calloc + n_realloc)), n_freeb);

  if (n_allocb != n_freeb) {
    LOG_NONFREED_START();

    item *prev = list, *cur = prev->next;
    while(cur != NULL) {
      if (cur->cnt > 0) {
        LOG_BLOCK(cur->ptr, cur->size, cur->cnt);
      }
      prev = cur;
      cur = cur->next;
    }
  }

  LOG_STOP();

  // free list (not needed for part 1)
  free_list(list);
}

// ...

void *malloc(size_t size) {
  void *ptr;
  char *err;

  if (!mallocp) {
    mallocp = dlsym(RTLD_NEXT, "malloc");

    if ((err = dlerror()) != NULL) {
      mlog(err);
      exit(1);
    }
  }

  ptr = mallocp(size);
  if (ptr != NULL) {
    LOG_MALLOC(size, ptr);
    n_malloc++;
    n_allocb += size;

    alloc(list, ptr, size);
  }

  return ptr;
}

void *calloc(size_t num, size_t size) {
  void *ptr;
  char *err;

  if (!callocp) {
    callocp = dlsym(RTLD_NEXT, "calloc");

    if ((err = dlerror()) != NULL) {
      mlog(err);
      exit(1);
    }
  }

  ptr = callocp(num, size);
  if (ptr != NULL) {
    LOG_CALLOC(num, size, ptr);
    n_calloc++;
    n_allocb += num * size;

    alloc(list, ptr, num * size);
  }

  return ptr;
}

void *realloc(void *ptr, size_t new_size) {
  void *new_ptr;
  char *err;
  item *freed_block;
  if (!reallocp) {
    reallocp = dlsym(RTLD_NEXT, "realloc");

    if ((err = dlerror()) != NULL) {
      mlog(err);
      exit(1);
    }
  }

  new_ptr = reallocp(ptr, new_size);
  if (new_ptr != NULL) {
    LOG_REALLOC(ptr, new_size, new_ptr);
    n_realloc++;
    n_allocb += new_size;

    freed_block = dealloc(list, ptr);
    n_freeb += freed_block->size;
    alloc(list, new_ptr, new_size);
  }

  return new_ptr;
}

void free(void* ptr) {
  char *err;
  item *freed_item;
  if (!freep) {
    freep = dlsym(RTLD_NEXT, "free");

    if ((err = dlerror()) != NULL) {
      mlog(err);
      exit(1);
    }
  }

  LOG_FREE(ptr);
  freed_item = find(list, ptr);
  if (freed_item == NULL) {
    LOG_ILL_FREE();
  }
  else if (freed_item->cnt == 0) {
    LOG_DOUBLE_FREE();
  }
  else {
    freep(ptr);
    freed_item = dealloc(list, ptr);
    n_freeb += freed_item->size;
  }
}
