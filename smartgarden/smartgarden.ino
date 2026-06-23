#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <time.h>

#include "secrets.h"
#include "pins.h"

const char* TABLE_NAME        = "sensor_data";
const char* CONFIG_TABLE_NAME = "device_config";

const char* NTP_SERVER   = "pool.ntp.org";
const long  UTC_OFFSET_S = 3 * 3600;

const int SECTOR_ID = 1;

// Valores padrão usados até a primeira leitura de device_config (ou se ela falhar).
int           ldrThreshold     = 40;
float         minReservoirL    = 5.0;
float         tankRadiusCm     = 15.0;
float         tankHeightCm     = 40.0;
unsigned long iterationDelayMs = 15000;
String        sunriseTime      = "";
String        sunsetTime       = "";

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

  fetchConfig();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    fetchConfig();
  }

  bool soil_moisture = getSoilMoisture();
  int lumi      = getLumi();
  long distance = getDistanceCm();
  float volumeL = getReservoirVolume(distance);

  bool daytime       = isDaytime();
  bool low_light     = daytime && lumi <= ldrThreshold;
  bool low_reservoir = volumeL <= minReservoirL;

  controlActuators(soil_moisture, low_light, low_reservoir);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Enviando para o Supabase...");
    postToSupabase(soil_moisture, lumi, daytime, low_light, volumeL, low_reservoir, getTimestamp());
  } else {
    Serial.println("Sem envio: WiFi off");
  }

  delay(iterationDelayMs);
}

//busca a configuração do dispositivo no Supabase
void fetchConfig() {
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure());
  client->setInsecure();

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/" + CONFIG_TABLE_NAME +
               "?select=*&sector=eq." + String(SECTOR_ID);
  http.begin(*client, url);
  http.addHeader("apikey",        SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);

  int statusCode = http.GET();
  if (statusCode != 200) {
    Serial.print("Falha ao buscar config, codigo: ");
    Serial.println(statusCode);
    http.end();
    return;
  }

  String responseBody = http.getString();
  http.end();

  StaticJsonDocument<768> doc;
  DeserializationError err = deserializeJson(doc, responseBody);

  if (err || doc.size() == 0) {
    Serial.print("Config vazia ou invalida, mantendo valores atuais: ");
    Serial.println(err.c_str());
    return;
  }

  JsonObject config   = doc[0];
  sunriseTime          = config["sunrise_time"].as<String>();
  sunsetTime            = config["sunset_time"].as<String>();
  iterationDelayMs     = (unsigned long) config["iteration_interval_s"].as<long>() * 1000UL;
  ldrThreshold          = config["ldr_threshold"].as<int>();
  minReservoirL         = config["min_reservoir_volume_l"].as<float>();
  tankRadiusCm          = config["tank_radius_cm"].as<float>();
  tankHeightCm          = config["tank_height_cm"].as<float>();

  Serial.println("Config atualizada: ");
  Serial.print("  amanhecer=");   Serial.print(sunriseTime);
  Serial.print(" entardecer=");   Serial.print(sunsetTime);
  Serial.print(" intervalo(s)="); Serial.print(iterationDelayMs / 1000);
  Serial.print(" ldrThreshold="); Serial.print(ldrThreshold);
  Serial.print(" minReservoirL="); Serial.print(minReservoirL);
  Serial.print(" tankRadiusCm=");  Serial.print(tankRadiusCm);
  Serial.print(" tankHeightCm="); Serial.println(tankHeightCm);
}

//converte "HH:MM" ou "HH:MM:SS" em minutos desde meia-noite
int timeStringToMinutes(const String& timeStr) {
  return timeStr.substring(0, 2).toInt() * 60 + timeStr.substring(3, 5).toInt();
}

//verifica se o horario atual esta entre o amanhecer e o entardecer
bool isDaytime() {
  if (sunriseTime.length() < 5 || sunsetTime.length() < 5) {
    return true; // sem config valida, assume dia para nao bloquear o aviso de luminosidade
  }

  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  int nowMinutes     = t.tm_hour * 60 + t.tm_min;
  int sunriseMinutes = timeStringToMinutes(sunriseTime);
  int sunsetMinutes  = timeStringToMinutes(sunsetTime);

  return nowMinutes >= sunriseMinutes && nowMinutes < sunsetMinutes;
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
void controlActuators(bool soil_moisture, bool low_light, bool low_reservoir) {
  if (low_light)
    blinkLed(GREEN_LED);

  if (low_reservoir)
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
  float waterHeightCm = constrain(tankHeightCm - distanceCm, 0, tankHeightCm);
  float volumeCm3 = PI * tankRadiusCm * tankRadiusCm * waterHeightCm;
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
void postToSupabase(bool soil_moisture, int lumi, bool daytime, bool low_light, float volumeLiters, bool low_reservoir, const String& timestamp) {
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
  StaticJsonDocument<192> doc;
  doc["sector"]             = SECTOR_ID;
  doc["soil_moisture"]      = soil_moisture;
  //de noite o LDR nao e confiavel, entao envia null em vez do valor bruto
  if (daytime) {
    doc["luminosity"] = lumi;
  } else {
    doc["luminosity"] = nullptr;
  }
  doc["low_light"]          = low_light;
  doc["reservoir_volume_l"] = volumeLiters;
  doc["low_reservoir"]      = low_reservoir;
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
