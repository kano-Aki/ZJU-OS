.section .text.entry

.extern test
.global trap_s
.extern handler_s
.equ reg_size, 0x8
.align 2

trap_s:
	# save the caller saved registers and sepc
	addi sp, sp, -reg_size*17

	sd ra, 0*reg_size(sp)
	sd t0, 1*reg_size(sp)
	sd t1, 2*reg_size(sp)
	sd t2, 3*reg_size(sp)
	sd a0, 4*reg_size(sp)
	sd a1, 5*reg_size(sp)
	sd a2, 6*reg_size(sp)
	sd a3, 7*reg_size(sp)
	sd a4, 8*reg_size(sp)
	sd a5, 9*reg_size(sp)
	sd a6, 10*reg_size(sp)
	sd a7, 11*reg_size(sp)
	sd t3, 12*reg_size(sp)
	sd t4, 13*reg_size(sp)
	sd t5, 14*reg_size(sp)
	sd t6, 15*reg_size(sp)
	csrr a0, sepc
	sd a0, 16*reg_size(sp)

	# call handler_s(scause)
	csrr a0, scause
	jal handler_s	#调用完handler_s后，若进程已经切换，则现在的sp为新进程的sp，恢复的上下文同样为新进程的
					#对于进程A，在前面保留了上下文，若在handler_s中切换为进程B，则下面恢复的为进程B上次切换时的上下文
					#当下次切换回进程A时，当时的上下文则被恢复，即下面各寄存器
					#若新的进程为第一次启动，则其栈上根本没有上下文
	# load sepc and caller saved registers
	ld ra, 0*reg_size(sp)
	ld t0, 1*reg_size(sp)
	ld t1, 2*reg_size(sp)
	ld t2, 3*reg_size(sp)
	ld a0, 4*reg_size(sp)
	ld a1, 5*reg_size(sp)
	ld a2, 6*reg_size(sp)
	ld a3, 7*reg_size(sp)
	ld a4, 8*reg_size(sp)
	ld a5, 9*reg_size(sp)
	ld a6, 10*reg_size(sp)
	ld a7, 11*reg_size(sp)
	ld t3, 12*reg_size(sp)
	ld t4, 13*reg_size(sp)
	ld t5, 14*reg_size(sp)
	ld t6, 16*reg_size(sp)
	csrw sepc, t6
	ld t6, 15*reg_size(sp)

	addi sp, sp, reg_size*17

	sret



.globl __switch_to
__switch_to:
	li    a4,  40 
	add   a3, a0, a4
	add   a4, a1, a4		#+40后为struct thread地址
	# TODO: Save context into prev->thread
	sd ra,0(a3)				#事实上，在这里存入struct thread的ra值应为&switch_to
	sd sp,reg_size(a3)
	sd s0,2*reg_size(a3)
	sd s1,3*reg_size(a3)
	sd s2,4*reg_size(a3)
	sd s3,5*reg_size(a3)
	sd s4,6*reg_size(a3)
	sd s5,7*reg_size(a3)
	sd s6,8*reg_size(a3)
	sd s7,9*reg_size(a3)
	sd s8,10*reg_size(a3)
	sd s9,11*reg_size(a3)
	sd s10,12*reg_size(a3)
	sd s11,13*reg_size(a3)	#caller saved寄存器在trap_s函数中已经保存，在切换这里保存ra、sp和callee saved

	# TODO: Restore context from next->thread
	ld ra,0(a4)				#在进程第一次启动时，我们已手动将栈中ra值设为__init_sepc，
							#而当之后该进程被切换时，在上面的入栈操作时，ra值已被更改为&switch_to，从而只有第一次启动会进入__init_sepc
	ld sp,reg_size(a4)
	ld s0,2*reg_size(a4)
	ld s1,3*reg_size(a4)
	ld s2,4*reg_size(a4)
	ld s3,5*reg_size(a4)
	ld s4,6*reg_size(a4)
	ld s5,7*reg_size(a4)
	ld s6,8*reg_size(a4)
	ld s7,9*reg_size(a4)
	ld s8,10*reg_size(a4)
	ld s9,11*reg_size(a4)
	ld s10,12*reg_size(a4)
	ld s11,13*reg_size(a4)
  
	# return to ra
	ret


.globl __init_sepc
__init_sepc:
  	# TODO
	la t0,test
	csrw sepc,t0
	sret