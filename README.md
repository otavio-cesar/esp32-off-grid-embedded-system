# Monitor de corrente e controle de relé com ESP32

Este projeto utiliza um ESP32 para monitorar a corrente elétrica medida por um
sensor de corrente **SCT-13 10 A / 1 V** e controlar um relé conforme a potência
estimada da carga.

## Funcionamento

O sinal analógico condicionado do sensor é lido pelo **GPIO 34** por meio do ADC
do ESP32. A tensão medida é convertida em corrente usando a função de primeiro
grau obtida na calibração:

```text
corrente (A) = 13,6436 × tensão do ADC (V) + 4,4319
```

Valores de corrente inferiores a **5,9 A** são considerados parte da zona morta
da medição e, nesse caso, a corrente e a potência são definidas como zero.

A potência é estimada considerando uma tensão residencial de **127 V**:

```text
potência (W) = corrente (A) × 127 V
```

O relé é controlado pelo **GPIO 26** e opera com lógica ativa em nível baixo.
Para evitar comutações causadas por oscilações próximas ao limite, o controle
utiliza histerese e temporização:

- Potência acima de **1000 W** durante 5 segundos: desliga o relé.
- Potência acima de **1200 W** durante 2 segundos: desliga o relé.
- Potência abaixo de **900 W** durante 5 segundos: aciona o relé.
- Potência entre **900 W e 1000 W**: mantém o estado atual.
- Depois de desligar, mantém o relé desativado por pelo menos 30 segundos.
- O intervalo de 30 segundos não bloqueia nem atrasa um desligamento por
  sobrecarga; ele protege somente o religamento.

Ao iniciar, o programa coloca o GPIO 26 em nível alto para manter o relé
desligado. O contator é normalmente fechado para a concessionária; portanto,
a carga permanece alimentada pela rede enquanto o ESP32 inicializa ou enquanto
o relé está desenergizado.

O acionamento do relé fica bloqueado durante os primeiros **60 segundos** após a
inicialização. Encerrado esse período, a potência ainda precisa permanecer
abaixo de **900 W** durante 5 segundos. Assim, o primeiro acionamento pode
ocorrer aproximadamente 65 segundos após a inicialização. Esse bloqueio inicial
é independente do intervalo de 30 segundos aplicado depois de um desligamento
por sobrecarga.

## Exibição e atualização

Um display OLED SSD1306 de **128 × 64 pixels**, no endereço I²C `0x3C`, mostra:

1. tensão lida no ADC, em volts;
2. corrente calculada, em amperes;
3. potência estimada, em watts.

As medições e o controle são atualizados aproximadamente a cada 600 ms. O
programa também envia uma mensagem de atividade pela porta serial.

## Ligações principais

| Componente | Pino do ESP32 | Função |
|---|---:|---|
| Sinal condicionado do SCT-13 | GPIO 34 | Entrada analógica |
| Módulo relé | GPIO 26 | Saída digital ativa em nível baixo |
| OLED SSD1306 | Barramento I²C | Exibição das medições |

## Bibliotecas

- Adafruit GFX Library
- Adafruit SSD1306
- Wire
- SPI

O arquivo contém apenas trechos comentados relacionados ao sensor DHT e ao
GPIO 4. A biblioteca e o sensor não são utilizados na versão atual.
