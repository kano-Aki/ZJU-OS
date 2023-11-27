#include "mm.h"

#include "vm.h"
#include "stdio.h"


#define set_split(x) ((unsigned int)(x) | 0x80000000)//最高位表示是否被分割
#define set_unsplit(x) ((unsigned int)(x) & 0x7fffffff)
#define get_size(x) set_unsplit(x) //排除最高位，即split位，得到真实的size
#define check_split(x) ((unsigned int)(x) & 0x80000000)


uint64_t get_index(uint64_t pa) {

  uint64_t offset = (pa - buddy_system.base_addr) / PAGE_SIZE;//这个offset确定的是pa在buddy_tree的leaf节点中的位置，
                                                              //也就是大小为几(个page)。从leaf开始，不断往上确定属于哪个节点
  int block_size = 1;//leaf为1(个page大小)
  while(offset % block_size == 0) {
    block_size <<= 1;//上一层的父节点
  }//直到找到不能整除，说明这个size是pa所在的节点的父亲的size
  block_size = block_size >> 1;
  return ((MEMORY_SIZE / PAGE_SIZE) / block_size) + (offset / block_size);
  //因为是满二叉树，所以每一层的节点数都等于上面各层节点数之和，所以直接/block size求出上面各层节点个数，再加上offset/block size，即在这一层的index
}

uint64_t get_addr(int index) {

  int level = 0, index_copy = index;
  while(index_copy > 1) {
    index_copy >>= 1;
    level++;
  }

  int block_size = MEMORY_SIZE >> level;
  int offset = (index - (1 << level)) * block_size;
  return buddy_system.base_addr + offset;
}

uint32_t get_block_size(int index) {
  int level = 0, index_copy = index;
  while(index_copy > 1) {
    index_copy >>= 1;
    level++;
  }
  return MEMORY_SIZE >> level;
}


uint64_t alloc_page() {
  return alloc_pages(1);
}

int alloced_page_num() {
  // 返回已经分配的物理页面的数量
  // 在buddy system中，我们不再使用该函数
  return -1;
}

void init_buddy_system() {
  // TODO: 初始化buddy system
  // 1. 将buddy system的每个节点初始化为可分配的内存大小，单位为PAGE_SIZE
  // 注意我们用buddy_system.bitmap实现一个满二叉树，其下标变化规律如下：如果当前节点的下标是 X，那么左儿子就是 `X * 2` ，右儿子就是 `X * 2 + 1` ，X 从1开始。
  // 那么，下标为 X 的节点可分配的内存为多少呢？
  // 2. 将buddy system的base_addr设置为&_end的物理地址
  // 3. 将buddy system的initialized设置为true
  buddy_system.initialized = 1;
  buddy_system.base_addr = PHYSICAL_ADDR(&_end);
  for(int i=1;i<8192;i++)
  {
    buddy_system.bitmap[i]=get_block_size(i)/PAGE_SIZE;
  }
};

uint64_t alloc_buddy(int index, unsigned int num) {
  // TODO: 找到可分配的节点并完成分配，返回分配的物理页面的首地址（通过get_addr函数可以获取节点index的起始地址）
  // 1. 如果当前节点大于num，则查看当前节点的左右儿子节点
  // 提示：通过get_size函数可以获取节点index的可分配连续内存大小
  // 2. 如果当前节点的可分配连续内存等于num，且当前节点没有被拆分，则分配当前节点
  // 提示：通过check_split函数可以获取节点index的状态
  // 3. 如果当前节点的可分配连续内存等于num且已经被拆分，则查看当前节点的左右儿子节点
  // 4. 如果当前节点的可分配连续内存小于num，则分配失败返回上层节点
  // 如果完成分配，则要递归更新节点信息，将涉及到的节点的可分配连续内存更新，
  // 已拆分则是左右子节点的可分配的最大连续物理内存最大值，未拆分则是两者之和，并使用set_split更新节点的状态
  if(get_size(buddy_system.bitmap[index])<num) return 0;
  if(get_size(buddy_system.bitmap[index])==num && !check_split(buddy_system.bitmap[index]))
  {
    buddy_system.bitmap[index]=0;//分配该节点
    // int level = 0, index_copy = index;
    // while(index_copy > 1)
    // {
    //   index_copy >>= 1;
    //   level++;
    // }//找到该节点的层数
    // for(int count=1;level+count<13;count++)//将被分配节点的子节点内存值设为0,都被分配了
    // {
    //   for(int i=index<<count;i<(index<<count)+(1<<count)-1;i++)
    //   {
    //     buddy_system.bitmap[i]=0;
    //   }
    // }//没必要将更改子节点的状态，因为之后在分配时在这一节点便失败了，不会继续往下
    return get_addr(index);
  }
  else
  {
    uint64_t pa=alloc_buddy(index*2,num);
    if(pa>0)//更新父节点的内存值
    {
      if(get_size(buddy_system.bitmap[2*index])>=get_size(buddy_system.bitmap[2*index+1]))
        buddy_system.bitmap[index]=buddy_system.bitmap[index*2];
      else
        buddy_system.bitmap[index]=buddy_system.bitmap[index*2+1];

      buddy_system.bitmap[index]=set_split(buddy_system.bitmap[index]);//被拆分
      return pa;
    }
    pa=alloc_buddy(index*2+1,num);
    if(pa>0)
    {
      if(get_size(buddy_system.bitmap[2*index]) >= get_size(buddy_system.bitmap[2*index+1]))
        buddy_system.bitmap[index]=buddy_system.bitmap[index*2];
      else
        buddy_system.bitmap[index]=buddy_system.bitmap[index*2+1];

      buddy_system.bitmap[index]=set_split(buddy_system.bitmap[index]);//被拆分
      return pa;
    }
    return 0;//左右儿子都没分配成功，则失败
  }
}

uint64_t alloc_pages(unsigned int num) {
  // 分配num个页面，返回分配到的页面的首地址，如果没有足够的空闲页面，返回0
  if (!buddy_system.initialized) {
    init_buddy_system();
  }
  // TODO:
  // 1. 将num向上对齐到2的幂次
  // 2. 调用alloc_buddy函数完成分配
  if(num>MEMORY_SIZE/PAGE_SIZE) return 0;
  for(int i=0; ;i++)
  {
    if(num <= (unsigned int)(1<<i))
    {
      num=(unsigned int)(1<<i);
      break;
    }
  }
  return alloc_buddy(1,num);
}


void free_buddy(int index) {
  // TODO: 释放节点index的页面
  // 1. 首先判断节点index的状态，如果已经被拆分，则不能直接释放。异常状态，报错并进入死循环。
  // 2. 如果没有被拆分，则恢复节点index的状态为初始状态
  // 3. 如果该节点与其兄弟节点都没有被使用，则合并这两个节点，并递归处理父节点
  // 提示：使用check_split函数可以获取节点index的状态
  // 提示：使用set_unsplit函数可以将节点index的状态恢复为初始状态
  // 提示：使用get_block_size函数可以获取节点index的初始可分配内存大小
  if(check_split(buddy_system.bitmap[index]))
  {
    printf("error: the node has been split\n");
    while(1);
  }
  buddy_system.bitmap[index]=get_block_size(index)/PAGE_SIZE;
  if(index==1) return;//根节点
  if(index/2*2==index && buddy_system.bitmap[index+1]==get_block_size(index+1)/PAGE_SIZE)//判断index是哪个儿子
  {
    buddy_system.bitmap[index/2]=set_unsplit(buddy_system.bitmap[index/2]);//设为未拆分
    free_buddy(index/2);
  }
  else if(index/2*2+1==index && buddy_system.bitmap[index-1]==get_block_size(index-1)/PAGE_SIZE)
  {
    buddy_system.bitmap[index/2]=set_unsplit(buddy_system.bitmap[index/2]);
    free_buddy(index/2);
  }
  return;
}

void free_pages(uint64_t pa) {
  // check that the pa is in the buddy system
  if (pa < buddy_system.base_addr || pa >= buddy_system.base_addr + MEMORY_SIZE) {
    printf("error: pa out of range\n");
    return;
  }
  // TODO: find the buddy system node according to pa, and free it.
  // 注意，如果该节点的状态为已经被拆分，则应该释放其左子节点
  // 提示：使用get_index函数可以获取pa对应的最上层节点的下标
  int index=get_index(pa);
  while(check_split(buddy_system.bitmap[index])) index*=2;//沿着左路径，直到找到未拆分节点。leaf肯定未拆分
  free_buddy(index);

}