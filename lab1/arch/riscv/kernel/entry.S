#ifndef PRINT_ONLY

.section .text.entry

.global trap_s
.extern handler_s
.equ reg_size, 0x8
.align 2

trap_s:
	# TODO: save the caller saved registers and sepc
	sd x1, 0(sp)
	sd x5, -8(sp)
	sd x6, -16(sp)
	sd x7, -24(sp)
	sd x10, -32(sp)
	sd x11, -40(sp)
	sd x12, -48(sp)
	sd x13, -56(sp)
	sd x14, -64(sp)
	sd x15, -72(sp)
	sd x16, -80(sp)
	sd x17, -88(sp)
	sd x28, -96(sp)
	sd x29, -104(sp)
	sd x30, -112(sp)
	sd x31, -120(sp)
	csrr t0,sepc
	sd t0,-128(sp)
	addi sp, sp, -136

	# TODO: call handler_s(scause)
	csrr a0,scause
	call handler_s

	# TODO: load sepc and caller saved registers
	ld t0,8(sp)
	csrw sepc,t0
	ld x31,16(sp)
	ld x30,24(sp)
	ld x29,32(sp)
	ld x28,40(sp)
	ld x17,48(sp)
	ld x16,56(sp)
	ld x15,64(sp)
	ld x14,72(sp)
	ld x13,80(sp)
	ld x12,88(sp)
	ld x11,96(sp)
	ld x10,104(sp)
	ld x7,112(sp)
	ld x6,120(sp)
	ld x5,128(sp)
	ld x1,136(sp)
	addi sp,sp,136
	
	sret

#endif