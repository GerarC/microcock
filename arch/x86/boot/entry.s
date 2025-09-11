org 0x7C00 ; this is the sector of the memory that says that we are in bios mode
bits 16; says how many bits we are working with, to emit them


main:
    hlt

.halt: ; in case the processor restarts, it gets stuck in a infinite loop becuase is not good Idea to let the processor run after the code
    jmp .halt


times 510-($-$$) db 0 ; ($-$$) says the complete size of the section of the program
dw 0AA55H ; Write in the first position of the memory  
