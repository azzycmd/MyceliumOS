# MyceliumOS Amanita (v0.2.2)

```text
      .-'~~~-.           
    .'o  oOOOo.`         MyceliumOS
   :~~~-.oOo   o.  `     Kernel: Mycelium-x86
    `. \ ~-.  oOOo.      Versao: Amanita (v0.2.2)
      `.; / ~.  OO:      
       .'  ;-- `.o.'
     ,'  ; ~~--'~
     ;  ;
 _\\;_\\//_________
```

## Funcionalidades Novas (v0.2.2)

  - Muito mais facil de mexer no terminal agr

  - Adicionada navegação por palavras (Ctrl + Setas).

  - Sistema de deleção inteligente (Backspace detecta Tabs e Ctrl+Backspace apaga palavras).

  - Corrigido bug de renderização que "comia" o prompt do sistema.

## Comandos Disponíveis

  ajuda: Lista todos os comandos integrados.

  echo: Imprime texto ou variáveis de sistema.

  uptime: Mostra o tempo de atividade do sistema.

  fetch: Exibe informações do sistema com arte ASCII.

  color: Altera a cor do terminal.

  beep: Gera um som em uma frequência específica.

  reboot: Reinicia o sistema via Triple Fault (agora com tempo pra resetar).

  cpuinfo: Mostra fabricante e nome da CPU

  rtc: Mostra o tempo real

  set: Muda o valor de algumas variaveis

## Como Executar

  Execute o arquivo "Makefile". Para compilar, use "make". Para rodar, use "make run". Para limpar, "make clean"

Autor: [azzycmd]
Licença: MIT