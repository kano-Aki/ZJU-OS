#include "sched.h"
#include "test.h"
#include "stdio.h"

#define Kernel_Page 0x80210000
#define LOW_MEMORY 0x80211000
#define PAGE_SIZE 4096UL

extern void __init_sepc();

struct task_struct* current;
struct task_struct* task[NR_TASKS];

// If next==current,do nothing; else update current and call __switch_to.
void switch_to(struct task_struct* next) {
  if (current != next) {
    struct task_struct* prev = current;
    current = next;
    // printf("__switch_to in");
    __switch_to(prev, next);
    // printf("__switch_to out");
  }
}

int task_init_done = 0;
// initialize tasks, set member variables
void task_init(void) {
  puts("task init...\n");

  for(int i = 0; i < LAB_TEST_NUM; ++i) {
    // initialize task[i]
    // get the task_struct based on Kernel_Page and i
    task[i] = Kernel_Page + (0x1000 * i);
    // set state = TASK_RUNNING, counter = 0, priority = 5, 
    // blocked = 0, pid = i, thread.sp, thread.ra
    task[i]->state = TASK_RUNNING;
    task[i]->counter = 0;
    task[i]->priority = 5;
    task[i]->blocked = 0;
    task[i]->pid = i;
    task[i]->thread.sp = Kernel_Page + (0x1000 * (i + 1));
    task[i]->thread.ra = &__init_sepc;
    printf("[PID = %d] Process Create Successfully! %x\n", task[i]->pid, task[i]);
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
}


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
  --current->counter;
  if(!(current->counter))//执行完了就分配新的
    schedule();
  
}


// Select the next task to run. If all tasks are done(counter=0), reinitialize all tasks.
void schedule(void) {
  char next = NR_TASKS;
  int flag = 1;
  // printf("schedule %d\n", NR_TASKS);
  for(int i = NR_TASKS - 1; ~i; i--)      //从大到小遍历，task[i]为NULL说明后面已经没有进程了
    if(task[i] && task[i]->state == TASK_RUNNING)
    {
      // printf("%d\n", i);
      flag &= (task[i]->counter == 0);
      //判断是不是都是0
      if(next == NR_TASKS || task[next]->counter == 0 || (task[i]->counter != 0 && task[i]->counter < task[next]->counter))
        next = i;
      //schedule
    }
  // printf("schedule 2\n");
  if(flag){
    printf("all zero\n");
    init_test_case();
    schedule();
    //重新schedule
    return;
  }
  
  // printf("next: %d\n", next);
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
  --current->counter;
  schedule();//每次都要重新schedule
}

// Select the task with highest priority and lowest counter to run. If all tasks are done(counter=0), reinitialize all tasks.
void schedule(void) {
  char next = NR_TASKS;
  int flag = 1;
  for(int i = NR_TASKS - 1; ~i; i--)
    if(task[i] && task[i]->state == TASK_RUNNING)
    {
      //task[i]为NULL说明后面已经没有进程了
      flag &= (task[i]->counter == 0);
      //判断是不是都是0
      if(next == NR_TASKS || task[next]->counter == 0 || (task[i]->counter != 0 && task[i]->priority < task[next]->priority) || (task[i]->counter != 0 && task[i]->priority == task[next]->priority && task[i]->counter < task[next]->counter))
        next = i;
      //schedule
    }
  if(flag){
    printf("all zero\n");
    init_test_case();
    schedule();
    //重新schedule
    return;
  }
  
  

  show_schedule(next);

  switch_to(task[next]);
}

#endif