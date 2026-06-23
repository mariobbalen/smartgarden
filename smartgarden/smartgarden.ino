#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <time.h>

#include "secrets.h"
#include "pins.h"

const char* TABLE_NAME    = "sensor_data";

const char* NTP_SERVER   = "pool.ntp.org";
const long  UTC_OFFSET_S = -3 * 3600;

const int SECTOR_ID = 1;

const int   LDR_THRESHOLD         = 40;
const float LOW_WATER_THRESHOLD_L = 5.0;

const float TANK_RADIUS_CM = 15.0;
const float TANK_HEIGHT_CM = 40.0;

Servo servo;

void setup() {
  Serial.begin(115200);

  pinMode(SOIL_MOISTURE_PIN, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  servo.attach(SERVO_PIN);

  connectWiFi();
  configTime(UTC_OFFSET_S, 0, NTP_SERVER);

  Serial.print("Sincronizando tempo");
  while (time(nullptr) < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" pronto");
}

void loop() {
  bool soil_moisture = getSoilMoisture();
  int lumi   = getLumi();
  long distance = getDistanceCm();
  float volumeL = getReservoirVolume(distance);

  controlActuators(soil_moisture, lumi, volumeL);
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Enviando para o Supabase...");
    postToSupabase(soil_moisture, lumi, volumeL, getTimestamp());
  } else {
    Serial.println("Sem envio: WiFi off");
  }

  delay(15000);
}

//pisca LEDs
void blinkLed(int pin) {
  unsigned long start = millis();
  bool state = false;
  while (millis() - start < 5000) {
    state = !state;
    digitalWrite(pin, state ? HIGH : LOW);
    delay(250);
  }
  digitalWrite(pin, LOW);
}

//controla os atuadores (LEDs e Servo)
void controlActuators(int soil_moisture, int ldr, float volumeL) {
  if (ldr <= LDR_THRESHOLD) 
    blinkLed(GREEN_LED);
    
  if (volumeL <= LOW_WATER_THRESHOLD_L) 
    blinkLed(RED_LED);

  //se estiver umido, n faz nada
  if(soil_moisture)
    return;

  //se estiver seco, liga o led, gira o servo e desliga novamente
  digitalWrite(YELLOW_LED, HIGH);
  for (int pos = 0; pos <= 180; pos++) {
    servo.write(pos);
    delay(15);
  }
  for (int pos = 180; pos >= 0; pos--) {
    servo.write(pos);
    delay(15);
  }
  digitalWrite(YELLOW_LED, LOW);

}

//verifica umidade do solo
bool getSoilMoisture() {
  // LOW = seco, HIGH = umido.
  bool soil_moisture = digitalRead(SOIL_MOISTURE_PIN);
  //inverte para que fique com low = seco e high = umido
  soil_moisture = !soil_moisture;
  Serial.print("Umidade Solo = ");
  Serial.println(soil_moisture ? "HIGH (umido)" : "LOW (seco)");
  return soil_moisture;
}

//verifica LDR
int getLumi() {
  int ldr = analogRead(LDR_PIN);
  Serial.print("LDR = ");
  Serial.println(ldr);
  return ldr;
}

//pega o volume de água no reservatório
float getReservoirVolume(long distanceCm) {
  float waterHeightCm = constrain(TANK_HEIGHT_CM - distanceCm, 0, TANK_HEIGHT_CM);
  float volumeCm3 = PI * TANK_RADIUS_CM * TANK_RADIUS_CM * waterHeightCm;
  float volumeL = volumeCm3 / 1000.0;
  Serial.print("Volume (L) = ");
  Serial.println(volumeL);
  return volumeL;
}

//pega a distancia para calcular o volume
long getDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = (duration / 2) / 29.1;
  Serial.print("Distancia em cm: ");
  Serial.println(distance);
  delay(50);
  return distance;
}

//faz o post para o supabase
void postToSupabase(int soil_moisture, int lumi, float volumeLiters, const String& timestamp) {
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure());
  client->setInsecure();

  //headers de autorizacao
  HTTPClient http;
  http.begin(*client, String(SUPABASE_URL) + "/rest/v1/" + TABLE_NAME);
  http.addHeader("Content-Type",  "application/json");
  http.addHeader("apikey",        SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Prefer",        "return=minimal");

  //cria o json que vai enviar para alimentar a tabela
  StaticJsonDocument<160> doc;
  doc["sector"]             = SECTOR_ID;
  doc["soil_moisture"]      = soil_moisture;
  doc["luminosity"]         = lumi;
  doc["reservoir_volume_l"] = volumeLiters;
  doc["recorded_at"]        = timestamp;

  String body;
  serializeJson(doc, body);
  int statusCode = http.POST(body);

  if (statusCode > 0) {
    Serial.print("Codigo resposta Supabase: ");
    Serial.println(statusCode);
    if (statusCode >= 400) {
      Serial.println(http.getString());
    }
  } else {
    Serial.print("POST falhou, erro: ");
    Serial.println(http.errorToString(statusCode));
  }

  http.end();
}

//pgea o horario atual para enviar na tabela do supa
String getTimestamp() {
  time_t now = time(nullptr);
  
  if (now < 8 * 3600 * 2) 
    return "1970-01-01T00:00:00";
    
  struct tm t;
  localtime_r(&now, &t);
  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &t);
  return String(buf);
}

//faz a conexão com o wifi
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}
