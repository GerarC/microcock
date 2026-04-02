mkdir -p build/isodir
mkdir -p build/isodir/boot
mkdir -p build/isodir/boot/grub

cp build/cock.bin build/isodir/boot/cock.bin
cp build/userland/hello.elf build/isodir/boot/

cat > build/isodir/boot/grub/grub.cfg << EOF
menuentry "cock" {
	multiboot /boot/cock.bin
    module /boot/hello.elf
}
EOF
grub-mkrescue -o build/cock.iso build/isodir
