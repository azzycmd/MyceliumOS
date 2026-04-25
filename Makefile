# --- Configurações de Compilação ---
CC = gcc
AS = nasm
LD = ld

# Flags de compilação para Kernel x86 (32-bit)
# -m32: Compila para 32 bits
# -ffreestanding: Indica que não temos a biblioteca padrão do C (libc)
# -fno-stack-protector: Desativa proteção de pilha que exige suporte do SO
CFLAGS = -m32 -nostdlib -fno-builtin -fno-stack-protector \
         -ffreestanding -O0 -Wall -Wextra -Iinclude

ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld

# --- Estrutura de Pastas ---
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# --- Descoberta Automática de Arquivos ---
# Procura todos os .c e .asm dentro de src e subpastas
C_SOURCES = $(shell find $(SRCDIR) -name '*.c')
ASM_SOURCES = $(shell find $(SRCDIR) -name '*.asm')

# Define os arquivos .o correspondentes dentro da pasta obj/
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(C_SOURCES)) \
          $(patsubst $(SRCDIR)/%.asm, $(OBJDIR)/%.o, $(ASM_SOURCES))

# Nome do binário final
KERNEL = $(BINDIR)/mycelium.bin

# --- Regras Principais ---

all: setup $(KERNEL)

# Cria as pastas necessárias se não existirem
setup:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)
	@mkdir -p $(dir $(OBJECTS))

# Linkagem final
$(KERNEL): $(OBJECTS)
	@echo " [LD] Linkando $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# Compilação de arquivos C
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo " [CC] Compilando $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Compilação de arquivos Assembly
$(OBJDIR)/%.o: $(SRCDIR)/%.asm
	@echo " [AS] Compilando $<"
	@$(AS) $(ASFLAGS) $< -o $@

# Limpeza
clean:
	@echo " Limpando arquivos antigos..."
	@rm -rf $(OBJDIR) $(BINDIR)

# Atalho para rodar no QEMU (se você tiver instalado)
run: all
	qemu-system-i386 -kernel $(KERNEL) -audiodev pa,id=pa0 -machine pcspk-audiodev=pa0
.PHONY: all setup clean run