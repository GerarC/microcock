global flush_tlb
flush_tlb:
    mov eax, cr3
    mov cr3, eax
    ret


global invalpg
invlpg:
    mov eax, [esp + 4]
    invlpg [eax]
    ret
