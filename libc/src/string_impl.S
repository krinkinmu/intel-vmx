	.text
	.global memcpy
memcpy:
	movq	%rdi, %rax
	test	%rdx, %rdx
	jz	out1
mv8:	cmpq	$8, %rdx
	jb	mv1
	movsq
	subq	$8, %rdx
	jz	out1
	jmp	mv8
mv1:	movsb
	dec	%rdx
	jnz	mv1
out1:	ret

	.global memset
memset:
	movq	$8, %rcx
fill:	movb	%sil, %al
	salq	$8, %rax
	loop	fill
	movq	%rdi, %rcx
	test	%rdx, %rdx
	jz	out2
st8:	cmpq	$8, %rdx
	jb	st1
	stosq
	subq	$8, %rdx
	jz	out2
	jmp	st8
st1:	stosb
	dec	%rdx
	jnz	st1
out2:	movq	%rcx, %rax
	ret

	.global strlen
strlen:
	movq	%rdi, %rsi
	xorq	%rax, %rax
	repne
	scasb
	subq	%rsi, %rdi
	movq	%rdi, %rax
	dec	%rax
	ret
