	.file	"path-tracer.cpp"
	.option nopic
	.option norelax
	.attribute arch, "rv64i2p0_m2p0_a2p0_f2p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.align	2
	.type	_Z17intersect_bucketsPvPK7TreeletP3Hit.isra.0, @function
_Z17intersect_bucketsPvPK7TreeletP3Hit.isra.0:
.LFB2643:
	.cfi_startproc
	addi	sp,sp,-608
	.cfi_def_cfa_offset 608
	lui	a5,%hi(.LC2)
	fsw	fs6,548(sp)
	.cfi_offset 54, -60
	flw	fs6,%lo(.LC2)(a5)
	lui	a5,%hi(.LC1)
	flw	ft9,%lo(.LC1)(a5)
	lui	a5,%hi(.LC0)
	fsw	fs7,544(sp)
	.cfi_offset 55, -64
	flw	fs7,%lo(.LC0)(a5)
	li	t0,7
	li	t6,985964544
	sd	ra,600(sp)
	mv	a6,a0
	sd	s0,592(sp)
	sd	s1,584(sp)
	fsw	fs0,572(sp)
	fsw	fs1,568(sp)
	fsw	fs2,564(sp)
	fsw	fs3,560(sp)
	fsw	fs4,556(sp)
	fsw	fs5,552(sp)
	fsw	fs8,540(sp)
	fsw	fs9,536(sp)
	fsw	fs10,532(sp)
	.cfi_offset 1, -8
	.cfi_offset 8, -16
	.cfi_offset 9, -24
	.cfi_offset 40, -36
	.cfi_offset 41, -40
	.cfi_offset 50, -44
	.cfi_offset 51, -48
	.cfi_offset 52, -52
	.cfi_offset 53, -56
	.cfi_offset 56, -68
	.cfi_offset 57, -72
	.cfi_offset 58, -76
	mv	t2,a1
	mv	ra,a2
	li	t4,0
	addi	a0,sp,528
	slli	t0,t0,34
	addi	t6,t6,-1114
	li	t1,-1
.L22:
 #APP
# 263 "./src/intersect.hpp" 1
	lbray ft0, 0(a6)
# 0 "" 2
 #NO_APP
	fdiv.s	ft8,fs6,ft3
	fmv.x.w	a5,ft7
	fmv.s	ft10,ft3
	fmv.s	ft11,ft4
	slli	a4,a5,32
	fmv.s	fs5,ft5
	fmv.x.w	t5,ft6
	srli	a5,a4,16
	add	a1,t2,a5
	addi	a5,sp,272
	fdiv.s	fs4,fs6,ft4
	fdiv.s	fs8,fs6,ft5
.L2:
	fsw	ft9,0(a5)
	addi	a5,a5,8
	bne	a5,a0,.L2
	ld	a5,272(sp)
	fmv.w.x	fs3,zero
	fmv.s	fa4,fs7
	and	a5,a5,t0
	fmv.s	fa6,fs3
	fmv.s	fa5,ft9
	or	a5,a5,t6
	sd	a5,272(sp)
	li	a7,0
	li	t3,0
	li	a3,0
.L19:
	slli	a4,a3,32
	srli	a5,a4,29
	addi	a5,a5,528
	add	a5,a5,sp
	ld	a4,-256(a5)
	fle.s	a5,fa5,fa4
	addiw	a3,a3,-1
	sd	a4,8(sp)
	beq	a5,zero,.L42
.L3:
	beq	a3,t1,.L18
.L43:
	slli	a4,a3,32
	srli	a5,a4,29
	addi	a5,a5,528
	add	a5,a5,sp
	flw	fa4,-256(a5)
	j	.L19
.L42:
	lw	a4,12(sp)
	andi	a5,a4,1
	beq	a5,zero,.L15
	j	.L4
.L6:
	fgt.s	a5,fa5,fa1
	beq	a5,zero,.L9
	addiw	a3,a3,1
	slli	s0,a3,32
	srli	a5,s0,29
	addi	a5,a5,528
	add	a5,a5,sp
	fsw	fa1,-256(a5)
	sw	a2,-252(a5)
.L9:
	fgt.s	a5,fa5,fa4
	beq	a5,zero,.L3
	sw	a4,12(sp)
	andi	a5,a4,1
	bne	a5,zero,.L4
.L15:
	ld	a5,8(sp)
	andi	a4,a4,2
	srli	a5,a5,37
	bne	a4,zero,.L5
	slli	a5,a5,2
	add	a5,a1,a5
	lw	a4,24(a5)
	flw	fs10,32(a5)
	flw	fs9,36(a5)
	flw	fs2,40(a5)
	flw	fa7,44(a5)
	flw	fa2,48(a5)
	flw	fa3,52(a5)
	lw	a2,56(a5)
	fmv.s	ft3,ft8
	fmv.s	ft4,fs4
	fmv.s	ft5,fs8
	flw	ft6,0(a5)
	flw	ft7,4(a5)
	flw	fs0,8(a5)
	flw	fs1,12(a5)
	flw	fa0,16(a5)
	flw	fa1,20(a5)
 #APP
# 26 "./src/intersect.hpp" 1
	boxisect fa4
# 0 "" 2
 #NO_APP
	fmv.s	ft6,fs10
	fmv.s	ft7,fs9
	fmv.s	fs0,fs2
	fmv.s	fs1,fa7
	fmv.s	fa0,fa2
	fmv.s	fa1,fa3
 #APP
# 26 "./src/intersect.hpp" 1
	boxisect fa1
# 0 "" 2
 #NO_APP
	flt.s	a5,fa4,fa1
	bne	a5,zero,.L6
	fgt.s	a5,fa5,fa4
	beq	a5,zero,.L13
	addiw	a3,a3,1
	slli	s0,a3,32
	srli	a5,s0,29
	addi	a5,a5,528
	add	a5,a5,sp
	fsw	fa4,-256(a5)
	sw	a4,-252(a5)
.L13:
	fgt.s	a5,fa5,fa1
	beq	a5,zero,.L3
	mv	a4,a2
	sw	a4,12(sp)
	andi	a5,a4,1
	beq	a5,zero,.L15
.L4:
	ld	s0,8(sp)
	li	a2,0
	srli	a5,s0,37
	slli	a5,a5,2
	srli	s0,s0,34
	add	a5,a1,a5
	andi	s0,s0,7
.L17:
	lw	s1,36(a5)
	fmv.s	ft3,ft10
	fmv.s	ft4,ft11
	fmv.s	ft5,fs5
	flw	ft6,0(a5)
	flw	ft7,4(a5)
	flw	fs0,8(a5)
	flw	fs1,12(a5)
	flw	fa0,16(a5)
	flw	fa1,20(a5)
	flw	fa2,24(a5)
	flw	fa3,28(a5)
	flw	fa4,32(a5)
	fmv.s	fa7,fs3
 #APP
# 90 "./src/intersect.hpp" 1
	triisect a4
# 0 "" 2
 #NO_APP
	sext.w	a4,a4
	fmv.s	fs3,fa7
	addiw	a2,a2,1
	beq	a4,zero,.L16
	mv	t4,s1
	li	t3,1
.L16:
	addi	a5,a5,40
	bgeu	s0,a2,.L17
	bne	a3,t1,.L43
.L18:
	beq	t3,zero,.L20
	slli	a4,t5,32
	srli	a5,a4,28
	add	a5,ra,a5
	fmv.s	fa7,fa5
	fmv.s	fs2,fa6
	fmv.w.x	fs4,t4
 #APP
# 341 "./src/intersect.hpp" 1
	sbray fa7, 0(a5)
# 0 "" 2
 #NO_APP
.L20:
	beq	a7,zero,.L22
	addiw	a5,a7,-1
	slli	a4,a5,32
	srli	a5,a4,30
	addi	a3,sp,20
	addi	a4,sp,16
	add	a5,a5,a3
.L23:
	fmv.s	ft3,ft10
	fmv.s	ft4,ft11
	fmv.s	ft5,fs5
	fmv.w.x	ft6,t5
	flw	ft7,0(a4)
 #APP
# 313 "./src/intersect.hpp" 1
	sbray ft0, 0(a6)
# 0 "" 2
 #NO_APP
	addi	a4,a4,4
	bne	a4,a5,.L23
	j	.L22
.L5:
	slli	a2,a7,32
	srli	a4,a2,30
	addi	a4,a4,528
	add	a4,a4,sp
	sw	a5,-512(a4)
	addiw	a7,a7,1
	j	.L3
	.cfi_endproc
.LFE2643:
	.size	_Z17intersect_bucketsPvPK7TreeletP3Hit.isra.0, .-_Z17intersect_bucketsPvPK7TreeletP3Hit.isra.0
	.align	2
	.globl	_Z6_lbrayPKvRj
	.type	_Z6_lbrayPKvRj, @function
_Z6_lbrayPKvRj:
.LFB2589:
	.cfi_startproc
 #APP
# 263 "./src/intersect.hpp" 1
	lbray ft0, 0(a1)
# 0 "" 2
 #NO_APP
	fsw	ft0,0(a0)
	fsw	ft1,4(a0)
	fsw	ft2,8(a0)
	fsw	ft3,12(a0)
	fsw	ft4,16(a0)
	fsw	ft5,20(a0)
	fsw	ft6,24(a0)
	fsw	ft7,0(a2)
	ret
	.cfi_endproc
.LFE2589:
	.size	_Z6_lbrayPKvRj, .-_Z6_lbrayPKvRj
	.align	2
	.globl	_Z6_sbrayRK9BucketRayjPKv
	.type	_Z6_sbrayRK9BucketRayjPKv, @function
_Z6_sbrayRK9BucketRayjPKv:
.LFB2596:
	.cfi_startproc
	flw	ft0,0(a0)
	flw	ft1,4(a0)
	flw	ft2,8(a0)
	flw	ft3,12(a0)
	flw	ft4,16(a0)
	flw	ft5,20(a0)
	flw	ft6,24(a0)
	fmv.w.x	ft7,a1
 #APP
# 313 "./src/intersect.hpp" 1
	sbray ft0, 0(a2)
# 0 "" 2
 #NO_APP
	ret
	.cfi_endproc
.LFE2596:
	.size	_Z6_sbrayRK9BucketRayjPKv, .-_Z6_sbrayRK9BucketRayjPKv
	.align	2
	.globl	_Z6_cshitRK3HitPS_
	.type	_Z6_cshitRK3HitPS_, @function
_Z6_cshitRK3HitPS_:
.LFB2597:
	.cfi_startproc
	addi	sp,sp,-16
	.cfi_def_cfa_offset 16
	fsw	fs2,12(sp)
	fsw	fs3,8(sp)
	fsw	fs4,4(sp)
	.cfi_offset 50, -4
	.cfi_offset 51, -8
	.cfi_offset 52, -12
	flw	fa7,0(a0)
	flw	fs2,4(a0)
	flw	fs3,8(a0)
	flw	fs4,12(a0)
 #APP
# 341 "./src/intersect.hpp" 1
	sbray fa7, 0(a1)
# 0 "" 2
 #NO_APP
	flw	fs2,12(sp)
	.cfi_restore 50
	flw	fs3,8(sp)
	.cfi_restore 51
	flw	fs4,4(sp)
	.cfi_restore 52
	addi	sp,sp,16
	.cfi_def_cfa_offset 0
	jr	ra
	.cfi_endproc
.LFE2597:
	.size	_Z6_cshitRK3HitPS_, .-_Z6_cshitRK3HitPS_
	.section	.text.startup,"ax",@progbits
	.align	2
	.globl	main
	.type	main, @function
main:
.LFB2610:
	.cfi_startproc
	li	a4,1
	li	a3,128
	amoadd.w a5,a4,0(a3)
	lw	a4,264(zero)
	sext.w	a5,a5
	bleu	a4,a5,.L52
	lui	a4,%hi(.LC3)
	flw	fa0,%lo(.LC3)(a4)
	lui	a4,%hi(.LC4)
	flw	fa1,%lo(.LC4)(a4)
	lui	a4,%hi(.LC1)
	flw	fa6,%lo(.LC1)(a4)
	li	a2,1
.L49:
	lw	a4,256(zero)
	flw	fa4,304(zero)
	flw	ft0,300(zero)
	divuw	a1,a5,a4
	flw	ft3,320(zero)
	flw	ft4,324(zero)
	mv	a3,a5
	remuw	a4,a5,a4
	fcvt.s.wu	fa5,a1
	fadd.s	fa5,fa5,fa0
	fmadd.s	fa5,fa5,fa4,fa1
	flw	fa4,336(zero)
	fmul.s	fa2,fa5,fa4
	flw	fa4,332(zero)
	fmul.s	fa3,fa5,fa4
	flw	fa4,340(zero)
	fmul.s	fa4,fa5,fa4
	fcvt.s.wu	fa5,a4
	fadd.s	fa5,fa5,fa0
	fmadd.s	fa5,fa5,ft0,fa1
	fmadd.s	ft3,fa5,ft3,fa3
	flw	fa3,328(zero)
	fmadd.s	ft4,fa5,ft4,fa2
	fmadd.s	fa5,fa5,fa3,fa4
	flw	fa4,348(zero)
	fsub.s	ft4,ft4,fa4
	flw	fa4,344(zero)
	fsub.s	ft3,ft3,fa4
	fmul.s	ft5,ft4,ft4
	flw	fa4,352(zero)
	fsub.s	fa5,fa5,fa4
	fmadd.s	ft5,ft3,ft3,ft5
	fmadd.s	ft5,fa5,fa5,ft5
 #APP
# 45 "./src/stdafx.hpp" 1
	fsqrt.s ft5, ft5
	
# 0 "" 2
 #NO_APP
	slli	a1,a5,32
	ld	a5,384(zero)
	srli	a4,a1,28
	fdiv.s	ft3,ft3,ft5
	add	a5,a5,a4
	flw	ft0,308(zero)
	ld	a4,400(zero)
	flw	ft1,312(zero)
	flw	ft2,316(zero)
	fmv.w.x	ft6,a3
	fmv.w.x	ft7,zero
	fsw	fa6,0(a5)
	fdiv.s	ft4,ft4,ft5
	fdiv.s	ft5,fa5,ft5
 #APP
# 313 "./src/intersect.hpp" 1
	sbray ft0, 0(a4)
# 0 "" 2
 #NO_APP
	li	a4,128
	amoadd.w a5,a2,0(a4)
	lw	a4,264(zero)
	sext.w	a5,a5
	bgtu	a4,a5,.L49
.L52:
	li	a4,1
	li	a3,132
	amoadd.w a5,a4,0(a3)
	lw	a3,264(zero)
	sext.w	a5,a5
	bgtu	a3,a5,.L66
	li	a3,136
	amoadd.w a5,a4,0(a3)
	lw	a4,264(zero)
	sext.w	a5,a5
	bleu	a4,a5,.L57
	lui	a4,%hi(.LC2)
	flw	fa4,%lo(.LC2)(a4)
	lui	a4,%hi(.LC5)
	flw	fa1,%lo(.LC5)(a4)
	lui	a4,%hi(.LC6)
	flw	fa2,%lo(.LC6)(a4)
	lui	a4,%hi(.LC3)
	flw	fa3,%lo(.LC3)(a4)
	li	a1,-1
	li	a6,-16777216
	li	a2,1
	li	a0,-16777216
	j	.L53
.L67:
	fmin.s	fa5,fa5,fa4
	ld	a4,272(zero)
	slli	a5,a5,2
	fmul.s	fa5,fa5,fa1
	add	a4,a4,a5
	fmin.s	fa5,fa5,fa4
	fmadd.s	fa5,fa5,fa2,fa3
	fcvt.wu.s a3,fa5,rtz
	slliw	a5,a3,8
	slliw	a7,a3,16
	or	a5,a5,a7
	or	a5,a5,a3
	or	a5,a5,a0
	sw	a5,0(a4)
	li	a4,136
	amoadd.w a5,a2,0(a4)
	lw	a4,264(zero)
	sext.w	a5,a5
	bleu	a4,a5,.L57
.L53:
	ld	a4,384(zero)
	slli	a5,a5,32
	srli	a5,a5,32
	slli	a3,a5,4
	add	a4,a4,a3
	lw	a3,12(a4)
	flw	fa5,0(a4)
	bne	a3,a1,.L67
	ld	a4,272(zero)
	slli	a5,a5,2
	add	a5,a4,a5
	sw	a6,0(a5)
	li	a4,136
	amoadd.w a5,a2,0(a4)
	lw	a4,264(zero)
	sext.w	a5,a5
	bgtu	a4,a5,.L53
.L57:
	li	a0,0
	ret
.L66:
	ld	a2,384(zero)
	ld	a1,392(zero)
	ld	a0,400(zero)
	addi	sp,sp,-16
	.cfi_def_cfa_offset 16
	sd	ra,8(sp)
	.cfi_offset 1, -8
	call	_Z17intersect_bucketsPvPK7TreeletP3Hit.isra.0
	.cfi_endproc
.LFE2610:
	.size	main, .-main
	.section	.srodata.cst4,"aM",@progbits,4
	.align	2
.LC0:
	.word	985963430
	.align	2
.LC1:
	.word	1333788672
	.align	2
.LC2:
	.word	1065353216
	.align	2
.LC3:
	.word	1056964608
	.align	2
.LC4:
	.word	-1082130432
	.align	2
.LC5:
	.word	1061997773
	.align	2
.LC6:
	.word	1132396544
	.ident	"GCC: (g5964b5cd727) 11.1.0"
