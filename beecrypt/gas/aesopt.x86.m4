include(config.m4)
include(ASM_SRCDIR/asmdefs.m4)
include(ASM_SRCDIR/x86.m4)

ifdef(`USE_MMX',`

define(`s0',`%mm0')
define(`s1',`%mm1')
define(`s2',`%mm2')
define(`s3',`%mm3')
define(`t0',`%mm4')
define(`t1',`%mm5')
define(`t2',`%mm6')
define(`t3',`%mm7')

	.section	.rodata

	.p2align 4
LOCAL(mask000000FF):
	.quad 0x00000000000000ff
LOCAL(mask0000FF00):
	.quad 0x000000000000ff00
LOCAL(mask00FF0000):
	.quad 0x0000000000ff0000
LOCAL(maskFF000000):
	.quad 0x00000000ff000000

define(`sxrk',`
	movl   (%esi),%eax
	xorl   (%ebp),%eax
	movd %eax,s0
	movl  4(%esi),%ebx
	xorl  4(%ebp),%ebx
	movd %ebx,s1
	movl  8(%esi),%ecx
	xorl  8(%ebp),%ecx
	movd %ecx,s2
	movl 12(%esi),%edx
	xorl 12(%ebp),%edx
	movd %edx,s3
')

define(`etfs',`
	movd $1+ 0(%ebp),t0
	movd s0,%eax
	movd $1+ 4(%ebp),t1
	movd s1,%ebx
	movd $1+ 8(%ebp),t2
	movzbl %al,%ecx
	movd $1+12(%ebp),t3
	movzbl %bl,%edx
	pxor 0(%esi,%ecx,8),t0
	movzbl %ah,%ecx
	pxor 0(%esi,%edx,8),t1
	movzbl %bh,%edx
	pxor 2048(%esi,%ecx,8),t3
	shrl `$'16,%eax
	pxor 2048(%esi,%edx,8),t0
	movzbl %al,%ecx
	shrl `$'16,%ebx
	pxor 4096(%esi,%ecx,8),t2
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 4096(%esi,%edx,8),t3
	movzbl %bh,%edx
	pxor 6144(%esi,%ecx,8),t1
	movd s2,%eax
	pxor 6144(%esi,%edx,8),t2
	movzbl %al,%ecx
	movd s3,%ebx
	pxor 0(%esi,%ecx,8),t2
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 0(%esi,%edx,8),t3
	movzbl %bh,%edx
	pxor 2048(%esi,%ecx,8),t1
	shrl `$'16,%eax
	pxor 2048(%esi,%edx,8),t2
	movzbl %al,%ecx
	shrl `$'16,%ebx
	pxor 4096(%esi,%ecx,8),t0
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 4096(%esi,%edx,8),t1
	movzbl %bh,%edx
	pxor 6144(%esi,%ecx,8),t3
	pxor 6144(%esi,%edx,8),t0
')

define(`esft',`
	movd $1+ 0(%ebp),s0
	movd t0,%eax
	movd $1+ 4(%ebp),s1
	movd t1,%ebx
	movd $1+ 8(%ebp),s2
	movzbl %al,%ecx
	movd $1+12(%ebp),s3
	movzbl %bl,%edx
	pxor 0(%esi,%ecx,8),s0
	movzbl %ah,%ecx
	pxor 0(%esi,%edx,8),s1
	movzbl %bh,%edx
	pxor 2048(%esi,%ecx,8),s3
	shrl `$'16,%eax
	pxor 2048(%esi,%edx,8),s0
	movzbl %al,%ecx
	shrl `$'16,%ebx
	pxor 4096(%esi,%ecx,8),s2
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 4096(%esi,%edx,8),s3
	movzbl %bh,%edx
	pxor 6144(%esi,%ecx,8),s1
	movd t2,%eax
	pxor 6144(%esi,%edx,8),s2
	movzbl %al,%ecx
	movd t3,%ebx
	pxor 0(%esi,%ecx,8),s2
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 0(%esi,%edx,8),s3
	movzbl %bh,%edx
	pxor 2048(%esi,%ecx,8),s1
	shrl `$'16,%eax
	pxor 2048(%esi,%edx,8),s2
	movzbl %al,%ecx
	shrl `$'16,%ebx
	pxor 4096(%esi,%ecx,8),s0
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 4096(%esi,%edx,8),s1
	movzbl %bh,%edx
	pxor 6144(%esi,%ecx,8),s3
	pxor 6144(%esi,%edx,8),s0
')

define(`elr',`
	movd  0(%ebp),s0
	movd t0,%eax
	movd  4(%ebp),s1
	movd t1,%ebx
	movd  8(%ebp),s2
	movzbl %al,%ecx
	movd 12(%ebp),s3
	movzbl %bl,%edx
	movd 8192(%esi,%ecx,4),t0
	movzbl %ah,%ecx
	movd 8192(%esi,%edx,4),t1
	movzbl %bh,%edx
	pand LOCAL(mask000000FF),t0
	pand LOCAL(mask000000FF),t1
	pxor t0,s0
	movd 8192(%esi,%ecx,4),t0
	pxor t1,s1
	movd 8192(%esi,%edx,4),t1
	pand LOCAL(mask0000FF00),t0
	shrl `$'16,%eax
	pand LOCAL(mask0000FF00),t1
	shrl `$'16,%ebx
	pxor t0,s3
	movzbl %al,%ecx
	pxor t1,s0
	movzbl %bl,%edx
	movd 8192(%esi,%ecx,4),t0
	movzbl %ah,%ecx
	movd 8192(%esi,%edx,4),t1
	pand LOCAL(mask00FF0000),t0
	movzbl %bh,%edx
	pand LOCAL(mask00FF0000),t1
	pxor t0,s2
	movd 8192(%esi,%ecx,4),t0
	pxor t1,s3
	movd 8192(%esi,%edx,4),t1
	movd t2,%eax
	pand LOCAL(maskFF000000),t0
	movd t3,%ebx
	pand LOCAL(maskFF000000),t1
	pxor t0,s1
	movzbl %al,%ecx
	pxor t1,s2
	movzbl %bl,%edx
	movd 8192(%esi,%ecx,4),t2
	movzbl %ah,%ecx
	movd 8192(%esi,%edx,4),t3
	movzbl %bh,%edx
	pand LOCAL(mask000000FF),t2
	pand LOCAL(mask000000FF),t3
	pxor t2,s2
	movd 8192(%esi,%ecx,4),t2
	pxor t3,s3
	movd 8192(%esi,%edx,4),t3
	pand LOCAL(mask0000FF00),t2
	shrl `$'16,%eax
	pand LOCAL(mask0000FF00),t3
	shrl `$'16,%ebx
	pxor t2,s1
	movzbl %al,%ecx
	pxor t3,s2
	movzbl %bl,%edx
	movd 8192(%esi,%ecx,4),t2
	movzbl %ah,%ecx
	movd 8192(%esi,%edx,4),t3
	pand LOCAL(mask00FF0000),t2
	movzbl %bh,%edx
	pand LOCAL(mask00FF0000),t3
	pxor t2,s0
	movd 8192(%esi,%ecx,4),t2
	pxor t3,s1
	movd 8192(%esi,%edx,4),t3
	pand LOCAL(maskFF000000),t2
	pand LOCAL(maskFF000000),t3
	pxor t2,s3
	pxor t3,s0
')

define(`eblock',`
	sxrk

	movl `$'SYMNAME(_ae0),%esi

	etfs(16)
	esft(32)
	etfs(48)
	esft(64)
	etfs(80)
	esft(96)
	etfs(112)
	esft(128)
	etfs(144)

	movl 256(%ebp),%eax
	cmp `$'10,%eax
	je $1

	esft(160)
	etfs(176)

	movl 256(%ebp),%eax
	cmp `$'12,%eax
	je $1

	esft(192)
	etfs(208)

	movl 256(%ebp),%eax

	.align 4
$1:
	sall `$'4,%eax
	addl %eax,%ebp

	elr
')


C_FUNCTION_BEGIN(aesEncrypt)
	pushl %edi
	pushl %esi
	pushl %ebp
	pushl %ebx

	movl 20(%esp),%ebp
	movl 24(%esp),%edi
	movl 28(%esp),%esi

	eblock(LOCAL(00))

	movd s0, 0(%edi)
	movd s1, 4(%edi)
	movd s2, 8(%edi)
	movd s3,12(%edi)

	xorl %eax,%eax
	emms 

	popl %ebx
	popl %ebp
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(aesEncrypt)


C_FUNCTION_BEGIN(aesEncryptECB)
	pushl %edi
	pushl %esi
	pushl %ebp
	pushl %ebx

	movl 24(%esp),%edi
	movl 28(%esp),%esi

	.p2align 4,,15
LOCAL(aesEncryptECB_loop):
	movl 20(%esp),%ebp

	eblock(LOCAL(01))

	movd s0, 0(%edi)
	movd s1, 4(%edi)
	movd s2, 8(%edi)
	movd s3,12(%edi)

	addl `$'16,%esi
	addl `$'16,%edi
	decl 32(%esp)
	jnz LOCAL(aesEncryptECB_loop)

	xorl %eax,%eax
	emms 

	popl %ebx
	popl %ebp
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(aesEncryptECB)


define(`dtfs',`
	movd $1+ 0(%ebp),t0
	movd s0,%eax
	movd $1+ 4(%ebp),t1
	movd s1,%ebx
	movd $1+ 8(%ebp),t2
	movzbl %al,%ecx
	movd $1+12(%ebp),t3
	movzbl %bl,%edx
	pxor 0(%esi,%ecx,8),t0
	movzbl %ah,%ecx
	pxor 0(%esi,%edx,8),t1
	movzbl %bh,%edx
	pxor 2048(%esi,%ecx,8),t1
	shrl `$'16,%eax
	pxor 2048(%esi,%edx,8),t2
	movzbl %al,%ecx
	shrl `$'16,%ebx
	pxor 4096(%esi,%ecx,8),t2
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 4096(%esi,%edx,8),t3
	movzbl %bh,%edx
	pxor 6144(%esi,%ecx,8),t3
	movd s2,%eax
	pxor 6144(%esi,%edx,8),t0
	movzbl %al,%ecx
	movd s3,%ebx
	pxor 0(%esi,%ecx,8),t2
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 0(%esi,%edx,8),t3
	movzbl %bh,%edx
	pxor 2048(%esi,%ecx,8),t3
	shrl `$'16,%eax
	pxor 2048(%esi,%edx,8),t0
	movzbl %al,%ecx
	shrl `$'16,%ebx
	pxor 4096(%esi,%ecx,8),t0
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 4096(%esi,%edx,8),t1
	movzbl %bh,%edx
	pxor 6144(%esi,%ecx,8),t1
	pxor 6144(%esi,%edx,8),t2
')

define(`dsft',`
	movd $1+ 0(%ebp),s0
	movd t0,%eax
	movd $1+ 4(%ebp),s1
	movd t1,%ebx
	movd $1+ 8(%ebp),s2
	movzbl %al,%ecx
	movd $1+12(%ebp),s3
	movzbl %bl,%edx
	pxor 0(%esi,%ecx,8),s0
	movzbl %ah,%ecx
	pxor 0(%esi,%edx,8),s1
	movzbl %bh,%edx
	pxor 2048(%esi,%ecx,8),s1
	shrl `$'16,%eax
	pxor 2048(%esi,%edx,8),s2
	movzbl %al,%ecx
	shrl `$'16,%ebx
	pxor 4096(%esi,%ecx,8),s2
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 4096(%esi,%edx,8),s3
	movzbl %bh,%edx
	pxor 6144(%esi,%ecx,8),s3
	movd t2,%eax
	pxor 6144(%esi,%edx,8),s0
	movzbl %al,%ecx
	movd t3,%ebx
	pxor 0(%esi,%ecx,8),s2
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 0(%esi,%edx,8),s3
	movzbl %bh,%edx
	pxor 2048(%esi,%ecx,8),s3
	shrl `$'16,%eax
	pxor 2048(%esi,%edx,8),s0
	movzbl %al,%ecx
	shrl `$'16,%ebx
	pxor 4096(%esi,%ecx,8),s0
	movzbl %bl,%edx
	movzbl %ah,%ecx
	pxor 4096(%esi,%edx,8),s1
	movzbl %bh,%edx
	pxor 6144(%esi,%ecx,8),s1
	pxor 6144(%esi,%edx,8),s2
')

define(`dlr',`
	movd  0(%ebp),s0
	movd t0,%eax
	movd  4(%ebp),s1
	movd t1,%ebx
	movd  8(%ebp),s2
	movzbl %al,%ecx
	movd 12(%ebp),s3
	movzbl %bl,%edx
	movd 8192(%esi,%ecx,4),t0
	movzbl %ah,%ecx
	movd 8192(%esi,%edx,4),t1
	movzbl %bh,%edx
	pand LOCAL(mask000000FF),t0
	pand LOCAL(mask000000FF),t1
	pxor t0,s0
	movd 8192(%esi,%ecx,4),t0
	pxor t1,s1
	movd 8192(%esi,%edx,4),t1
	pand LOCAL(mask0000FF00),t0
	shrl `$'16,%eax
	pand LOCAL(mask0000FF00),t1
	shrl `$'16,%ebx
	pxor t0,s1
	movzbl %al,%ecx
	pxor t1,s2
	movzbl %bl,%edx
	movd 8192(%esi,%ecx,4),t0
	movzbl %ah,%ecx
	movd 8192(%esi,%edx,4),t1
	pand LOCAL(mask00FF0000),t0
	movzbl %bh,%edx
	pand LOCAL(mask00FF0000),t1
	pxor t0,s2
	movd 8192(%esi,%ecx,4),t0
	pxor t1,s3
	movd 8192(%esi,%edx,4),t1
	movd t2,%eax
	pand LOCAL(maskFF000000),t0
	movd t3,%ebx
	pand LOCAL(maskFF000000),t1
	pxor t0,s3
	movzbl %al,%ecx
	pxor t1,s0
	movzbl %bl,%edx
	movd 8192(%esi,%ecx,4),t2
	movzbl %ah,%ecx
	movd 8192(%esi,%edx,4),t3
	movzbl %bh,%edx
	pand LOCAL(mask000000FF),t2
	pand LOCAL(mask000000FF),t3
	pxor t2,s2
	movd 8192(%esi,%ecx,4),t2
	pxor t3,s3
	movd 8192(%esi,%edx,4),t3
	pand LOCAL(mask0000FF00),t2
	shrl `$'16,%eax
	pand LOCAL(mask0000FF00),t3
	shrl `$'16,%ebx
	pxor t2,s3
	movzbl %al,%ecx
	pxor t3,s0
	movzbl %bl,%edx
	movd 8192(%esi,%ecx,4),t2
	movzbl %ah,%ecx
	movd 8192(%esi,%edx,4),t3
	pand LOCAL(mask00FF0000),t2
	movzbl %bh,%edx
	pand LOCAL(mask00FF0000),t3
	pxor t2,s0
	movd 8192(%esi,%ecx,4),t2
	pxor t3,s1
	movd 8192(%esi,%edx,4),t3
	pand LOCAL(maskFF000000),t2
	pand LOCAL(maskFF000000),t3
	pxor t2,s1
	pxor t3,s2
')

define(`dblock',`
	sxrk

	movl `$'SYMNAME(_ad0),%esi

	dtfs(16)
	dsft(32)
	dtfs(48)
	dsft(64)
	dtfs(80)
	dsft(96)
	dtfs(112)
	dsft(128)
	dtfs(144)

	movl 256(%ebp),%eax
	cmp `$'10,%eax
	je $1

	dsft(160)
	dtfs(176)

	movl 256(%ebp),%eax
	cmp `$'12,%eax
	je $1

	dsft(192)
	dtfs(208)

	movl 256(%ebp),%eax

	.align 4
$1:
	sall `$'4,%eax
	addl %eax,%ebp

	dlr
')


C_FUNCTION_BEGIN(aesDecrypt)
	pushl %edi
	pushl %esi
	pushl %ebp
	pushl %ebx

	movl 20(%esp),%ebp
	movl 24(%esp),%edi
	movl 28(%esp),%esi

	dblock(LOCAL(10))

	movd s0, 0(%edi)
	movd s1, 4(%edi)
	movd s2, 8(%edi)
	movd s3,12(%edi)

	xorl %eax,%eax
	emms 

	popl %ebx
	popl %ebp
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(aesDecrypt)


C_FUNCTION_BEGIN(aesDecryptECB)
	pushl %edi
	pushl %esi
	pushl %ebp
	pushl %ebx

	movl 24(%esp),%edi
	movl 28(%esp),%esi

	.p2align 4,,15
LOCAL(aesDecryptECB_loop):
	movl 20(%esp),%ebp

	dblock(LOCAL(11))

	movd s0, 0(%edi)
	movd s1, 4(%edi)
	movd s2, 8(%edi)
	movd s3,12(%edi)

	addl `$'16,%esi
	addl `$'16,%edi
	decl 32(%esp)
	jnz LOCAL(aesDecryptECB_loop)

	xorl %eax,%eax
	emms 

	popl %ebx
	popl %ebp
	popl %esi
	popl %edi
	ret
C_FUNCTION_END(aesDecryptECB)

')
