#include <Arduino.h>
#include <math.h>

constexpr uint8_t SIGNAL_PIN = A0;
constexpr uint8_t LED_PIN = 8;

// These values assume an active-low LED.
// Swap HIGH and LOW if your LED is active-high.
constexpr uint8_t LED_OFF = HIGH;
constexpr uint8_t LED_ON  = LOW;

constexpr uint32_t NSAMPLES = 32767UL;
constexpr unsigned long LOWER_RR_MS = 500UL;
constexpr unsigned long UPPER_RR_MS = 2000UL;
constexpr unsigned long MEASUREMENT_MS = 300000UL;

bool pulseLatched = false;
bool firstBeat = false;
bool finished = false;

double signalAvg = 0.0;
double signalSd = 0.0;
double cutoff = 500.0;
int rrbase[600];

unsigned long previousBeatMs = 0;
unsigned long startMs = 0;

// Online RR statistics.
uint32_t rrCount = 0;
double meanRR = 0.0;
double m2RR = 0.0;

static inline void swapIfGreater(int &a, int &b) {
  if (a > b) {
    const int temp = a;
    a = b;
    b = temp;
  }
}

int median5(int a, int b, int c, int d, int e) {
  swapIfGreater(a, b);
  swapIfGreater(b, c);
  swapIfGreater(c, d);
  swapIfGreater(d, e);

  swapIfGreater(a, b);
  swapIfGreater(b, c);
  swapIfGreater(c, d);

  swapIfGreater(a, b);
  swapIfGreater(b, c);

  swapIfGreater(a, b);

  return c;
}

int filteredRead() {
  const int a = analogRead(SIGNAL_PIN);
  const int b = analogRead(SIGNAL_PIN);
  const int c = analogRead(SIGNAL_PIN);
  const int d = analogRead(SIGNAL_PIN);
  const int e = analogRead(SIGNAL_PIN);

  return median5(a, b, c, d, e);
}

// Welford's online algorithm for signal mean and variance.
void calibrateSignal() {
  double m2 = 0.0;
  signalAvg = 0.0;

  for (uint32_t n = 1; n <= NSAMPLES; ++n) {
    const double sample = static_cast<double>(filteredRead());

    const double delta = sample - signalAvg;
    signalAvg += delta / static_cast<double>(n);
    m2 += delta * (sample - signalAvg);
  }

  // Population standard deviation for the calibration samples.
  signalSd = sqrt(m2 / static_cast<double>(NSAMPLES));
  cutoff = signalAvg + signalSd;
}

// Welford's online algorithm for RR intervals.
void addRR(unsigned long rrMs) {
  rrbase[rrCount] = rrMs;
  ++rrCount;

  const double value = static_cast<double>(rrMs);
  const double delta = value - meanRR;

  meanRR += delta / static_cast<double>(rrCount);
  m2RR += delta * (value - meanRR);
}

void printRRStats() {
  if (rrCount == 0) {
    Serial.println(F("Nenhum intervalo RR valido."));
    return;
  }

  const double averageHR = 60000.0 / meanRR;

  // Sample standard deviation. Use rrCount instead of rrCount - 1
  // here if you specifically require population standard deviation.
  const double sdRR =
      rrCount > 1
          ? sqrt(m2RR / static_cast<double>(rrCount - 1))
          : 0.0;

  Serial.print(F("Frequencia cardiaca media (/min): "));
  Serial.println(averageHR, 2);

  Serial.print(F("Intervalo RR medio (ms): "));
  Serial.println(meanRR, 2);

  Serial.print(F("SDRR (ms): "));
  Serial.println(sdRR, 2);
}

void printtempo() {
  char buffer[10];
  unsigned long int now = millis();
  int tempo_decorrido = now - startMs;
  int segundos = tempo_decorrido / 1000;
  int minutos = segundos / 60;
  segundos %= 60;
  sprintf(buffer, "%02d", minutos);
  Serial.print(buffer);
  Serial.print(":");
  sprintf(buffer, "%02d", segundos);
  Serial.println(buffer);
}

void setup() {
  Serial.begin(115200);
  
  for(;!Serial;); // waits for serial port;

  Serial.println("Porta serial ok.");

  pinMode(SIGNAL_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  Serial.println(F("Calibrando..."));
  calibrateSignal();

  Serial.print(F("Media do sinal: "));
  Serial.println(signalAvg, 2);

  Serial.print(F("Desvio-padrao do sinal: "));
  Serial.println(signalSd, 2);

  Serial.print(F("Limiar: "));
  Serial.println(cutoff, 2);

  for (uint8_t i = 0; i < 3; ++i) {
    digitalWrite(LED_PIN, LED_ON);
    delay(200);

    digitalWrite(LED_PIN, LED_OFF);
    delay(200);
  }

  // Keep the physical LED and software state consistent.
  digitalWrite(LED_PIN, LED_OFF);
  pulseLatched = false;

  startMs = millis();

  Serial.print(F("Inicio: "));
  Serial.println(startMs);
}

void loop() {
  const unsigned long currentMs = millis();

  // Print the final result exactly once.
  if (!finished && currentMs - startMs >= MEASUREMENT_MS) {
    finished = true;
    pulseLatched = false;
    digitalWrite(LED_PIN, LED_OFF);

    Serial.println(F("Resultado final:"));
    
    for (int i = 0; i < 600; ++i) {
      Serial.println(rrbase[i]);
    }
    
    printRRStats();
    
  }

  if (finished) {
    return;
  }

  const int x = filteredRead();

  // Rising threshold crossing.
  if (!pulseLatched && static_cast<double>(x) > cutoff) {
    pulseLatched = true;
    digitalWrite(LED_PIN, LED_ON);

    const unsigned long beatMs = millis();

    if (!firstBeat) {
      firstBeat = true;
      previousBeatMs = beatMs;
    } else {
      const unsigned long candidateRR = beatMs - previousBeatMs;

      if (candidateRR >= LOWER_RR_MS &&
          candidateRR <= UPPER_RR_MS) {
        previousBeatMs = beatMs;
        addRR(candidateRR);
        printRRStats();
        Serial.print("Tempo decorrido: ");
        printtempo();
      } else if (candidateRR > UPPER_RR_MS) {
        // Long gap: discard this interval and resynchronize.
        previousBeatMs = beatMs;
      }

      // A too-short interval is treated as noise, so previousBeatMs
      // is deliberately not changed.
    }
  }
  // Hysteresis: re-arm only after falling below the signal average.
  else if (pulseLatched &&
           static_cast<double>(x) < signalAvg) {
    pulseLatched = false;
    digitalWrite(LED_PIN, LED_OFF);
  }
}