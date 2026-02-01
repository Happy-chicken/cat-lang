global _start
    _start:
        mov eax, 60 ; system call exit
        xor edi, edi ; return val
        syscall