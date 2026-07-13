#define SIGNAL 0
#define LED_PIN 8
#define LED_DESLIGADO 1
#define LED_LIGADO 0
#define NSAMPLES 32767
#define LOWER_RR_THRESHOLD 500
#define UPPER_RR_THRESHOLD 2000

bool led_state = false;

// parametros de detecção de pulso

int counter = 0;
double signalavg = 0;
int signalmax = 0;
int cutoff = 500;
double signalsum = 0.0;
double signalsumsq = 0.0;
double signalsd = 0.0;
int now = 0;
int before = 0;
int rr = 0;
bool firstbeat = false;
long int rrsum = 0;
long int rrsumsq = 0;
double avgrr = 0.0;
double sdrr = 0.0;
double avghr = 0.0;
long int inicio = 0;

// função que retorna a mediana de 5 valores
int median5(int a, int b, int c, int d, int e) {

  if (a > b) {
    int temp = a;
    a = b;
    b = temp;
  }
  
  if (b > c) {
    int temp = b;
    b = c;
    c = temp;
  }
  
  if (c > d) {
    int temp = c;
    c = d;
    d = temp;
  }
  
  if (d > e) {
    int temp = d;
    d = e;
    e = temp;
  }
  
  if (a > b) {
    int temp = a;
    a = b;
    b = temp;
  }
  
  if (b > c) {
    int temp = b;
    b = c;
    c = temp;
  }
  
  if (c > d) {
    int temp = c;
    c = d;
    d = temp;
  }

  if (a > b) {
    int temp = a;
    a = b;
    b = temp;
  }
    
  if (b > c) {
    int temp = b;
    b = c;
    c = temp;
  }
  
  if (a > b) {
    int temp = a;
    a = b;
    b = temp;
  }

  return c;
  
}

void setup() {
  Serial.begin(115200);
  pinMode(SIGNAL, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_DESLIGADO);
  for (int i = 0; i < NSAMPLES; ++i) {
    int x = analogRead(SIGNAL);
    int y = analogRead(SIGNAL);
    int z = analogRead(SIGNAL);
    int t = analogRead(SIGNAL);
    int w = analogRead(SIGNAL);
    x = median5(x,y,z,t,w);
    signalsum += x;
    signalsumsq += x*x;
    if (x > signalmax) {
      signalmax = x;    
    }
  }
  
  signalavg = signalsum / NSAMPLES;
  signalsd = sqrt(signalsumsq / NSAMPLES - signalavg * signalavg);
   
  cutoff = int (signalavg + signalsd);
  for (int i = 0; i < 3; ++i) {
  digitalWrite(LED_PIN, LED_LIGADO);
  delay(200);
  digitalWrite(LED_PIN, LED_DESLIGADO);
  delay(200);
  }
  digitalWrite(LED_PIN, LED_LIGADO);
  inicio = millis();
}

void loop() {
  int x = analogRead(SIGNAL);
  int y = analogRead(SIGNAL);
  int z = analogRead(SIGNAL);
  int t = analogRead(SIGNAL);
  int w = analogRead(SIGNAL);
  x = median5(x,y,z,t,w);
//  Serial.println(x);
  if (x > cutoff && led_state == false) {
    digitalWrite(LED_PIN, LED_LIGADO);
    led_state = true;
    if (firstbeat) {
      now = millis();
      rr = now-before;
      before = now;
      if (rr > LOWER_RR_THRESHOLD && rr < UPPER_RR_THRESHOLD) {
        ++counter;
        rrsum += rr;
        rrsumsq += rr*rr;
        avgrr = rrsum / counter;
        avghr = 60000 /avgrr;
        sdrr = sqrt(rrsumsq / counter - avgrr * avgrr);
        if (now - inicio <= 300000) {
          Serial.print("Frequência cardíaca média (/min): ");
          Serial.println(avghr);
          Serial.print("Intervalo rr médio (ms): ");
          Serial.println(avgrr);
          Serial.print("sdrr (ms): ");
          Serial.println(sdrr);
        }
        else Serial.println("Resultado final acima.");
      }
    } else {
      now = millis();
      before = now;
      firstbeat = true;
      digitalWrite(LED_PIN, LED_DESLIGADO);
    }
  }
  else if (x < signalavg && led_state == true) {
    digitalWrite(LED_PIN, LED_DESLIGADO);
    led_state = false;
  }
}
