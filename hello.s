_start:
	nop
	push x2
	mov sp, x2
	push x0
	.string L0 "oh no!"
	lea L0(x31), x0
	push x0
	pop x0
	call panic
	pop x0
	mov $0, x0
	mov x0, x0
	mov x2, sp
	pop x2
	ret
