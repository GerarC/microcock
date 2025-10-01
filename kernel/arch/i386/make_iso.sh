mkdir -p build/isodir
mkdir -p build/isodir/boot
mkdir -p build/isodir/boot/grub

cp build/cock.bin build/isodir/boot/cock.bin
cat > build/isodir/boot/grub/grub.cfg << EOF
menuentry "cock" {
	multiboot /boot/cock.bin
}
EOF
grub-mkrescue -o build/cock.iso build/isodir
