#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_MMA8452Q.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "internet.h"

const int botaoTrava = 19;
const int ledTrava = 5;
const int ledDestrava = 18;

bool estadoTrava = false; // false = destravado, true = travado
bool ultimoEstadoBotao = HIGH;
float axAnterior = 0, ayAnterior = 0, azAnterior = 0;
const float limiteMovimento = 0.2; // Sensibilidade (menor = mais sensível)

MMA8452Q accel;
WiFiClient espClient;
PubSubClient client(espClient);

const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_id = "trackbox-completo";
const char *TOPICO_MOVIMENTO = "trackbox/movimento";

unsigned long ultimaLeitura = 0;
unsigned long intervalo = 2000;

void mqttConnect() {
  while (!client.connected()) {
    Serial.println("Conectando ao MQTT...");
    if (client.connect(mqtt_id)) {
      Serial.println("Conectado ao MQTT!");
    } else {
      Serial.print("Erro MQTT: ");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  conectaWiFi();
  client.setServer(mqtt_server, mqtt_port);

  Wire.begin();

  if (!accel.begin()) {
    Serial.println("Erro ao iniciar o MMA8452!");
    while (1); 
  }

  accel.setScale(SCALE_2G);
  accel.setDataRate(ODR_800);

  pinMode(botaoTrava, INPUT_PULLUP);
  pinMode(ledTrava, OUTPUT);
  pinMode(ledDestrava, OUTPUT);
}

void loop() {
  if (!client.connected()) {
    mqttConnect();
  }
  client.loop();

  bool estadoAtualBotao = digitalRead(botaoTrava);
  if (ultimoEstadoBotao == HIGH && estadoAtualBotao == LOW) {
    estadoTrava = !estadoTrava;
    Serial.print("Novo estado da trava: ");
    Serial.println(estadoTrava ? "TRAVADO" : "DESTRAVADO");
    delay(300);
  }
  ultimoEstadoBotao = estadoAtualBotao;

  digitalWrite(ledTrava, estadoTrava ? HIGH : LOW);
  digitalWrite(ledDestrava, estadoTrava ? LOW : HIGH);

  if (millis() - ultimaLeitura > intervalo) {
    ultimaLeitura = millis();

    accel.read();
    float ax = accel.cx;
    float ay = accel.cy;
    float az = accel.cz;

    bool movimentoDetectado =
      (abs(ax - axAnterior) > limiteMovimento) ||
      (abs(ay - ayAnterior) > limiteMovimento) ||
      (abs(az - azAnterior) > limiteMovimento);

    axAnterior = ax;
    ayAnterior = ay;
    azAnterior = az;

    StaticJsonDocument<192> doc;
    doc["movimento"] = movimentoDetectado;
    doc["trava"] = estadoTrava ? "Travado" : "Destravado";

    if (estadoTrava && movimentoDetectado) {
      doc["alerta"] = "Veículo se movimentando com módulo travado !";
    }

    char mensagem[192];
    serializeJson(doc, mensagem);
    client.publish(TOPICO_MOVIMENTO, mensagem);
    Serial.println(mensagem);
  }

  delay(10);
}