#include <DHT.h>
#define DHT11_PIN  4 // ESP32 pin GPIO21 connected to DHT11 sensor
DHT dht11(DHT11_PIN, DHT11);
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
const int sctval = 10;
const int tensaoResidencia = 127;
const float aFx = 13.6436;
const float bFx = 4.4319;
const float deadZone = 5.9; // Menos que isso ainda não é possível ler.
int ct = 0;
// Caso use ADC_11db entao 3.3
// Caso use ADC_0db entao 0.9
const float atenuacao = 3.3;
#define RELE1 26

const float potenciaDesligar = 1050.0;
const float potenciaLigar = 900.0;
const unsigned long tempoConfirmacaoRele = 5000UL;
const unsigned long tempoMinimoEntreComutacoes = 30000UL;

bool releLigado = false;
bool releJaComutou = false;
unsigned long inicioCondicaoRele = 0;
unsigned long ultimaComutacaoRele = 0;


void setup() {
  pinMode(RELE1, OUTPUT);
  // Desliga os relés ao iniciar
  digitalWrite(RELE1, HIGH);

  Serial.begin(115200);
  analogReadResolution(12); // 0-4095
  //analogSetAttenuation(ADC_11db); // permite medir até ~3.3V
  analogSetAttenuation(ADC_6db); // permite medir até ~1.9V
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
  // Leitura usando analogRead
  // int leitura = analogRead(pinoADC);
  // float tensao = (leitura / 4095.0) * atenuacao;  // Vadc
  // Leitura usando analogReadMilliVolts
  int leitura = analogReadMilliVolts(pinoADC);
  float tensao = leitura / 1000.0; // Vadc
  float corrente = tensao * aFx + bFx;
  float potencia = corrente * tensaoResidencia;
  if (corrente < deadZone)
  {
    corrente = 0;
    potencia = 0;
  }
  displayLED(tensao, corrente, potencia);

  controlarRele(potencia);
  Serial.println("Enviando dados pela serial...");

  delay(500);
}

void controlarRele(float potencia) {
  unsigned long agora = millis();
  bool novoEstado;

  // Histerese: dentro da faixa entre os limites, mantém o estado atual.
  if (releLigado && potencia > potenciaDesligar) {
    novoEstado = false;
  }
  else if (!releLigado && potencia < potenciaLigar) {
    novoEstado = true;
  }
  else {
    inicioCondicaoRele = 0;
    return;
  }

  // Protege o contator contra duas comutações muito próximas.
  if (releJaComutou &&
      agora - ultimaComutacaoRele < tempoMinimoEntreComutacoes) {
    inicioCondicaoRele = 0;
    return;
  }

  // A condição precisa permanecer válida antes de alterar o relé.
  if (inicioCondicaoRele == 0) {
    inicioCondicaoRele = agora;
    return;
  }

  if (agora - inicioCondicaoRele < tempoConfirmacaoRele) {
    return;
  }

  releLigado = novoEstado;
  digitalWrite(RELE1, releLigado ? LOW : HIGH);

  ultimaComutacaoRele = agora;
  releJaComutou = true;
  inicioCondicaoRele = 0;

  Serial.println(releLigado ? "Relé acionado." : "Relé desativado.");
}

void displayLED(float t,float t2, float t3) {
  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(String(t));
  display.println(String(t2));
  display.println(String(t3));
  display.display();      
  delay(100);
}
