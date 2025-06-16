#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include "internet.h" // Certifique-se de que essa biblioteca conecta ao Wi-Fi
#include <ArduinoJson.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);

const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_id = "trackbox-central";
const char *mqtt_topic_sub = "trackbox/sensores";

LiquidCrystal_I2C lcd(0x27, 20, 4); // LCD 20x4

bool trava;
bool movimento;
bool alerta = 0;
float lat;
float temp;
float longi;
long timesTemp;

void callback(char *topic, byte *payload, unsigned int length) {

  String mensagem = "";
  for (unsigned int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, mensagem);
  if (error) {
    Serial.print("Erro ao fazer parse do JSON: ");
    Serial.println(error.c_str());
    return;
  }

  if (!doc["trava"].isNull()) trava = doc["trava"];
  if (!doc["movimento"].isNull()) movimento = doc["movimento"];
  if (!doc["alerta"].isNull()) alerta = doc["alerta"];
  if (!doc["timestamp"].isNull()) timesTemp = doc["timestamp"];
  if (!doc["latitude"].isNull()) lat = doc["latitude"];
  if (!doc["longitude"].isNull()) longi = doc["longitude"];
  if (!doc["temperatura"].isNull()) temp = doc["temperatura"];
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT...");
    if (client.connect(mqtt_id)) {
      Serial.println("Conectado!");
      client.subscribe(mqtt_topic_sub);
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  conectaWiFi(); // Função da sua biblioteca "internet.h"

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
    client.loop();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LAT: ");
    lcd.print(lat, 8);
    lcd.setCursor(0, 1);
    lcd.print("LONG: ");
    lcd.print(longi, 8);
    lcd.setCursor(0, 2);
    lcd.print("TEMP CARGA: ");
    lcd.print(temp);
    lcd.setCursor(0, 3);
    lcd.print("TRAVA: ");
    lcd.print(trava ? "SIM" : "NAO");

}