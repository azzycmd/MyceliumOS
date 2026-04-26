# MyceliumOS Amanita (v0.2.1)

```text
      .-'~~~-.           
    .'o  oOOOo.`         MyceliumOS
   :~~~-.oOo   o.  `     Kernel: Mycelium-x86
    `. \ ~-.  oOOo.      Versao: Amanita (v0.2.1)
      `.; / ~.  OO:      
       .'  ;-- `.o.'
     ,'  ; ~~--'~
     ;  ;
 _\\;_\\//_________
```

## Funcionalidades Novas (v0.2.1)

  - Set - Comando que muda algumas variaveis

  - Separacao do som, time (idt) e main - O som foi pro som.c e som.h, o time foi pro lib.c e lib.h e o main ficou la.

  - Smod - Preparacao do terreno pra smod (super mode). Ativado com "set smod 1".

  - Correcao do sleep - Mudar o idt de lugar + umas correcoes com o sti consequentemente deixou o sleep podendo ser ativado em comandos, ent correcao no reboot

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