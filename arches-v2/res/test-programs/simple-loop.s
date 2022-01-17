	.file	"simple-loop.cpp"
	.option nopic
	.option norelax
	.attribute arch, "rv64i2p0_m2p0_f2p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.section	.text.startup,"ax",@progbits
	.align	2
	.globl	main
	.type	main, @function
main:
.LFB0:
	.cfi_startproc
	addi	sp,sp,-16
	.cfi_def_cfa_offset 16
	sw	zero,12(sp)
	lw	a4,12(sp)
	li	a5,3
	bgtu	a4,a5,.L2
	lui	a2,%hi(.LANCHOR0)
	li	a3,-11128832
	addi	a2,a2,%lo(.LANCHOR0)
	addi	a3,a3,1042
	li	a1,3
.L3:
	lw	a5,12(sp)
	lw	a4,12(sp)
	slli	a0,a5,32
	addiw	a4,a4,1
	sw	a4,12(sp)
	srli	a5,a0,30
	lw	a4,12(sp)
	add	a5,a2,a5
	sw	a3,0(a5)
	bleu	a4,a1,.L3
.L2:
	li	a0,0
	addi	sp,sp,16
	.cfi_def_cfa_offset 0
	jr	ra
	.cfi_endproc
.LFE0:
	.size	main, .-main
	.globl	frame_buffer
	.bss
	.align	3
	.set	.LANCHOR0,. + 0
	.type	frame_buffer, @object
	.size	frame_buffer, 16
frame_buffer:
	.zero	16
	.ident	"GCC: (GNU) 11.1.0"
