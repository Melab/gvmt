	.text
    
.globl gvmt_setjump
	.type	gvmt_setjump, @function
gvmt_setjump:
    movl 0(%esp), %eax  #EIP
    mov  %eax, 0(%ecx)
    mov  %ebp, 4(%ecx)   # Save EBP, EBX, EDI, ESI, and ESP
    mov  %ebx, 8(%ecx)
    mov  %edi, 12(%ecx)
    mov  %esi, 16(%ecx)
    mov  %esp, 20(%ecx)
    mov  $0, %eax
    ret
.size	gvmt_setjump, .-gvmt_setjump        
  
.globl gvmt_longjump
	.type	gvmt_longjump, @function
gvmt_longjump:
    mov  4(%ecx), %ebp  # Restore EBP, EBX, EDI, ESI, and ESP
    mov  8(%ecx), %ebx
    mov  12(%ecx), %edi
    mov  16(%ecx), %esi
    mov  20(%ecx), %esp
    mov  0(%ecx), %eax  #EIP
    movl %eax, 0(%esp)  
    mov  %edx, %eax
    ret
.size	gvmt_longjump, .-gvmt_longjump   

.globl gvmt_swap_context
    .type	gvmt_swap_context, @function
gvmt_swap_context:
    mov  0(%esp), %edx  #Current EIP
    mov  0(%ecx), %eax
    mov  %edx, 0(%ecx)
    mov  %eax, %edx   # Swapped EIP
    mov  4(%ecx), %eax
    mov  %ebp, 4(%ecx)
    mov  %eax, %ebp
    mov  8(%ecx), %eax
    mov  %ebx, 8(%ecx)
    mov  %eax, %ebx
    mov  12(%ecx), %eax
    mov  %edi, 12(%ecx)
    mov  %eax, %edi
    mov  16(%ecx), %eax
    mov  %esi, 16(%ecx)
    mov  %eax, %esi
    mov  20(%ecx), %eax
    mov  %esp, 20(%ecx)
    mov  %eax, %esp
    movl %edx, 0(%esp)
    ret
.size	gvmt_swap_context, .-gvmt_swap_context   


.globl TEST_OVERFLOW
    .type	TEST_OVERFLOW, @function
TEST_OVERFLOW:
    jo .OVER
    movl	$0, %eax
    ret 
.OVER:
    movl	$1, %eax
    ret
    .size	TEST_OVERFLOW, .-TEST_OVERFLOW

.global gs_register  
    .type	gs_register, @function
gs_register:
	movl %gs:0, %eax
    ret

.global x86_tls_load
x86_tls_load:
	movl	%ecx, %eax
    movl	%gs:(%eax), %eax
	ret

.global x86_tls_store
x86_tls_store:
	movl	%edx, %eax
    movl	%ecx, %gs:(%eax)
	ret

