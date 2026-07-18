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
const float coeficienteA = 20.453f;
const float coeficienteB = -0.7362f;
const float raizDeDois = 1.41421356f;

#define RELE1 26
#define BUZZER 27

const float potenciaLigar = 1000.0;
const float potenciaSobrecarga = 1200.0;
const float potenciaPico = 1450.0;
const unsigned long tempoConfirmacaoSobrecarga = 10000UL;
const unsigned long tempoConfirmacaoPico = 2000UL;
const unsigned long tempoConfirmacaoLigamento = 5000UL;
const unsigned long tempoMinimoDesligado = 30000UL;
const unsigned long tempoEsperaInicial = 60000UL;

bool releLigado = false;
bool releFoiDesligado = false;
bool esperaInicialConcluida = false;
unsigned long inicioSobrecarga = 0;
unsigned long inicioPico = 0;
unsigned long inicioCondicaoLigar = 0;
unsigned long momentoDesligamentoRele = 0;

void setup() {
  pinMode(RELE1, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  // Mantem o rele desligado durante a inicializacao.
  digitalWrite(RELE1, HIGH);
  digitalWrite(BUZZER, LOW);

  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Falha ao inicializar o display SSD1306."));
    for (;;) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.display();
}

void loop() {
  float vmin;
  float vmax;

  medirExtremosTensao(vmin, vmax);

  float vpp = vmax - vmin;
  float vp = vpp / 2.0f;
  float vrms = vp / raizDeDois;
  float corrente = coeficienteA * vrms + coeficienteB;

  if (corrente < 0.0f) {
    corrente = 0.0f;
  }

  float potencia = corrente * tensaoResidencia;
  displayLED(vpp, vrms, vmin, vmax, corrente, potencia);

  controlarRele(potencia);
  Serial.println("Enviando dados pela serial...");

  delay(500);
}

void medirExtremosTensao(float &vmin, float &vmax) {
  uint32_t minimoMv = 0;
  uint32_t maximoMv = 0;
  unsigned long proximaAmostra = micros();

  for (int i = 0; i < numeroAmostras; i++) {
    long tempoRestante = (long)(proximaAmostra - micros());
    if (tempoRestante > 0) {
      delayMicroseconds((unsigned int)tempoRestante);
    }
    proximaAmostra += periodoAmostragemUs;

    uint32_t amostraMv = analogReadMilliVolts(pinoADC);

    if (i == 0 || amostraMv < minimoMv) {
      minimoMv = amostraMv;
    }

    if (i == 0 || amostraMv > maximoMv) {
      maximoMv = amostraMv;
    }
  }

  vmin = minimoMv / 1000.0f;
  vmax = maximoMv / 1000.0f;
}

void controlarRele(float potencia) {
  unsigned long agora = millis();

  if (releLigado) {
    bool deveDesligar = false;
    inicioCondicaoLigar = 0;

    // Sobrecarga: desliga apos 10 segundos entre 1200 W e 1450 W.
    if (potencia >= potenciaSobrecarga && potencia <= potenciaPico) {
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

    // Pico critico: desliga apos 2 segundos acima de 1450 W.
    if (potencia > potenciaPico) {
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

    // O bloqueio de 30 segundos nunca atrasa um desligamento.
    if (deveDesligar) {
      releLigado = false;
      digitalWrite(RELE1, HIGH);
      digitalWrite(BUZZER, HIGH);

      momentoDesligamentoRele = agora;
      releFoiDesligado = true;
      inicioSobrecarga = 0;
      inicioPico = 0;

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
  digitalWrite(BUZZER, LOW);
  inicioCondicaoLigar = 0;

  Serial.println("Relé acionado.");
}

void displayLED(float vpp, float vrms, float vmin, float vmax,
                float corrente, float potencia) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Vpp ");
  display.print(vpp, 2);
  display.print(" Vrms ");
  display.print(vrms, 2);

  display.setCursor(0, 9);
  display.print("Min ");
  display.print(vmin, 2);
  display.print(" Max ");
  display.print(vmax, 2);

  display.setTextSize(2);
  display.setCursor(0, 21);
  display.print("I ");
  display.print(corrente);
  display.print(" A");

  display.setCursor(0, 43);
  display.print("P ");
  display.print(potencia, 0);
  display.print(" W");

  display.display();
}
