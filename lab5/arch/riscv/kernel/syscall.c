#include "syscall.h"

#include "task_manager.h"
#include "stdio.h"
#include "defs.h"
#include "slub.h"
#include "mm.h"


struct ret_info syscall(uint64_t syscall_num, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    struct ret_info ret;
    switch (syscall_num) {
    case SYS_GETPID: {
        ret.a0 = getpid();
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
        break;
    }
    case SYS_MMAP: {
        // TODO: implement mmap
        // 1. create a new vma struct (kmalloc), if kmalloc failed, return -1
        // 2. initialize the vma struct
        // 2.1 set the vm_start and vm_end according to arg0 and arg1
        // 2.2 set the vm_flags to arg2
        // 2.3 set the mapped flag to 0
        // 3. add the vma struct to the mm_struct's vma list
        // return the vm_start
        struct vm_area_struct *vma = (struct vm_area_struct *)kmalloc(sizeof(struct vm_area_struct));
        if(vma == NULL) {
            ret.a0 = -1;
            break;
        }
        //打印vma值
        //printf("vma:0x%016lx\n",arg0);
        INIT_LIST_HEAD(&vma->vm_list);
        vma->vm_start=arg0-arg0%PAGE_SIZE;//向前对齐
        vma->vm_end=vma->vm_start+(arg1-1)/PAGE_SIZE*PAGE_SIZE+PAGE_SIZE-1;//向后对齐。start+len-1才是end值
        vma->vm_flags=arg2;
        vma->mapped=0;
        //printf("vm_start:0x%016lx\n",vma->vm_start);
        list_add(&(vma->vm_list),&(current->mm.vm->vm_list));
        ret.a0=vma->vm_start;
        
        break;

    }
    case SYS_MUNMAP: {
        // TODO: implement munmap
        // 1. find the vma according to arg0 and arg1
        // note: you can iterate the vm_list by list_for_each_entry(vma, &current->mm.vm->vm_list, vm_list), then you can use `vma` in your loop
        // 2. if the vma mapped, free the physical pages (free_pages). Using `get_pte` to get the corresponding pte.
        // 3. ummap the physical pages from the virtual address space (create_mapping)
        // 4. delete the vma from the mm_struct's vma list (list_del).
        // 5. free the vma struct (kfree).
        // return 0 if success, otherwise return -1


        // flash the TLB
        uint64_t start=arg0, end=arg0+arg1-1,sign=-1;
        struct vm_area_struct *vma;
        list_for_each_entry(vma, &current->mm.vm->vm_list, vm_list)
        {
            if(vma->vm_start==start && vma->vm_end==end)
            {
                if(vma->mapped)
                {
                    //打印vma->vm_start
                    //printf("vma->vm_start:0x%016lx\n",vma->vm_start);
                    //printf("vma->vm_end:0x%016lx\n",vma->vm_end);
                    uint64_t* page_table=(current->satp & 0x00000fffffffffff)<<12;
                    uint64_t pa=((get_pte(page_table,vma->vm_start)>>10)<<12) | (vma->vm_start & 0xfff);//get_pte得到的是页号,[53:10]才是PPN
                    //printf("in_unmap_pa:0x%016lx\n",pa);
                    free_pages(pa);
                    create_mapping(page_table,vma->vm_start,pa,end-start+1,0);//将这部分va的页表项值V位置0
                }
                //printf("unmap success x%016lx\n",vma->vm_start);
                list_del(&vma->vm_list);
                kfree(vma);
                sign=0;
                break;
            }
        }
        ret.a0=sign;
        asm volatile ("sfence.vma");
        break;


    }
    default:
        printf("Unknown syscall! syscall_num = %d\n", syscall_num);
        while(1);
        break;
    }
    return ret;
}
