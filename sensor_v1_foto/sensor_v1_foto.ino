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
const int numeroAmostras = 400;

void setup() {
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
  mostrarMedicao(vpp, vmin, vmax);
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

void mostrarMedicao(float vpp, float vmin, float vmax) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Vpp ");
  display.print(vpp, 2);
  display.println("V");

  display.setCursor(0, 22);
  display.print("Vmin ");
  display.print(vmin, 2);
  display.println("V");

  display.setCursor(0, 44);
  display.print("Vmax ");
  display.print(vmax, 2);
  display.println("V");

  display.display();
}
