# Monitor de corrente e controle de relé com ESP32

Este projeto utiliza um ESP32 para medir a corrente por meio de um sinal
condicionado, estimar a potência da carga e controlar um relé com base em
limites de proteção.

## Medição do sinal

O sinal analógico condicionado é lido pelo GPIO 34 usando o ADC de 12 bits do
ESP32, com atenuação de 11 dB. O firmware coleta 400 amostras a 2 kHz para
calcular o valor RMS da tensão alternada após a remoção do bias DC.

A corrente é estimada a partir do valor de RMS pela equação:

```text
Corrente (A) = 9,295629481419246 * Vrms - 0,178376044032725
```

Valores negativos de corrente são corrigidos para zero antes de calcular a
potência estimada.

A potência é calculada considerando uma tensão residencial de 120 V:

```text
Potência (W) = Corrente (A) * 120 V
```

## Controle do relé e proteção

O relé é controlado pelo GPIO 26 e opera com lógica ativa em nível baixo. O
buzzer ativo está ligado ao GPIO 27.

A proteção funciona da seguinte forma:

- se a potência permanecer a partir de 1000 W por 2 segundos, o relé é desligado;
- se a potência permanecer acima de 1200 W por 1 segundo, o relé é desligado;
- se a potência permanecer abaixo de 1000 W por 5 segundos, o relé é ligado;
- após um desligamento, o relé permanece desativado por pelo menos 30 segundos;
- o buzzer é acionado quando a condição de desligamento é confirmada.

Ao inicializar, o programa mantém o relé desligado durante os primeiros 60
segundos. Depois desse período, o primeiro religamento só ocorre se a potência
permanecer abaixo de 1000 W durante 5 segundos.

## Display

O display OLED SSD1306 de 128 x 64 pixels mostra apenas:

- tensão RMS;
- corrente estimada;
- potência estimada.

## Ligações principais

| Componente | Pino do ESP32 | Função |
|---|---:|---|
| Sinal analógico condicionado | GPIO 34 | Entrada do ADC |
| Módulo relé | GPIO 26 | Saída digital ativa em nível baixo |
| Buzzer ativo | GPIO 27 | Alarme de desligamento |
| OLED SSD1306 | Barramento I2C | Exibição das medições |

## Bibliotecas

- Adafruit GFX Library
- Adafruit SSD1306
- Wire

## Compilação

O projeto é compatível com a placa genérica `esp32:esp32:esp32` do pacote ESP32
para Arduino.
