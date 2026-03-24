# === GDB init for Microcock kernel debugging ===

target remote localhost:1234

symbol-file build/cock.elf

layout split
layout src
layout regs

break cock_main

define vga_line
    echo VGA first line (hex 16-bit words):\n
    x/80hx 0xb8000
end

define vga_text
    echo VGA first line (ASCII chars):\n
    set $i = 0
    while ($i < 80)
        set $ch = *(char*)(0xb8000 + $i*2)
        if ($ch >= 32 && $ch <= 126)
            printf "%c", $ch
        else
            printf "."
        end
        set $i = $i + 1
    end
    printf "\n"
end

