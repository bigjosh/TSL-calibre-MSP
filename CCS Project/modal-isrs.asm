

	.global asmfunc
	.global gvar
asmfunc:
	MOV &gvar,R11
	ADD R11,R12
	RET
