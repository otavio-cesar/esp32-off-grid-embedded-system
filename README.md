# Leitura de onda senoidal e corrente com ESP32

Este projeto utiliza um ESP32 para medir uma onda senoidal condicionada na
faixa aproximada de 0 a 3 V. A partir dos extremos da onda, o firmware calcula
a tensão pico a pico, a tensão RMS e a corrente estimada.

## Funcionamento

O sinal analógico é lido pelo **GPIO 34** usando o ADC de 12 bits do ESP32, com
atenuação de 11 dB.

O firmware coleta **400 amostras a 2 kHz**, com intervalo de 500 microssegundos
entre as amostras. Cada janela de medição dura aproximadamente 200 ms, o que
corresponde a cerca de 12 ciclos completos de uma onda de 60 Hz.

Durante cada janela, o programa identifica:

- `Vmin`: menor tensão medida;
- `Vmax`: maior tensão medida;
- `Vpp`: tensão pico a pico.

A tensão pico a pico é calculada por:

```text
Vpp = Vmax - Vmin
```

## Cálculo da corrente

Primeiro, a tensão pico a pico é convertida em tensão de pico:

```text
Vp = Vpp / 2
```

Em seguida, a tensão de pico é convertida em tensão RMS, considerando uma onda
senoidal:

```text
Vrms = Vp / sqrt(2)
```

Por fim, a corrente é calculada pela equação de primeiro grau modelada no
MATLAB:

```text
Corrente (A) = 20,453 * Vrms - 0,7362
```

O firmware usa `1,41421356` como aproximação de `sqrt(2)`.

A equação é aplicada diretamente, sem limitar o resultado mínimo. Por isso,
valores muito baixos de `Vpp` podem resultar em uma corrente calculada
negativa devido ao coeficiente linear `-0,7362`.

## Display

Um display OLED SSD1306 de **128 x 64 pixels**, no endereço I2C `0x3C`, mostra:

1. tensão pico a pico (`Vpp`), em volts;
2. tensão mínima (`Vmin`), em volts;
3. tensão máxima (`Vmax`), em volts;
4. corrente calculada (`I`), em amperes.

As medições são atualizadas continuamente após cada janela de amostragem.

## Ligações principais

| Componente | Pino do ESP32 | Função |
|---|---:|---|
| Sinal analógico condicionado | GPIO 34 | Entrada do ADC |
| OLED SSD1306 | Barramento I2C | Exibição das medições |

## Bibliotecas

- Adafruit GFX Library
- Adafruit SSD1306
- Wire

## Compilação

O código foi validado com o Arduino CLI usando o pacote ESP32 versão 3.2.0 e a
placa genérica `esp32:esp32:esp32`.

O programa atual não realiza controle de relé ou buzzer e não calcula potência.
