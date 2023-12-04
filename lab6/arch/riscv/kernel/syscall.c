#include "syscall.h"
#include "list.h"
#include "riscv.h"
#include "sched.h"
#include "task_manager.h"
#include "stdio.h"
#include "defs.h"
#include "slub.h"
#include "mm.h"
#include "vm.h"

extern uint64_t text_start;
extern uint64_t rodata_start;
extern uint64_t data_start;
extern uint64_t user_program_start;
extern void trap_s_bottom(void);
extern int now_max_pid;

int strcmp(const char *a, const char *b) {
  while (*a && *b) {
    if (*a < *b)
      return -1;
    if (*a > *b)
      return 1;
    a++;
    b++;
  }
  if (*a && !*b)
    return 1;
  if (*b && !*a)
    return -1;
  return 0;
}

uint64_t get_program_address(const char * name) {
    uint64_t offset = 0;
    if (strcmp(name, "hello") == 0) offset = PAGE_SIZE;
    else if (strcmp(name, "malloc") == 0) offset = PAGE_SIZE * 2;
    else if (strcmp(name, "print") == 0) offset = PAGE_SIZE * 3;
    else if (strcmp(name, "guess") == 0) offset = PAGE_SIZE * 4;
    else {
        printf("Unknown user program %s\n", name);
        while (1);
    }
    return PHYSICAL_ADDR((uint64_t)(&user_program_start) + offset);
}

struct ret_info syscall(uint64_t syscall_num, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t sp) {
    uint64_t* sp_ptr = (uint64_t*)(sp);

    struct ret_info ret;
    switch (syscall_num) {
    case SYS_GETPID: {
        ret.a0 = getpid();
        sp_ptr[4] = ret.a0;
        sp_ptr[16] += 4;
        break;
    }
    case SYS_READ: {
        ret.a0 = getchar();
        sp_ptr[4] = ret.a0;
        sp_ptr[16] += 4;
        break;
    }
    case SYS_FORK: {
        // TODO:
        // 1. create new task and set counter, priority and pid (use our task array)
        // 2. create root page table, set current process's satp
        //   2.1 copy current process's user program address, create mapping for user program
        //   2.2 create mapping for kernel address
        //   2.3 create mapping for UART address
        // 3. create user stack, copy current process's user stack and save user stack sp to new_task->sscratch
        // 4. copy mm struct and create mapping
        // 5. set current process a0 = new task pid, sepc += 4
        // 6. copy kernel stack (only need trap_s' stack)
        // 7. set new process a0 = 0, and ra = trap_s_bottom, sp = register number * 8
        struct task_struct* new_task = (struct task_struct*)(VIRTUAL_ADDR(alloc_page()));
        // printf("fork:va of new_task: 0x%016lx\n", new_task);
        new_task->state= TASK_RUNNING;
        new_task->counter = 10;
        new_task->priority = current->priority-1;//设为与父进程相同
        new_task->blocked = 0;
        new_task->pid = ++now_max_pid;
        task[now_max_pid] = new_task;

        // printf("new pid:%d\n",now_max_pid);
        uint64_t root_page_table = alloc_page();
        uint64_t task_addr = current->mm.user_program_start;
        task[now_max_pid]->satp = (root_page_table) >> 12 | 0x8000000000000000 | (((uint64_t) (new_task->pid))  << 44);
        task[now_max_pid]->mm.user_program_start = task_addr;
        create_mapping((uint64_t*)root_page_table, 0x1000000, task_addr, PAGE_SIZE, PTE_V | PTE_R | PTE_X | PTE_U | PTE_W);//用户程序
        create_mapping((uint64_t*)root_page_table, 0xffffffc000000000, 0x80000000, 16 * 1024 * 1024, PTE_V | PTE_R | PTE_W | PTE_X);//内核空间
        create_mapping((uint64_t*)root_page_table, 0x80000000, 0x80000000, 16 * 1024 * 1024, PTE_V | PTE_R | PTE_W | PTE_X);//等值
        //内核不同段的权限设置
        create_mapping((uint64_t*)root_page_table, 0xffffffc000000000, 0x80000000, PHYSICAL_ADDR((uint64_t)&rodata_start) - 0x80000000, PTE_V | PTE_R | PTE_X);
        create_mapping((uint64_t*)root_page_table, (uint64_t)&rodata_start, PHYSICAL_ADDR((uint64_t)&rodata_start), (uint64_t)&data_start - (uint64_t)&rodata_start, PTE_V | PTE_R);
        create_mapping((uint64_t*)root_page_table, (uint64_t)&data_start, PHYSICAL_ADDR((uint64_t)&data_start), (uint64_t)&_end - (uint64_t)&data_start, PTE_V | PTE_R | PTE_W);
        //等值映射部分，对内核不同段的权限设置
        create_mapping((uint64_t*)root_page_table, 0x80000000, 0x80000000, PHYSICAL_ADDR((uint64_t)&rodata_start) - 0x80000000, PTE_V | PTE_R | PTE_X);
        create_mapping((uint64_t*)root_page_table, PHYSICAL_ADDR((uint64_t)&rodata_start), PHYSICAL_ADDR((uint64_t)&rodata_start), (uint64_t)&data_start - (uint64_t)&rodata_start, PTE_V | PTE_R);
        create_mapping((uint64_t*)root_page_table, PHYSICAL_ADDR((uint64_t)&data_start), PHYSICAL_ADDR((uint64_t)&data_start), (uint64_t)&_end - (uint64_t)&data_start, PTE_V | PTE_R | PTE_W);
        create_mapping((uint64_t*)root_page_table, 0x10000000, 0x10000000, 1 * 1024 * 1024, PTE_V | PTE_R | PTE_W | PTE_X);//硬件
        
        uint64_t physical_stack = alloc_page();
        memcpy((void*)physical_stack, (void*)current->mm.user_stack, PAGE_SIZE);
        create_mapping((uint64_t*)root_page_table, 0x1001000, physical_stack, PAGE_SIZE, PTE_V | PTE_R | PTE_W | PTE_U);//用户栈
        task[now_max_pid]->mm.user_stack = physical_stack;
        task[now_max_pid]->sscratch = read_csr(sscratch);//当前的sscratch值即为父进程的用户栈顶。子进程用户栈顶值应相等

        task[now_max_pid]->mm.vm = (struct vm_area_struct*)kmalloc(sizeof(struct vm_area_struct));
        memcpy(task[now_max_pid]->mm.vm, current->mm.vm, sizeof(struct vm_area_struct));//拷贝vm的内容
        INIT_LIST_HEAD(&(task[now_max_pid]->mm.vm->vm_list));//完成初始化后和父进程已经不同
        struct vm_area_struct* vma,*son_vm=task[now_max_pid]->mm.vm;
        int count=1;
        // printf("in fork current->mm.vm->vm_list:%d\n",list_empty(&(current->mm.vm->vm_list)));//判断父进程的vm_area链表是否为空
        list_for_each_entry(vma, &current->mm.vm->vm_list, vm_list)//不会遍历到哨兵
        {
            if(vma->mapped==1)
            {
                uint64_t size=vma->vm_end-vma->vm_start;
                uint64_t physical_addr=alloc_pages(size/PAGE_SIZE);
                uint64_t pte=get_pte((current->satp & ((1ULL << 44) - 1)) << 12, vma->vm_start);//得到父进程该vm_area对应的物理页表项
                memcpy((void*)physical_addr, (void*)((pte >> 10) << 12), size);
                create_mapping((uint64_t*)root_page_table, vma->vm_start, physical_addr, size, PTE_V | PTE_R | PTE_W | PTE_X | PTE_U);
                //将新的物理地址映射到子进程的对应虚拟地址
            }
            
            struct vm_area_struct* tmp=(struct vm_area_struct*)kmalloc(sizeof(struct vm_area_struct));
            memcpy(tmp, vma, sizeof(struct vm_area_struct));
            list_add(&(tmp->vm_list), &(son_vm->vm_list));//将新的vm_area加入到子进程的vm_area链表中
            son_vm=tmp;//让son_vm为子进程的vm_area链表的tail，确保子进程的vm_area链表的顺序与父进程相同
            // printf("parent number of vmlist:%d\n",count++);
        }

        ret.a0 = now_max_pid;
        sp_ptr[4] = now_max_pid;
        sp_ptr[16] += 4;//父进程中返回子进程的pid

        uint64_t* kernel_stack=task[now_max_pid]+PAGE_SIZE;//新进程的内核栈底
        kernel_stack-=8*31;
        memcpy((void*)kernel_stack, (void*)sp_ptr, 8*31);//拷贝父进程进入trap时，存在内核栈上的寄存器值。传入handler_s的sp值为调用handler_s函数之前的值

        kernel_stack[4]=0;//a0=0,子进程中返回0
        //kernel_stack[16]+=4;//与父进程保持一致，已经赋过值
        task[now_max_pid]->thread.ra=(uint64_t)&trap_s_bottom;
        task[now_max_pid]->thread.sp=(uint64_t)kernel_stack;//从其他进程切换到该子进程时，ra和sp值更换为这两个，从而能正确返回子进程用户态

        //printf("fork sscratch:0x%016lx\n",read_csr(sscratch));
        //printf("fork sepc:0x%016lx\n",sp_ptr[16]);
        //printf("fork success\n");
        break;
    }
    case SYS_EXEC: {
        // TODO:
        // 1. free current process vm_area_struct and it's mapping area
        // 2. reset user stack, user_program_start
        // 3. create mapping for new user program address
        // 4. set sepc = 0x1000000
        // 5. refresh TLB
        //printf("start_exec\n");
        //printf("in exec current->pid:%d\n",current->pid);
        //printf("in exec current->mm.user_program_start:0x%016lx\n",current->mm.user_program_start);
        //printf("in exec current->mm.vm:0x%016lx\n",current->mm.vm);
        //printf("1\n");
        //printf("in exec current->mm.vm->vm_list:%d\n",list_empty(&(current->mm.vm->vm_list)));
        //printf("2\n");
        struct vm_area_struct* vma;
        list_for_each_entry(vma, &current->mm.vm->vm_list, vm_list) 
        {
            if (vma->mapped == 1)//若已分配物理页
            {
                uint64_t pte = get_pte((current->satp & ((1ULL << 44) - 1)) << 12, vma->vm_start);
                free_pages((pte >> 10) << 12);
            }
            create_mapping((current->satp & ((1ULL << 44) - 1)) << 12, vma->vm_start, 0, (vma->vm_end - vma->vm_start), 0);
            list_del(&(vma->vm_list));
            kfree(vma);
        }

        //printf("has freeed vm\n");
        uint64_t new_sscratch = (uint64_t)0x1001000 + PAGE_SIZE;
        write_csr(sscratch, new_sscratch);//栈起始物理地址无需改变，将用户栈顶设回初始值即可(reset)
        uint64_t task_addr = get_program_address((const char*)arg0);
        current->mm.user_program_start = task_addr;

        create_mapping((current->satp & ((1ULL << 44) - 1)) << 12, 0x1000000, task_addr, PAGE_SIZE, PTE_V | PTE_R | PTE_X | PTE_U | PTE_W);

        sp_ptr[16] =0x1000000;

        asm volatile ("sfence.vma");

        //printf("exec success\n");
        break;
    }
    case SYS_EXIT: {
        // TODO:
        // 1. free current process vm_area_struct and it's mapping area
        // 2. free user stack
        // 3. clear current task, set current task->counter = 0
        // 4. call schedule
        struct vm_area_struct* vma;
        list_for_each_entry(vma, &current->mm.vm->vm_list, vm_list) 
        {
            if (vma->mapped == 1)//若已分配物理页
            {
                uint64_t pte = get_pte((current->satp & ((1ULL << 44) - 1)) << 12, vma->vm_start);
                free_pages((pte >> 10) << 12);
            }
            create_mapping((current->satp & ((1ULL << 44) - 1)) << 12, vma->vm_start, 0, (vma->vm_end - vma->vm_start), 0);
            list_del(&(vma->vm_list));
            kfree(vma);
        }

        free_pages(current->mm.user_stack);

        current->counter = 0;
        kfree(current->mm.vm);

        schedule();

        //printf("has exited\n");
        break;
    }
    case SYS_WAIT: {
        // TODO:
        // 1. find the process which pid == arg0
        // 2. if not find
        //   2.1. sepc += 4, return
        // 3. if find and counter = 0
        //   3.1. free it's kernel stack and page table
        // 4. if find and counter != 0
        //   4.1. change current process's priority
        //   4.2. call schedule to run other process
        //   4.3. goto 1. check again
        if(task[arg0]==0x0)
        {
            sp_ptr[16]+=4;
            break;
        }
        //printf("task[arg0]:0x%016lx\n",task[arg0]);
        while(1)
        {
            if(task[arg0]->counter==0)
            {
                // printf("sign\n");
                uint64_t pa=(get_pte((task[arg0]->satp & ((1ULL << 44) - 1)) << 12, task[arg0]) >> 10) << 12;
                free_pages((task[arg0]->satp & ((1ULL << 44) - 1)) << 12);
                free_pages(pa);//内核栈即为task_struct所在的这个页面
                task[arg0]=0x0;
                sp_ptr[16]+=4;
                //printf("wait success\n");
                break;
            }
            else
            {
                task[arg0]->priority=current->priority-1;//优先级设为比当前进程高
                uint64_t* child_sp=(uint64_t*)(task[arg0]->thread.sp);
                //printf("child process sepc:0x%016lx\n",child_sp[16]);
                //printf("child sscratech:0x%016lx\n",task[arg0]->sscratch);
                schedule();
            }
        }

        break;
    }
    case SYS_WRITE: {
        int fd = arg0;
        char* buffer = (char*)arg1;
        int size = arg2;
        if(fd == 1) {
            for(int i = 0; i < size; i++) {
                putchar(buffer[i]);
            }
        }
        ret.a0 = size;
        sp_ptr[4] = ret.a0;
        sp_ptr[16] += 4;
        break;
    }
    case SYS_MMAP: {
        struct vm_area_struct* vma = (struct vm_area_struct*)kmalloc(sizeof(struct vm_area_struct));
        if (vma == NULL) {
            ret.a0 = -1;
            break;
        }
        vma->vm_start = arg0;
        vma->vm_end = arg0 + arg1;
        vma->vm_flags = arg2;
        vma->mapped = 0;
        list_add(&(vma->vm_list), &(current->mm.vm->vm_list));

        ret.a0 = vma->vm_start;
        sp_ptr[16] += 4;
        break;
    }
    case SYS_MUNMAP: {
        ret.a0 = -1;
        struct vm_area_struct* vma;
        list_for_each_entry(vma, &current->mm.vm->vm_list, vm_list) {
            if (vma->vm_start == arg0 && vma->vm_end == arg0 + arg1) {
                if (vma->mapped == 1) {
                    uint64_t pte = get_pte((current->satp & ((1ULL << 44) - 1)) << 12, vma->vm_start);
                    free_pages((pte >> 10) << 12);
                }
                create_mapping((current->satp & ((1ULL << 44) - 1)) << 12, vma->vm_start, 0, (vma->vm_end - vma->vm_start), 0);
                list_del(&(vma->vm_list));
                kfree(vma);

                ret.a0 = 0;
                break;
            }
        }
        // flash the TLB
        asm volatile ("sfence.vma");
        sp_ptr[16] += 4;
        break;
    }
    default:
        printf("Unknown syscall! syscall_num = %d\n", syscall_num);
        while(1);
        break;
    }
    return ret;
}