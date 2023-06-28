// The first function to be called after system start
.int a 90
_start:
	nop
	push x2
	mov sp, x2
	mov $0, x0
	mov x0, x0
	mov x2, sp
	pop x2
	ret
