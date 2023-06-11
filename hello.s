	.text
.global _start
_start:
	nop
	push x2
	mov sp, r2
	# }
	mov $0, x0
	mov x0, x0
	mov r2, sp
	pop r2
	ret
