#pragma once

#include "list.h"

#include "vm.h"

#define NR_PARTIAL 9
#define PAGE_SHIFT 12
#define PPN_SHIFT 10
#define PAGE_MASK (~((1UL << PAGE_SHIFT) - 1))
#define MEM_SHIFT (24 - PAGE_SHIFT)
#define STRUCTURE_SIZE 16UL

struct page {
  unsigned long flags;
  int count;//这段page被几个object使用了
  struct page *header;// Pointer to the first page of the slab,也就是几个连续的page的第一个page
  struct page *next;
  struct list_head slub_list;
  struct kmem_cache *slub; /* Pointer to slab 该page对应哪个slab*/
  void *freelist;
};

struct cache_area {
  void *base;
  size_t size;
  void *freelist;
};//存放各数据结构对应的kmem_cache的空间

struct kmem_cache {
  /* kmem_cache_cpu */
  void **freelist;   /* Pointer to next available object */
  unsigned long tid; /* Globally unique transaction id */
  struct page *page; /* The slab from which we are allocating 当前正在使用的slab的首page*/

  /* Used for retrieving partial slabs, etc. */
  int refcount;
  unsigned long min_partial;//该kmem_cache最小的partial slab的数量
  size_t size;         /* The size of an object including metadata */
  size_t object_size;  /* The size of an object without metadata */
  unsigned int offset; /* Free pointer offset 每个free object的首地址加上offset处存着下一个free object的地址*/
  unsigned long nr_page_per_slub;//每个slab由多个page组成，每个kmem_cache会有多个提前申请好的slab。每次使用某一个slab分配内存

  void (*init_func)(void *);
  unsigned int inuse;        /* Offset to metadata */
  unsigned int align;        /* Alignment */
  unsigned int red_left_pad; /* Left redzone padding size */
  const char *name;          /* Name (only for display!) */
  struct list_head list;     /* List of slab caches */

  /* kmem_cache_node */
  unsigned long nr_partial;//本实验中应该就是作为全部slab数量
  struct list_head partial;//node部分的slab list，slab数量为nr_partial。slub.c中根本没使用过
#ifdef CONFIG_SLUB_DEBUG
  unsigned long nr_slabs;
  unsigned long total_objects;
  struct list_head full;
#endif
};//kmem_cache_cpu是每个cpu独有的;kmem_cache_node是每个node独有的，不同CPU可以共享申请node的object
//本实验每个CPU似乎只有一个slab

void slub_init();
struct kmem_cache *kmem_cache_create(const char *, size_t, unsigned int, int,
                                     void *(void *));

void *kmem_cache_alloc(struct kmem_cache *);
void kmem_cache_free(void *);

void *kmalloc(size_t);
void kfree(const void *);
