# estacao-monitoramento-remoto-iot

# Estação de Monitoramento de Luminosidade com Baixo Consumo de Energia

Este projeto consiste no desenvolvimento do firmware para uma estação de monitoramento de luminosidade baseada no microcontrolador ATmega328P. O objetivo principal do sistema é aplicar técnicas avançadas de gerenciamento de energia (como *duty cycling*, desativação de periféricos via registrador PRR, desligamento do detector de *Brown-Out* e modos de *sleep*) para reduzir drasticamente o consumo elétrico do dispositivo durante a operação.

Para fins de avaliação e testes sem a necessidade de sensores físicos, o cenário é simulado no software Proteus utilizando dois microcontroladores ATmega328P interconectados via comunicação serial UART.

---

## Estrutura da Solução

O sistema é dividido em duas unidades funcionais:

1. **Estação Principal (Monitor):** Recebe os dados brutos de luminosidade, processa as informações, gerencia as transições entre os modos de operação (Idle e Power-down) e controla os componentes de sinalização (LEDs).
2. **Sensor Auxiliar (Simulador):** Microcontrolador secundário configurado para transmitir valores estruturados de luminosidade a partir de uma curva pré-definida na memória, atuando como o sensor do sistema.

---

## Estratégia de Gerenciamento de Energia

### 1. Modos de Sleep e Duty Cycling

- **Modo Idle:** É o estado padrão de espera da estação. A CPU permanece desligada para economizar energia, mas o módulo UART continua ativo. O microcontrolador é acordado instantaneamente por meio da interrupção de recepção da UART (`USART_RX_vect`) assim que um novo byte é recebido.  
- **Modo Power-down:** Ativado quando a estação detecta condições de baixa luminosidade prolongada. Neste estado, o consumo é reduzido ao mínimo absoluto. O microcontrolador desliga os osciladores internos e passa a ser acordado periodicamente pelas interrupções do Watchdog Timer (WDT) configurado para janelas de aproximadamente 4 segundos.

---

### 2. Otimização de Periféricos (PRR)

O firmware manipula diretamente os registradores de hardware em C puro. Através do registrador `PRR` (Power Reduction Register), o clock de todos os periféricos internos não utilizados é completamente cortado. Os seguintes módulos são desativados:

- Conversor Analógico-Digital (ADC)  
- Timer 0, Timer 1 e Timer 2  
- Interface SPI  
- Interface TWI (I2C)  

Apenas o módulo UART0 permanece energizado.

---

### 3. Desativação do Brown-Out Detector (BOD)

Imediatamente antes de entrar no modo Power-down, o firmware executa uma sequência temporizada de escrita no registrador `MCUCR` para desativar o circuito do Brown-Out Detector por software. Isso elimina o consumo estático do circuito de proteção durante o período de repouso profundo, sendo reativado automaticamente pelo hardware assim que o chip acorda.

---

### 4. Tratamento de Pinos Flutuantes

Todos os pinos não utilizados dos PORTs B, C e D são explicitamente configurados como entradas digitais com os resistores de *pull-up* internos ativados. Isso evita que as portas fiquem flutuando eletricamente, o que geraria correntes parasitas indesejadas e consumo desnecessário.

---

## Lógica de Transição de Estados

A mudança entre os estados de consumo é governada pelo valor do byte de luminosidade recebido, operando sob uma faixa de limiar com histerese para evitar oscilações rápidas:

- **Transição para Power-down:** Se a estação estiver em modo IDLE e receber **3 valores consecutivos abaixo de 70**, ela transmite o caractere `'S'` via UART para notificar o sensor, desliga o LED indicativo de IDLE, ativa o Watchdog Timer e entra em repouso profundo.  
- **Transição para Idle:** Quando em modo Power-down, a estação acorda a cada 4 segundos, pisca o LED REQUEST e transmite o caractere `'R'` solicitando uma leitura. Se ela receber **3 valores consecutivos maiores ou iguais a 70**, desativa o Watchdog Timer, transmite o caractere `'I'` para reativar o fluxo contínuo do sensor e retorna em definitivo ao modo IDLE.

---

## Estratégia de Comunicação

A comunicação entre os microcontroladores é realizada via UART nativa (Hardware) operando a 9600 bps, sem paridade, com 8 bits de dados e 1 stop bit (8N1). O fluxo é controlado por comandos em caracteres ASCII puros enviados pela estação principal:

- `'I'`: Solicita ao sensor o envio periódico automático de leituras (1 amostra por segundo).  
- `'S'`: Sinaliza ao sensor que a estação entrará em repouso profundo, instruindo-o a pausar os envios automáticos.  
- `'R'`: Solicita uma leitura única e imediata sob demanda.  

O sensor responde a estas instruções transmitindo o dado de luminosidade correspondente de forma binária pura (1 byte).

---

## Validação e Simulação no Proteus

A avaliação de funcionamento do firmware é conduzida dentro do ambiente do software Proteus.


https://github.com/user-attachments/assets/340679d7-3955-477a-8a0b-9c825a58dbd2



### Conexões do Esquemático Elétrico

- **Linhas de Comunicação:**  
  - PD1 (TX) da Estação → PD0 (RX) do Sensor   
  - PD0 (RX) da Estação → PD1 (TX) do Sensor  

- **Sinalização Visual:**  
  - LED no pino **PB0** (Modo IDLE)  
  - LED no pino **PB1** (REQUEST)  

- **Clock de Simulação:**  
  Ambos os ATmega328P devem estar configurados para **16 MHz** no Proteus.

---

## Requisitos de Compilação

O firmware foi desenvolvido em C puro para arquitetura AVR, manipulando diretamente registradores de clock, energia e periféricos sem uso de HAL ou bibliotecas Arduino.
