#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int pinoADC = 34;
const unsigned long taxaAmostragemHz = 2000UL;
const unsigned long periodoAmostragemUs = 1000000UL / taxaAmostragemHz;
// 200 ms correspondem a 12 ciclos completos de uma rede de 60 Hz.
const int numeroAmostras = 400;
const int tensaoResidencia = 120;
const float coeficienteA = 9.295629481419246f;
const float coeficienteB = -0.178376044032725f;

#define RELE1 26
#define BUZZER 27

const float potenciaLigar = 1000.0;
const float potenciaSobrecarga = 1200.0;
const unsigned long tempoConfirmacaoSobrecarga = 2000UL;
const unsigned long tempoConfirmacaoPico = 1000UL;
const unsigned long tempoConfirmacaoLigamento = 5000UL;
const unsigned long tempoMinimoDesligado = 30000UL;
const unsigned long tempoEsperaInicial = 60000UL;
const unsigned long duracaoBip = 500UL;

bool releLigado = false;
bool releFoiDesligado = false;
bool esperaInicialConcluida = false;
bool buzzerLigado = false;
unsigned long inicioSobrecarga = 0;
unsigned long inicioPico = 0;
unsigned long inicioCondicaoLigar = 0;
unsigned long momentoDesligamentoRele = 0;
unsigned long inicioBip = 0;

void setup() {
  pinMode(RELE1, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  // Mantem o rele desligado durante a inicializacao.
  digitalWrite(RELE1, HIGH);
  digitalWrite(BUZZER, LOW);

  iniciarBip();
  aguardarAtualizandoBuzzer(duracaoBip);

  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Falha ao inicializar o display SSD1306."));
    for (;;) {
      delay(1000);
    }
  }

  // Gira a interface 90 graus no sentido horario (formato 64 x 128).
  display.setRotation(1);
  display.clearDisplay();
  display.display();
}

void loop() {
  float vrms = medirTensaoRms();

  float corrente = coeficienteA * vrms + coeficienteB;

  if (corrente < 0.0f) {
    corrente = 0.0f;
  }

  float potencia = corrente * tensaoResidencia;
  displayLED(vrms, corrente, potencia);

  controlarRele(potencia);
  verificarBuzzer(potencia);
  Serial.println("Enviando dados pela serial...");

  aguardarAtualizandoBuzzer(500UL);
}

float medirTensaoRms() {
  double mediaMv = 0.0;
  double somaDesviosQuadrados = 0.0;
  unsigned long proximaAmostra = micros();

  for (int i = 0; i < numeroAmostras; i++) {
    long tempoRestante = (long)(proximaAmostra - micros());
    if (tempoRestante > 0) {
      delayMicroseconds((unsigned int)tempoRestante);
    }
    proximaAmostra += periodoAmostragemUs;

    uint32_t amostraMv = analogReadMilliVolts(pinoADC);

    // Algoritmo de Welford: calcula o RMS da componente alternada usando
    // todas as amostras. A media representa o bias DC e e removida.
    double delta = (double)amostraMv - mediaMv;
    mediaMv += delta / (i + 1);
    double delta2 = (double)amostraMv - mediaMv;
    somaDesviosQuadrados += delta * delta2;
  }

  return sqrt(somaDesviosQuadrados / numeroAmostras) / 1000.0;
}

void controlarRele(float potencia) {
  unsigned long agora = millis();

  if (releLigado) {
    bool deveDesligar = false;
    inicioCondicaoLigar = 0;

    // Sobrecarga: desliga apos 2 segundos continuos a partir de 1000 W.
    if (potencia >= potenciaLigar) {
      if (inicioSobrecarga == 0) {
        inicioSobrecarga = agora;
      }
      else if (agora - inicioSobrecarga >= tempoConfirmacaoSobrecarga) {
        deveDesligar = true;
      }
    }
    else {
      inicioSobrecarga = 0;
    }

    // Pico: desliga apos 1 segundo continuo acima de 1200 W.
    if (potencia > potenciaSobrecarga) {
      if (inicioPico == 0) {
        inicioPico = agora;
      }
      else if (agora - inicioPico >= tempoConfirmacaoPico) {
        deveDesligar = true;
      }
    }
    else {
      inicioPico = 0;
    }

    if (deveDesligar) {
      releLigado = false;
      digitalWrite(RELE1, HIGH);

      momentoDesligamentoRele = agora;
      releFoiDesligado = true;
      inicioSobrecarga = 0;
      inicioPico = 0;

      iniciarBip();
      Serial.println("Relé desativado por sobrecarga.");
    }

    return;
  }

  inicioSobrecarga = 0;
  inicioPico = 0;

  // Mantém o relé desligado durante os primeiros 60 segundos após a inicialização.
  if (!esperaInicialConcluida) {
    if (agora < tempoEsperaInicial) {
      inicioCondicaoLigar = 0;
      return;
    }

    esperaInicialConcluida = true;
  }

  // Apos um desligamento, mantem o rele desativado por pelo menos 30 segundos.
  if (releFoiDesligado &&
      agora - momentoDesligamentoRele < tempoMinimoDesligado) {
    inicioCondicaoLigar = 0;
    return;
  }

  // Religa somente se a potencia permanecer abaixo de 1000 W por 5 segundos.
  if (potencia >= potenciaLigar) {
    inicioCondicaoLigar = 0;
    return;
  }

  if (inicioCondicaoLigar == 0) {
    inicioCondicaoLigar = agora;
    return;
  }

  if (agora - inicioCondicaoLigar < tempoConfirmacaoLigamento) {
    return;
  }

  releLigado = true;
  digitalWrite(RELE1, LOW);
  inicioCondicaoLigar = 0;

  Serial.println("Relé acionado.");
}

void verificarBuzzer(float potencia) {
  (void)potencia;
  atualizarBuzzer();
}

void iniciarBip() {
  digitalWrite(BUZZER, HIGH);
  buzzerLigado = true;
  inicioBip = millis();
}

void atualizarBuzzer() {
  if (buzzerLigado && millis() - inicioBip >= duracaoBip) {
    digitalWrite(BUZZER, LOW);
    buzzerLigado = false;
  }
}

void aguardarAtualizandoBuzzer(unsigned long tempoEspera) {
  unsigned long inicioEspera = millis();

  while (millis() - inicioEspera < tempoEspera) {
    atualizarBuzzer();
    delay(1);
  }

  atualizarBuzzer();
}

void displayLED(float vrms, float corrente, float potencia) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Vrms ");
  display.print(vrms, 2);
  display.print("V");

  display.setTextSize(1);
  display.setCursor(0, 25);
  display.print("Corrente A");

  display.setTextSize(2);
  display.setCursor(0, 35);
  display.print(corrente, 2);

  display.setTextSize(1);
  display.setCursor(0, 60);
  display.print("Potencia W");

  display.setTextSize(2);
  display.setCursor(0, 70);
  display.print(potencia, 0);

  display.display();
}
