# =============================================================
# Makefile - MyceliumOS (Limine + Multiboot2 + UEFI/BIOS)
#
# Mudanças em relação ao original:
#   - Estrutura de pastas src/asm/ para .asm e src/ para .c
#   - Regra `iso` gera imagem hibrida com Limine
#   - Regra `run` usa OVMF (UEFI firmware) corretamente
#   - Removida a regra `image` (MBR) — não faz sentido no Caminho A
#   - Adicionada regra `run-legacy` para testar sem UEFI
# =============================================================

CC      = gcc
AS      = nasm
LD      = ld

# Flags de compilação (32-bit freestanding)
CFLAGS  = -m32 -nostdlib -fno-builtin -fno-stack-protector \
          -ffreestanding -O2 -fno-strict-aliasing -Wall -Wextra \
          -I src -I include

ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld

# --- Estrutura de pastas ---
SRCDIR  = src
OBJDIR  = obj
BINDIR  = bin

# --- Fontes ---
AS_SOURCES = $(wildcard $(SRCDIR)/asm/*.asm)
C_SOURCES  = $(wildcard $(SRCDIR)/*.c)
HEADERS    = $(wildcard include/*.h)

# --- Objetos ---
AS_OBJECTS = $(AS_SOURCES:$(SRCDIR)/asm/%.asm=$(OBJDIR)/asm/%.o)
C_OBJECTS  = $(C_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
OBJ        = $(AS_OBJECTS) $(C_OBJECTS)

KERNEL  = $(BINDIR)/mycelium.bin
ISO     = mycelium.iso
LIMINE_DEFAULT_ENTRY ?= 1

QEMU_USB_KBD = -device qemu-xhci,id=xhci,p2=4,p3=4 \
               -device usb-kbd,id=usbkbd,bus=xhci.0

# =============================================================
# Regras principais
# =============================================================

all: setup $(KERNEL)

setup:
	@mkdir -p $(OBJDIR)/asm $(BINDIR)

# Compilação C — recompila qualquer .c se qualquer .h mudar
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS) | setup
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Compilação Assembly
$(OBJDIR)/asm/%.o: $(SRCDIR)/asm/%.asm | setup
	@echo " [AS] $<"
	@$(AS) $(ASFLAGS) $< -o $@

# Linkagem
$(KERNEL): $(OBJ)
	@echo " [LD] $@"
	@$(LD) $(LDFLAGS) -o $@ $^

# =============================================================
# ISO com Limine para UEFI/BIOS hibrido
# =============================================================
iso: $(KERNEL)
	@mkdir -p isodir/boot isodir/EFI/BOOT
	@cp $(KERNEL) isodir/boot/mycelium.bin
	@sed 's/^default_entry:.*/default_entry: $(LIMINE_DEFAULT_ENTRY)/' limine.conf > isodir/limine.conf
	@cp .limeline/limelinebg.jpg isodir/boot/limelinebg.jpg
	@cp /usr/share/limine/limine-bios.sys isodir/boot/limine-bios.sys
	@cp /usr/share/limine/limine-bios-cd.bin isodir/boot/limine-bios-cd.bin
	@cp /usr/share/limine/limine-uefi-cd.bin isodir/boot/limine-uefi-cd.bin
	@cp /usr/share/limine/BOOTX64.EFI isodir/EFI/BOOT/BOOTX64.EFI
	@cp /usr/share/limine/BOOTIA32.EFI isodir/EFI/BOOT/BOOTIA32.EFI
	@echo "Criando ISO com Limine..."
	xorriso -as mkisofs -R -r -J \
		-b boot/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 \
		--efi-boot boot/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		isodir -o $(ISO)
	limine bios-install $(ISO)
	@echo "ISO criada: $(ISO)"

# =============================================================
# Rodar no QEMU com UEFI (OVMF)
# Requer: ovmf_code.fd na pasta raiz do projeto
# Para obter: sudo cp /usr/share/OVMF/OVMF_CODE.fd ./ovmf_code.fd
# =============================================================
run: 
	$(MAKE) clean
	$(MAKE) LIMINE_DEFAULT_ENTRY=2 iso
	@if [ ! -f ovmf_code.fd ]; then \
		echo ""; \
		echo "ERRO: ovmf_code.fd nao encontrado!"; \
		echo "Execute: sudo cp /usr/share/OVMF/OVMF_CODE.fd ./ovmf_code.fd"; \
		echo "Ou: sudo cp /usr/share/edk2/x64/OVMF_CODE.fd ./ovmf_code.fd"; \
		exit 1; \
	fi
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=./ovmf_code.fd \
		-cdrom $(ISO) \
		-boot d \
		-display gtk \
		-machine pcspk-audiodev=audio0 \
		-audiodev driver=pa,id=audio0 \
		-m 256M \
		-vga std

run-usb:
	$(MAKE) clean
	$(MAKE) LIMINE_DEFAULT_ENTRY=4 iso
	@if [ ! -f ovmf_code.fd ]; then \
		echo ""; \
		echo "ERRO: ovmf_code.fd nao encontrado!"; \
		echo "Execute: sudo cp /usr/share/OVMF/OVMF_CODE.fd ./ovmf_code.fd"; \
		echo "Ou: sudo cp /usr/share/edk2/x64/OVMF_CODE.fd ./ovmf_code.fd"; \
		exit 1; \
	fi
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=./ovmf_code.fd \
		-cdrom $(ISO) \
		-boot d \
		-display gtk \
		-machine pcspk-audiodev=audio0 \
		-audiodev driver=pa,id=audio0 \
		-m 256M \
		$(QEMU_USB_KBD) \
		-vga std

# Roda sem UEFI (legacy BIOS) com VGA text mode correto
run-legacy: iso
	qemu-system-i386 \
		-cdrom $(ISO) \
		-boot d \
		-display gtk,full-screen=on,zoom-to-fit=on \
		-machine pcspk-audiodev=audio0 \
		-audiodev driver=pa,id=audio0 \
		-m 256M \
		$(QEMU_USB_KBD) \
		-vga std

strict: 
	$(MAKE) clean
	$(MAKE) LIMINE_DEFAULT_ENTRY=2 iso
	@if [ ! -f ovmf_code.fd ]; then \
		echo ""; \
		echo "ERRO: ovmf_code.fd nao encontrado!"; \
		echo "Execute: sudo cp /usr/share/OVMF/OVMF_CODE.fd ./ovmf_code.fd"; \
		echo "Ou: sudo cp /usr/share/edk2/x64/OVMF_CODE.fd ./ovmf_code.fd"; \
		exit 1; \
	fi
	qemu-system-x86_64 \
    -machine pcspk-audiodev=audio0 \
    -drive if=pflash,format=raw,readonly=on,file=./ovmf_code.fd \
    -cdrom $(ISO) \
    -boot d \
    -m 256M \
    -vga std \
    -display gtk \
    -audiodev driver=pa,id=audio0 \
    $(QEMU_USB_KBD)

# =============================================================
# Utilitários
# =============================================================
clean:
	@echo "Limpando..."
	@rm -rf $(OBJDIR) $(BINDIR) isodir $(ISO)

debug:
	@echo "AS_SOURCES: $(AS_SOURCES)"
	@echo "C_SOURCES:  $(C_SOURCES)"
	@echo "OBJ:        $(OBJ)"

.PHONY: all setup clean run run-usb run-legacy strict iso debug
