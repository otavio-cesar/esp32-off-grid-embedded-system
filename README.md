# Monitor de corrente e controle de relé com ESP32

Este projeto utiliza um ESP32 para monitorar a corrente elétrica medida por um
sensor de corrente **SCT-13 10 A / 1 V** e controlar um relé conforme a potência
estimada da carga.

## Funcionamento

O sinal analógico condicionado do sensor é lido pelo **GPIO 34** por meio do ADC
do ESP32. O sinal senoidal ocupa aproximadamente a faixa de 0 a 3 V e tem um
offset próximo de 1,5 V. O firmware coleta 400 amostras a 2 kHz durante 200 ms
(12 ciclos completos de 60 Hz), calcula o offset pela média das amostras e o
remove antes de calcular a tensão RMS da componente alternada. A tensão RMS é
convertida em corrente usando a função de primeiro
grau obtida na calibração:

```text
corrente RMS (A) = 35,5249 × tensão AC RMS do ADC (V) + 0,0848
```

Esse ajuste foi obtido por regressão linear dos dados simulados do novo
condicionador, depois de retirar em quadratura o offset de 1,5 V da tensão RMS
total. O coeficiente de determinação do ajuste é aproximadamente `R² = 0,9996`.

A potência é estimada considerando uma tensão residencial de **120 V**:

```text
potência (W) = corrente (A) × 120 V
```

O relé é controlado pelo **GPIO 26** e opera com lógica ativa em nível baixo.
Para evitar comutações causadas por oscilações próximas ao limite, o controle
utiliza histerese e temporização:

- Potência entre **1200 W e 1450 W** durante 10 segundos: desliga o relé
  e aciona o buzzer.
- Potência acima de **1450 W** durante 2 segundos: desliga o relé e aciona
  o buzzer.
- Potência abaixo de **1000 W** durante 5 segundos: aciona o relé e desliga
  o buzzer.
- Potência entre **1000 W e 1200 W**: mantém o estado atual.
- Depois de desligar, mantém o relé desativado por pelo menos 30 segundos.
- O intervalo de 30 segundos não bloqueia nem atrasa um desligamento por
  sobrecarga; ele protege somente o religamento.

Ao iniciar, o programa coloca o GPIO 26 em nível alto para manter o relé
desligado. O contator é normalmente fechado para a concessionária; portanto,
a carga permanece alimentada pela rede enquanto o ESP32 inicializa ou enquanto
o relé está desenergizado.

O acionamento do relé fica bloqueado durante os primeiros **60 segundos** após a
inicialização. Encerrado esse período, a potência ainda precisa permanecer
abaixo de **1000 W** durante 5 segundos. Assim, o primeiro acionamento pode
ocorrer aproximadamente 65 segundos após a inicialização. Esse bloqueio inicial
é independente do intervalo de 30 segundos aplicado depois de um desligamento
por sobrecarga.

## Exibição e atualização

Um display OLED SSD1306 de **128 × 64 pixels**, no endereço I²C `0x3C`, mostra:

1. tensão lida no ADC, em volts;
2. corrente calculada, em amperes;
3. potência estimada, em watts.

Cada janela de medição RMS dura 200 ms. Depois da atualização do display, o
laço aguarda 500 ms antes de iniciar uma nova janela. O
programa também envia uma mensagem de atividade pela porta serial.

## Ligações principais

| Componente | Pino do ESP32 | Função |
|---|---:|---|
| Sinal condicionado do SCT-13 | GPIO 34 | Entrada analógica |
| Módulo relé | GPIO 26 | Saída digital ativa em nível baixo |
| Buzzer ativo | GPIO 27 | Alarme de desligamento por sobrecarga |
| OLED SSD1306 | Barramento I²C | Exibição das medições |

## Bibliotecas

- Adafruit GFX Library
- Adafruit SSD1306
- Wire
- SPI

O arquivo contém apenas trechos comentados relacionados ao sensor DHT e ao
GPIO 4. A biblioteca e o sensor não são utilizados na versão atual.
