// #include <DHT.h>
// #define DHT11_PIN  4 // ESP32 pin GPIO21 connected to DHT11 sensor
// DHT dht11(DHT11_PIN, DHT11);
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define NUMFLAKES     10 // Number of snowflakes in the animation example
#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16
static const unsigned char PROGMEM logo_bmp[] ={ 0b00000000, 0b11000000,  0b00000001, 0b11000000,  0b00000001, 0b11000000,  0b00000011, 0b11100000,  0b11110011, 0b11100000,  0b11111110, 0b11111000,  0b01111110, 0b11111111, 0b00110011, 0b10011111,  0b00011111, 0b11111100,  0b00001101, 0b01110000,  0b00011011, 0b10100000,  0b00111111, 0b11100000,  0b00111111, 0b11110000,  0b01111100, 0b11110000,  0b01110000, 0b01110000,  0b00000000, 0b00110000 };

const int pinoADC = 34;
const unsigned long taxaAmostragemHz = 2000UL;
const unsigned long periodoAmostragemUs = 1000000UL / taxaAmostragemHz;
// 200 ms correspondem a 12 ciclos completos de uma rede de 60 Hz.
const int numeroAmostrasRms = 400;
const int sctval = 10;
const int tensaoResidencia = 120;
// Ajuste linear obtido a partir dos dados simulados do novo condicionador.
// A tensao usada aqui e o RMS da componente AC, depois da remocao do offset.
const float aFx = 35.5249;
const float bFx = 0.0848;
int ct = 0;
// Caso use ADC_11db entao 3.3
// Caso use ADC_0db entao 0.9
const float atenuacao = 3.3;
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
  // Desliga os relés ao iniciar
  digitalWrite(RELE1, HIGH);
  digitalWrite(BUZZER, LOW);

  Serial.begin(115200);
  analogReadResolution(12); // 0-4095
  analogSetAttenuation(ADC_11db); // permite medir até ~3.3V
  // analogSetAttenuation(ADC_6db); // permite medir até ~1.9V
  Serial.begin(9600);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();
  delay(2000); // Pause for 2 seconds
  display.clearDisplay();
  display.drawPixel(10, 10, SSD1306_WHITE);
  display.display();
}

void loop() {
  float tensao = medirTensaoRms();
  float corrente = tensao * aFx + bFx;
  float potencia = corrente * tensaoResidencia;
  displayLED(tensao, corrente, potencia);

  controlarRele(potencia);
  Serial.println("Enviando dados pela serial...");

  delay(500);
}

float medirTensaoRms() {
  double mediaMv = 0.0;
  double somaDesviosQuadrados = 0.0;
  unsigned long proximaAmostra = micros();

  for (int i = 0; i < numeroAmostrasRms; i++) {
    // Agenda cada conversao a partir do instante inicial. Assim, o tempo gasto
    // em analogReadMilliVolts() nao e somado ao periodo de 500 us.
    long tempoRestante = (long)(proximaAmostra - micros());
    if (tempoRestante > 0) {
      delayMicroseconds((unsigned int)tempoRestante);
    }
    proximaAmostra += periodoAmostragemUs;

    double amostraMv = analogReadMilliVolts(pinoADC);

    // Algoritmo de Welford: calcula a variancia sem armazenar as 400 amostras.
    // A media representa o offset DC e e removida do resultado RMS.
    double delta = amostraMv - mediaMv;
    mediaMv += delta / (i + 1);
    double delta2 = amostraMv - mediaMv;
    somaDesviosQuadrados += delta * delta2;
  }

  return sqrt(somaDesviosQuadrados / numeroAmostrasRms) / 1000.0;
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

  // Ap�s um desligamento, mantém o relé desativado por pelo menos 30 segundos.
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

void displayLED(float tensao, float corrente, float potencia) {
  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.print(tensao);
  display.println(" V");
  display.print(corrente);
  display.println(" A");
  display.print(potencia);
  display.println(" W");
  display.display();      
  delay(100);
}
