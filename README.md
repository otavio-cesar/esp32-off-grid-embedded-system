# Monitor de corrente e controle de relé com ESP32

Este projeto utiliza um ESP32 para medir a corrente por meio de um sinal
senoidal condicionado, estimar a potência da carga e controlar um relé de
acordo com limites de proteção.

## Medição do sinal

O sinal analógico condicionado é lido pelo **GPIO 34** usando o ADC de 12 bits
do ESP32, com atenuação de 11 dB. A faixa esperada do sinal é de
aproximadamente 0 a 3 V.

O firmware coleta **400 amostras a 2 kHz**, com intervalo de 500 microssegundos
entre elas. Cada janela de medição dura aproximadamente 200 ms, o equivalente
a cerca de 12 ciclos de uma onda de 60 Hz.

Durante cada janela, o programa encontra a menor e a maior tensão medidas e
calcula a tensão pico a pico:

```text
Vpp = Vmax - Vmin
```

A tensão pico a pico é convertida em tensão de pico e depois em tensão RMS,
considerando uma onda senoidal:

```text
Vp   = Vpp / 2
Vrms = Vp / sqrt(2)
```

## Cálculo da corrente e da potência

A corrente é calculada pela equação de primeiro grau modelada no MATLAB:

```text
Corrente (A) = 20,453 * Vrms - 0,7362
```

O firmware usa `1,41421356` como aproximação de `sqrt(2)`. Resultados de
corrente menores que zero são ajustados para zero antes do cálculo da potência
e do controle do relé.

A potência é estimada considerando uma tensão residencial de **120 V**:

```text
Potência (W) = Corrente (A) * 120 V
```

## Controle do relé e proteção

O relé é controlado pelo **GPIO 26** e opera com lógica ativa em nível baixo.
O buzzer ativo está ligado ao **GPIO 27**.

Para evitar comutações causadas por oscilações próximas aos limites, o controle
utiliza histerese e temporização:

- potência entre **1200 W e 1450 W** durante 10 segundos: desliga o relé e
  aciona o buzzer;
- potência acima de **1450 W** durante 2 segundos: desliga o relé e aciona o
  buzzer;
- potência abaixo de **1000 W** durante 5 segundos: aciona o relé e desliga o
  buzzer;
- potência entre **1000 W e 1200 W**: mantém o estado atual;
- depois de desligar, mantém o relé desativado por pelo menos 30 segundos.

O intervalo mínimo de 30 segundos protege somente o religamento e nunca atrasa
um desligamento por sobrecarga.

Ao iniciar, o programa coloca o GPIO 26 em nível alto para manter o relé
desligado. O contator é normalmente fechado para a concessionária; portanto, a
carga permanece alimentada pela rede enquanto o ESP32 inicializa ou enquanto o
relé está desenergizado.

O acionamento do relé fica bloqueado durante os primeiros **60 segundos** após
a inicialização. Depois desse período, a potência ainda precisa permanecer
abaixo de 1000 W durante 5 segundos. Assim, o primeiro acionamento pode ocorrer
aproximadamente 65 segundos após a inicialização.

## Display

Um display OLED SSD1306 de **128 x 64 pixels**, no endereço I2C `0x3C`, mostra
as seguintes medições:

- em texto menor: `Vpp`, `Vrms`, `Vmin` e `Vmax`;
- em texto maior: corrente (`I`) e potência (`P`).

Cada janela de medição dura aproximadamente 200 ms. Depois de atualizar o
display e processar o controle, o programa aguarda 500 ms antes da próxima
janela.

## Ligações principais

| Componente | Pino do ESP32 | Função |
|---|---:|---|
| Sinal analógico condicionado | GPIO 34 | Entrada do ADC |
| Módulo relé | GPIO 26 | Saída digital ativa em nível baixo |
| Buzzer ativo | GPIO 27 | Alarme de desligamento por sobrecarga |
| OLED SSD1306 | Barramento I2C | Exibição das medições |

## Bibliotecas

- Adafruit GFX Library
- Adafruit SSD1306
- Wire

## Compilação

O projeto é compatível com a placa genérica `esp32:esp32:esp32` do pacote ESP32
para Arduino.
