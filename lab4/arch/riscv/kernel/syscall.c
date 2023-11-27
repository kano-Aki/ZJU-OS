#include "syscall.h"

#include "task_manager.h"
#include "stdio.h"
#include "defs.h"


struct ret_info syscall(uint64_t syscall_num, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    // TODO: implement syscall function
    struct ret_info ret;
    switch (syscall_num) {
        case SYS_WRITE://利用putchar函数完成
            if(arg0 != 1){
                printf("Unknown Standard Output");
                while(1);
                break;
            }
            char* base = arg1;
            uint64_t size = arg2, count = 0;
            for(uint64_t i = 0; i < size && base[i] != 0; i++)
                putchar(base[i]), count++;
            ret.a0 = count;
            break;
        case SYS_GETPID:
            ret.a0 = current->pid;
            break;    
        default:
            printf("Unknown syscall! syscall_num = %d\n", syscall_num);
            while(1);
            break;
        }
    return ret;
}