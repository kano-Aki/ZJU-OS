#include "defs.h"
#include "sched.h"
#include "clock.h"
#include "stdio.h"

void handler_s(uint64_t cause, uint64_t epc) {
  // interrupt
  if (cause >> 63 == 1) {
    // supervisor timer interrupt
    if (cause == 0x8000000000000005) {
      // run do_timer() per 10 ticks, which means 0.1s.
      clock_set_next_event();
      if (ticks % 10 == 0) {
        printf("ticks: %d\n", ticks);

        do_timer();//即为 counter的单位为0.1s，每过0.1s，就会调用一次do_timer()，counter-1
      }
    }
  }
  else 

  return;
}
