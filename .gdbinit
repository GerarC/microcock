# === GDB init for Microcock kernel debugging ===

# Connect to QEMU (run QEMU with: qemu-system-x86_64 -cdrom build/cock.iso -serial stdio -s -S)
target remote localhost:1234

# Load debug symbols
symbol-file build/cock.elf

# Enable TUI (text user interface) with code + regs
layout split
layout src
layout regs

# Break at kernel entry
break main_cock

# Convenience: show first VGA line (hex 16-bit words)
define vga_line
    echo VGA first line (hex 16-bit words):\n
    x/80hx 0xb8000
end

# Convenience: show first VGA line as chars
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

