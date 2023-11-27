#ifndef PRINT_ONLY
#include "defs.h"
#include "riscv.h"

extern main(), puts(), put_num(), ticks;
extern void clock_set_next_event(void);

void handler_s(uint64_t cause) {
  // TODO
  // interrupt
  //read_csr(scause);
  if ((cause>>63) == 1) {
    // supervisor timer interrupt
    if (cause==((1LL <<63)+5)) {//这里的1需设置为long long，否则认为int，只有32位，左移63位操作会出错
      // 设置下一个时钟中断，打印当前的中断数目
      // TODO
      put_num(ticks);//设置下一次时会++，所以先打印当前的
      clock_set_next_event();
    }
  }
}
#endif