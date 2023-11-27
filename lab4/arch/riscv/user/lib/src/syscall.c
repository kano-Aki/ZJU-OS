#include "syscall.h"


struct ret_info u_syscall(uint64_t syscall_num, uint64_t arg0, uint64_t arg1, uint64_t arg2, \
                uint64_t arg3, uint64_t arg4, uint64_t arg5){
    struct ret_info ret;
    // TODO: 完成系统调用，将syscall_num放在a7中，将参数放在a0-a5中，触发ecall，将返回值放在ret中
    __asm__ volatile(
    //汇编代码
    "mv a0, %[a0]\n"
    "mv a1, %[a1]\n"
    "mv a2, %[a2]\n"
    "mv a3, %[a3]\n"
    "mv a4, %[a4]\n"
    "mv a5, %[a5]\n"
    "mv a7, %[syscall_num]\n"
    "ecall\n"
    "mv %[return1], a0\n"
    "mv %[return2], a1"
    //返回值
    : [return1]"=r"(ret.a0), [return2]"=r"(ret.a1)
    //输入
    : [a0]"r"(arg0), [a1]"r"(arg1), [a2]"r"(arg2), [a3]"r"(arg3), [a4]"r"(arg4), [a5]"r"(arg5), [syscall_num]"r"(syscall_num)
    //可能影响的寄存器和储存器
    : "a0", "a1", "a2", "a3", "a4", "a5", "a7"
  );
    return ret;
}