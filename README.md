# MyceliumOS Amanita (v0.2.5)

```text
      .-'~~~-.           
    .'o  oOOOo.`         MyceliumOS
   :~~~-.oOo   o.  `     Kernel: Mycelium-x86
    `. \ ~-.  oOOo.      Versao: Amanita (v0.2.5)
      `.; / ~.  OO:      
       .'  ;-- `.o.'
     ,'  ; ~~--'~
     ;  ;
 _\\;_\\//_________
```

## Destaques da v0.2.5

- Shell com parser de aspas, escapes e limite claro de 10 argumentos.
- Codigos de erro por comando, com ultimo status em `$status`.
- `ajuda <comando>` com uso detalhado.
- Aliases: `help`, `clear`, `shutdown`.
- Historico navegavel em RAM e autocomplete parcial com TAB.
- Separacao logica entre stdout/stderr.
- Kernel panic para erros comuns e `Code` na tela de panic.
- Comandos de diagnostico: `cpuregs`, `idt`, `irq`, `memmap`, `pci`, `kbdinfo`, `ticks`, `bench`.
- `hex <endereco> [bytes]` com dump paginado.
- `panic --simulate irqstorm` para teste controlado de panic.

## Comandos

Principais comandos de shell:

- `ajuda`, `echo`, `uptime`, `fetch`, `color`, `beep`
- `reboot`, `desligar`, `cpuinfo`, `rtc`, `set`, `cls`
- `hex`, `panic`, `bootinfo`
- `cpuregs`, `idt`, `irq`, `memmap`, `pci`, `kbdinfo`, `ticks`, `bench`

## Autor e Licenca

Autor: azzycmd

Licenca: MIT
