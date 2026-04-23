#!/bin/bash

# Compilar tudo primeiro
nasm -f bin boot.asm -o bootloader.bin
nasm -f elf32 kernel_entry.asm -o kernel_entry.o
nasm -f elf32 interrupts.asm -o interrupts.o
gcc -m32 -ffreestanding -fno-stack-protector -fno-pic -c kernel.c -o kernel.o
ld -m elf_i386 -T linker.ld -o kernel.elf kernel_entry.o interrupts.o kernel.o
objcopy -O binary kernel.elf kernel.bin

# --- VERIFICAÇÃO CRÍTICA ---
if [ ! -f bootloader.bin ]; then
    echo "ERRO: bootloader.bin não foi gerado!"
    exit 1
fi

if [ ! -f kernel.bin ]; then
    echo "ERRO: kernel.bin não foi gerado!"
    exit 1
fi

# Criar a imagem apenas se os ficheiros existirem
echo "Criando imagem mycelium-0.1.6.img..."
dd if=/dev/zero of=mycelium-0.1.6.img bs=512 count=2880
dd if=bootloader.bin of=mycelium-0.1.6.img conv=notrunc
dd if=kernel.bin of=mycelium-0.1.6.img seek=1 conv=notrunc

echo "Pronto! Agora tenta rodar o QEMU."

qemu-system-i386 -hda mycelium-0.1.6.img -audiodev driver=pa,id=pa0 -machine pcspk-audiodev=pa0 