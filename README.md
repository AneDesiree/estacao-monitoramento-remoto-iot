# estacao-monitoramento-remoto-iot

# Estacao de Monitoramento de Luminosidade com Baixo Consumo de Energia

Este projeto consiste no desenvolvimento do firmware para uma estacao de monitoramento de luminosidade baseada no microcontrolador ATmega328P. O objetivo principal do sistema e aplicar tecnicas avancadas de gerenciamento de energia (como *duty cycling*, desativacao de perifericos via registrador PRR, desligamento do detector de *Brown-Out* e modos de *sleep*) para reduzir drasticamente o consumo eletrico do dispositivo durante a operacao.

Para fins de avaliacao e testes sem a necessidade de sensores fisicos, o cenario e simulado no software Proteus utilizando dois microcontroladores ATmega328P interconectados via comunicacao serial UART.

## Estrutura da Solucao

O sistema e dividido em duas unidades funcionais:

1. **Estacao Principal (Monitor):** Recebe os dados brutos de luminosidade, processa as informacoes, gerencia as transicoes entre os modos de operacao (Idle e Power-down) e controla os componentes de sinalizacao (LEDs).
2. **Sensor Auxiliar (Simulador):** Microcontrolador secundario configurado para transmitir valores estruturados de luminosidade a partir de uma curva pre-definida na memoria, atuando como o sensor do sistema.

---

## Estrategia de Gerenciamento de Energia

### 1. Modos de Sleep e Duty Cycling
* **Modo Idle:** E o estado padrao de espera da estacao. A CPU permanece desligada para economizar energia, mas o modulo UART continua ativo. O microcontrolador e acordado instantaneamente por meio da interrupcao de recepcao da UART (`USART_RX_vect`) assim que um novo byte e recebido.
* **Modo Power-down:** Ativado quando a estacao detecta condicoes de baixa luminosidade prolongada. Neste estado, o consumo e reduzido ao minimo absoluto. O microcontrolador desliga os osciladores internos e passa a ser acordado periodicamente pelas interrupcoes do Watchdog Timer (WDT) configurado para janelas de aproximadamente 4 segundos.

### 2. Otimizacao de Perifericos (PRR)
O firmware manipula diretamente os registradores de hardware em C puro. Atraves do registrador `PRR` (Power Reduction Register), o clock de todos os perifericos internos nao utilizados e completamente cortado. Os seguintes modulos sao desativados:
* Conversor Analogico-Digital (ADC)
* Timer 0, Timer 1 e Timer 2
* Interface SPI
* Interface TWI (I2C)
Apenas o modulo UART0 permanece energizado.

### 3. Desativacao do Brown-Out Detector (BOD)
Imediatamente antes de entrar no modo Power-down, o firmware executa uma sequencia temporizada de escrita no registrador `MCUCR` para desativar o circuito do Brown-Out Detector por software. Isso elimina o consumo estatico do circuito de protecao durante o periodo de repouso profundo, sendo reativado automaticamente pelo hardware assim que o chip acorda.

### 4. Tratamento de Pinos Flutuantes
Todos os pinos nao utilizados dos PORTs B, C e D sao explicitamente configurados como entradas digitais com os resistores de *pull-up* internos ativados. Isso evita que as portas fiquem flutuando eletricamente, o que geraria correntes parasitas indesejadas e consumo desnecessario.

---

## Logica de Transicao de Estados

A mudanca entre os estados de consumo e governada pelo valor do byte de luminosidade recebido, operando sob uma faixa de limiar com histerese para evitar oscilacoes rapidas:

* **Transicao para Power-down:** Se a estacao estiver em modo IDLE e receber **3 valores consecutivos abaixo de 70**, ela transmite o caractere `'S'` via UART para notificar o sensor, desliga o LED indicativo de IDLE, ativa o Watchdog Timer e entra em repouso profundo.
* **Transicao para Idle:** Quando em modo Power-down, a estacao acorda a cada 4 segundos, pisca o LED REQUEST e transmite o caractere `'R'` solicitando uma leitura. Se ela receber **3 valores consecutivos maiores ou iguais a 70**, desativa o Watchdog Timer, transmite o caractere `'I'` para reativar o fluxo continuo do sensor e retorna em definitivo ao modo IDLE.

---

## Estrategia de Comunicacao

A comunicacao entre os microcontroladores e realizada via UART nativa (Hardware) operando a 9600 bps, sem paridade, com 8 bits de dados e 1 stop bit (8N1). O fluxo e controlado por comandos em caracteres ASCII puros enviados pela estacao principal:

* `'I'`: Solicita ao sensor o envio periodico automatico de leituras (1 amostra por segundo).
* `'S'`: Sinaliza ao sensor que a estacao entrara em repouso profundo, instruindo-o a pausar os envios automaticos.
* `'R'`: Solicita uma leitura unica e imediata sob demanda.

O sensor responde a estas instrucoes transmitindo o dado de luminosidade correspondente de forma binaria pura (1 byte).

---

## Validacao e Simulacao no Proteus

A avaliacao de funcionamento do firmware e conduzida dentro do ambiente do software Proteus. 

### Conexoes do Esquemata Eletrico
* **Linhas de Comunicacao:** O pino PD1 (TX) da Estacao deve ser conectado ao pino PD0 (RX) do Sensor. O pino PD0 (RX) da Estacao deve ser conectado ao pino PD1 (TX) do Sensor.
* **Sinalizacao Visual:**
  * Um LED conectado ao pino **PB0** da Estacao Principal (Sinalizacao de Modo IDLE).
  * Um LED conectado ao pino **PB1** da Estacao Principal (Sinalizacao de Chamada/REQUEST).
* **Clock de Simulacao:** Ambos os componentes ATmega328P dentro do Proteus devem ter suas propriedades de clock editadas manualmente para rodar a **16MHz**.



https://github.com/user-attachments/assets/64b37e33-a56f-4178-a62f-48082179c627



---

## Requisitos de Compilacao

O firmware foi desenvolvido utilizando linguagem C pura voltada para arquitetura AVR, manipulando diretamente os registradores de clock, energia e perifericos sem o uso de HAL ou bibliotecas Arduino.



# Conversao do arquivo .elf para binario hexadecimal inteligivel pelo Proteus
avr-objcopy -O ihex estacao.elf estacao.hex
