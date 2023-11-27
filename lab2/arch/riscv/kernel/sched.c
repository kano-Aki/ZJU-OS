#include "sched.h"
#include "test.h"
#include "stdio.h"

#define Kernel_Page 0x80210000
#define LOW_MEMORY  0x80211000
#define PAGE_SIZE 4096UL

extern void __init_sepc();

struct task_struct* current;
struct task_struct* task[NR_TASKS];

// If next==current,do nothing; else update current and call __switch_to.
void switch_to(struct task_struct* next) {
  if (current != next) {
    struct task_struct* prev = current;
    current = next;
    __switch_to(prev, next);
  }
}

int task_init_done = 0;
// initialize tasks, set member variables
void task_init(void) {
  puts("task init...\n");

  for(int i = 0; i < LAB_TEST_NUM; ++i) {
    // TODO
    // initialize task[i]
    // get the task_struct based on Kernel_Page and i
    // set state = TASK_RUNNING, counter = 0, priority = 5, 
    // blocked = 0, pid = i, thread.sp, thread.ra
    task[i]=(struct task_struct*)(Kernel_Page + i * PAGE_SIZE);
    task[i]->state = TASK_RUNNING;
    task[i]->counter = 0;
    task[i]->priority = 5;
    task[i]->blocked = 0;
    task[i]->pid = i;
    task[i]->thread.sp = (unsigned long)(LOW_MEMORY + i * PAGE_SIZE);
    task[i]->thread.ra = (unsigned long)&__init_sepc;

    printf("[PID = %d] Process Create Successfully!\n", task[i]->pid);
  }
  task_init_done = 1;
}

void call_first_process() {
  // set current to 0x0 and call schedule()
  current = (struct task_struct*)(Kernel_Page + LAB_TEST_NUM * PAGE_SIZE);
  current->pid = -1;
  current->counter = 0;
  current->priority = 0;

  schedule();
}//即 在初始设置一个空的进程，然后马上调度，这样就会调度到第一个进程


void show_schedule(unsigned char next) {
  // show the information of all task and mark out the next task to run
  for (int i = 0; i < LAB_TEST_NUM; ++i) {
    if (task[i]->pid == next) {
      printf("task[%d]: counter = %d, priority = %d <-- next\n", i,
             task[i]->counter, task[i]->priority);
    } else {
      printf("task[%d]: counter = %d, priority = %d\n", i, task[i]->counter,
           task[i]->priority);
    }
  }
}

#ifdef SJF
// simulate the cpu timeslice, which measn a short time frame that gets assigned
// to process for CPU execution
void do_timer(void) {
  if (!task_init_done) return;

  printf("[*PID = %d] Context Calculation: counter = %d,priority = %d\n",
         current->pid, current->counter, current->priority);
  
  // current process's counter -1, judge whether to schedule or go on.
  // TODO
  current->counter--;
  if(current->counter == 0){
    schedule();
  }
}


// Select the next task to run. If all tasks are done(counter=0), reinitialize all tasks.
void schedule(void) {
  unsigned char next=NR_TASKS;
  long tmp_min=LAB_TEST_COUNTER+1;//tmp用来记录最小的counter,初始值为进程初始的时间片+1,确保大于
  // TODO
  for(int i=NR_TASKS-1;i>=0 ;i--)
  {
    if(task[i] && task[i]->state==TASK_RUNNING  && task[i]->counter>0 && task[i]->counter<tmp_min)
    {
      tmp_min=task[i]->counter;
      next=i;
    }
  }
  if(next==NR_TASKS)//均无剩余时间
  {
    init_test_case();//重新分配
    schedule();
    return;
  }
  
  show_schedule(next);
  
  switch_to(task[next]);
}

#endif

#ifdef PRIORITY

// simulate the cpu timeslice, which measn a short time frame that gets assigned
// to process for CPU execution
void do_timer(void) {
  if (!task_init_done) return;
  
  printf("[*PID = %d] Context Calculation: counter = %d,priority = %d\n",
         current->pid, current->counter, current->priority);
  
  // current process's counter -1, judge whether to schedule or go on.
  // TODO
  current->counter--;
  schedule();
}

// Select the task with highest priority and lowest counter to run. If all tasks are done(counter=0), reinitialize all tasks.
void schedule(void) {
  unsigned char next=NR_TASKS;
  long tmp_min_pri=6;//不存在的特权级，确保大于
  // TODO
  for(int i=NR_TASKS-1;i>=0;i--)
  {
    if(task[i] && task[i]->counter>0 && (task[i]->priority<tmp_min_pri || (task[i]->priority==tmp_min_pri && task[i]->counter < task[next]->counter)))
    {
      tmp_min_pri=task[i]->priority;
      next=i;
    }
  }
  if(next==NR_TASKS)
  {
    init_test_case();
    schedule();
    return;
  }

  show_schedule(next);

  switch_to(task[next]);
}

#endif
/*
原始函数：
void schedule(void) {
  unsigned char next=NR_TASKS;
  long tmp_min=LAB_TEST_COUNTER+1;//tmp用来记录最小的counter,初始值为进程初始的时间片+1,确保大于
  // TODO
  for(int i=NR_TASKS-1;i>=0 && task[i] && task[i]->counter>0;i--)
  {
    if(task[i]->state==TASK_RUNNING && task[i]->counter<tmp_min)
    {
      tmp_min=task[i]->counter;
      next=i;
    }
  }
  if(next==NR_TASKS)//均无剩余时间
  {
    init_test_case();//重新分配
    for(int i=NR_TASKS-1;i>=0 && task[i] && task[i]->counter>0;i--)
    {
      if(task[i]->state==TASK_RUNNING && task[i]->counter<tmp_min)
      {
        tmp_min=task[i]->counter;
        next=i;
      }
    }
  }
  
  show_schedule(next);
  
  switch_to(task[next]);
}

bug记录：schedule函数中原本将for循环写为for(int i=NR_TASKS-1;i>=0 && task[i] && task[i]->counter>0;i--)
将这几个条件提前了，但这是for循环，这是循环条件，一次不成功直接退出循环了。在call_first_process后切换时，i初始为63
task[63]为空，则直接退出循环，导致next为NR_TASKS;即使line173的if里重新分配了，但在里面仍会直接退出for循环
所以next仍为63。，然后直接进入switch函数，在_switch_to中，由于next为63，这部分内存并不在我们的定义中，
所以在加载新进程的各寄存器时，会出现错误。
同时gdb调试时会发现在handler_s函数中，cause值为0x5,这恰为load access fault异常的cause值，巧的是时钟中断cause值为
0x8000000000000005.误以为中断被变成了同步异常
*/