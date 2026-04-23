# MyceliumOS v0.1.5-stable

> Um sistema operacional x86 de 32 bits minimalista focado em modularidade, aprendizado e a beleza do micélio digital.

```text
      .-'~~~-.           
    .'o  oOOOo.`         MyceliumOS
   :~~~-.oOo   o.  `     Kernel: Mycelium-x86
    `. \ ~-.  oOOo.      Arch: i386 (32-bit)
      `.; / ~.  OO:      Version: 0.1.5-stable
       .'  ;-- `.o.'
     ,'  ; ~~--'~
     ;  ;
 _\\;_\\//_________
```

## Funcionalidades Atuais (v0.1.5)

  Interpretador de Comandos (Shell): Suporte a comandos dinâmicos e expansão de variáveis com prefixo $.

  Gestão de Terminal: Scrolling automático por hardware/memória e controle de cursor.

  Teclado: Driver completo com suporte a SHIFT e caracteres especiais.

  Gráficos de Texto: Sistema de cores VGA e interface baseada em caracteres.

  Timer & Som: Controle de PIT (Programmable Interval Timer) e beep via PC Speaker.

## Comandos Disponíveis

  ajuda: Lista todos os comandos integrados.

  echo: Imprime texto ou variáveis de sistema (ex: echo $ver).

  uptime: Mostra o tempo de atividade do sistema.

  fetch: Exibe informações do sistema com arte ASCII.

  color: Altera a cor do terminal.

  beep: Gera um som em uma frequência específica.

  reboot: Reinicia o sistema via Triple Fault.

## Como Executar

Para compilar e testar o MyceliumOS, você precisará de um ambiente Unix (Linux/WSL) com gcc (i386-elf), nasm e o emulador QEMU.

  Compilação:
  Bash

  # Use seu script de build ou Makefile
    nasm -f elf32 boot.s -o boot.o
    gcc -m32 -c kernel.c -o kernel.o -ffreestanding -O2 -Wall -Wextra
    ld -m elf_i386 -T linker.ld boot.o kernel.o -o mycelium.bin

  Execução no QEMU:
  Bash

    qemu-system-i386 -drive format=raw,file=mycelium-0.1.5-stable.img

Autor: [azzycmd]
Licença: MIT