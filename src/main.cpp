#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_MMA8452Q.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include "internet.h"

// VARIAVEIS DHT22//

#define DHTPIN 4

#define DHTTYPE DHT22

// VARIAVEIS GPS //

#define SIM808_RX_PIN 16 // Conectar ao TX do SIM808
#define SIM808_TX_PIN 17 // Conectar ao RX do SIM808

// VARIAVEIS ACELEROMETRO//

const int botaoTrava = 19;
const int ledTrava = 5;
const int ledDestrava = 18;

bool estadoAlerta = false;
bool estadoTrava = false; // false = destravado, true = travado
bool ultimoEstadoBotao = HIGH;
float axAnterior = 0, ayAnterior = 0, azAnterior = 0;
const float limiteMovimento = 0.2; // Sensibilidade (menor = mais sensível)

// VARIAVEIS GPS//

unsigned long lastGPSRequest = 0;
unsigned long lastStatusCheck = 0;
bool gpsInitialized = false;
int satelliteCount = 0;
double ultimaLatitude = 0.0;
double ultimaLongitude = 0.0;

MMA8452Q accel;
WiFiClient espClient;
PubSubClient client(espClient);
HardwareSerial sim808(1);
DHT dht(DHTPIN, DHTTYPE);

const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_id = "trackbox-completo";
const char *TOPICO_SENSORES = "trackbox/sensores";

unsigned long ultimaLeitura = 0;
unsigned long intervalo = 2000;

void mqttConnect();
void setupGPS();
void requestGPSData();
void checkGPSStatus();
void verificarMovimento();
void sendATCommand(const char *command, int delayTime = 1000);
void processResponse(String response);
void processSINF(String sinfData);
void processNMEA(String nmea);
double convertToDecimalDegrees(String coordStr, String direction);
double convertSINFCoordinate(String coordStr, bool isLatitude);

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic);

  // Imprime o payload recebido (opcional, para debug)
  Serial.print("Payload: ");
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Desserializa diretamente do payload
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error)
  {
    Serial.print("Erro ao fazer parse do JSON: ");
    Serial.println(error.c_str());
    return;
  }

  if (!doc["alerta2"].isNull() && doc["alerta2"].as<bool>() == false)
  {
    Serial.println("Comando recebido: alerta2 = false. Desligando alerta local.");
    estadoAlerta = false;
  }
}

void setup()
{
  Serial.begin(115200);
  conectaWiFi();
  setupGPS();
  client.setCallback(mqttCallback);
  client.setServer(mqtt_server, mqtt_port);
  sim808.begin(9600, SERIAL_8N1, SIM808_RX_PIN, SIM808_TX_PIN);
  Wire.begin();
  dht.begin();

  if (!accel.begin())
  {
    Serial.println("Erro ao iniciar o MMA8452!");
    while (1)
      ;
  }

  accel.setScale(SCALE_2G);
  accel.setDataRate(ODR_800);

  pinMode(botaoTrava, INPUT_PULLUP);
  pinMode(ledTrava, OUTPUT);
  pinMode(ledDestrava, OUTPUT);
}

void loop()
{
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();

  if (!client.connected())
  {
    mqttConnect();
    client.subscribe(TOPICO_SENSORES);
  }
  client.loop();

  if (sim808.available())
  {
    String response = sim808.readStringUntil('\n');
    response.trim();

    if (response.length() > 0)
    {
      processResponse(response);
    }
  }

  if (millis() - lastGPSRequest > 3000)
  {
    lastGPSRequest = millis();
    requestGPSData();
  }

  if (millis() - lastStatusCheck > 15000)
  {
    lastStatusCheck = millis();
    checkGPSStatus();
  }

  bool estadoAtualBotao = digitalRead(botaoTrava);
  if (ultimoEstadoBotao == HIGH && estadoAtualBotao == LOW)
  {
    estadoTrava = !estadoTrava;
    Serial.print("Novo estado da trava: ");
    Serial.println(estadoTrava ? "TRAVADO" : "DESTRAVADO");
    delay(300);
  }
  ultimoEstadoBotao = estadoAtualBotao;

  digitalWrite(ledTrava, estadoTrava ? HIGH : LOW);
  digitalWrite(ledDestrava, estadoTrava ? LOW : HIGH);

  if (millis() - ultimaLeitura > intervalo)
  {
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

    if (estadoTrava && movimentoDetectado)
    {
      estadoAlerta = true;
    }

    StaticJsonDocument<192> doc;
    doc["latitude"] = ultimaLatitude;
    doc["longitude"] = ultimaLongitude;
    doc["movimento"] = movimentoDetectado;
    doc["trava"] = estadoTrava;
    doc["temperatura"] = temperatura;
    doc["alerta"] = estadoAlerta;

    char mensagem[192];
    serializeJson(doc, mensagem);
    client.publish(TOPICO_SENSORES, mensagem);
    Serial.println(mensagem);
  }

  delay(10);
}

void mqttConnect()
{
  while (!client.connected())
  {
    Serial.println("Conectando ao MQTT...");
    if (client.connect(mqtt_id))
    {
      Serial.println("Conectado ao MQTT!");
    }
    else
    {
      Serial.print("Erro MQTT: ");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

void setupGPS()
{
  Serial.println("Configurando módulo GPS...");

  sendATCommand("AT", 500);

  Serial.println("Ligando GPS...");
  sendATCommand("AT+CGPSPWR=1", 2000);

  sendATCommand("AT+CGPSMODE=1", 1000);

  sendATCommand("AT+CGPSRST=0", 2000);

  gpsInitialized = true;
  Serial.println("GPS inicializado. Solicitando dados...");
}

void requestGPSData()
{
  sim808.println("AT+CGPSINF=0");
  delay(100);
}

void checkGPSStatus()
{
  Serial.println("\n--- STATUS GPS ---");
  sendATCommand("AT+CGPSSTATUS?", 1000);
  Serial.print("Satélites: ");
  Serial.println(satelliteCount);
  Serial.println("-----------------\n");
}

void sendATCommand(const char *command, int delayTime)
{
  Serial.println("CMD: " + String(command));
  sim808.println(command);
  delay(delayTime);

  unsigned long timeout = millis() + (delayTime + 500);
  while (millis() < timeout && sim808.available())
  {
    String response = sim808.readStringUntil('\n');
    response.trim();
    if (response.length() > 0)
    {
      processResponse(response);
    }
  }
}

void processResponse(String response)
{
  if (!response.startsWith("$G") && !response.startsWith("+CGPSINF"))
  {
    Serial.println("SIM808: " + response);
  }

  if (response.startsWith("+CGPSINF:") || response.startsWith("SINF:"))
  {
    processSINF(response);
  }

  else if (response.startsWith("$GPRMC") || response.startsWith("$GPGGA") ||
           response.startsWith("$GPGSV") || response.startsWith("$GPGSA"))
  {
    processNMEA(response);
  }
}

void processSINF(String sinfData)
{
  Serial.println("DADOS SINF: " + sinfData);

  String data = sinfData;
  if (data.startsWith("+CGPSINF: "))
  {
    data = data.substring(10);
  }
  else if (data.startsWith("SINF: "))
  {
    data = data.substring(6);
  }

  String fields[10];
  int fieldIndex = 0;
  int startIndex = 0;

  for (int i = 0; i <= data.length() && fieldIndex < 10; i++)
  {
    if (i == data.length() || data.charAt(i) == ',')
    {
      fields[fieldIndex] = data.substring(startIndex, i);
      fieldIndex++;
      startIndex = i + 1;
    }
  }

  if (fieldIndex >= 7)
  {
    String status = fields[0];
    String latStr = fields[1];
    String lonStr = fields[2];
    String altStr = fields[3];
    String dateTime = fields[4];
    String fixType = fields[5];
    String satStr = fields[6];

    satelliteCount = satStr.toInt();

    Serial.println("\n=== DADOS GPS PROCESSADOS ===");
    Serial.println("Status: " + status);
    Serial.println("Satélites: " + String(satelliteCount));
    Serial.println("Lat bruta: " + latStr);
    Serial.println("Lon bruta: " + lonStr);

    if (latStr.length() > 4 && lonStr.length() > 4 &&
        latStr.toDouble() != 0.0 && lonStr.toDouble() != 0.0)
    {

      ultimaLatitude = convertSINFCoordinate(latStr, true);
      ultimaLongitude = convertSINFCoordinate(lonStr, false);

      if (ultimaLatitude > 0 && ultimaLatitude < 60)
      {
        ultimaLatitude = -ultimaLatitude;
      }

      if (ultimaLongitude > 0 && ultimaLongitude < 180)
      {
        ultimaLongitude = -ultimaLongitude;
      }

      Serial.println("\n*** COORDENADAS GPS ***");
      Serial.printf("Latitude:  %.6f° %s\n", abs(ultimaLatitude), ultimaLatitude >= 0 ? "N" : "S");
      Serial.printf("Longitude: %.6f° %s\n", abs(ultimaLongitude), ultimaLongitude >= 0 ? "E" : "W");

      if (altStr.length() > 0)
      {
        Serial.printf("Altitude: %.1f m\n", altStr.toDouble());
      }

      Serial.printf("Satélites: %d\n", satelliteCount);

      if (dateTime.length() >= 14)
      {
        String date = dateTime.substring(0, 8);
        String time = dateTime.substring(8, 14);
        Serial.printf("Data: %s/%s/%s\n",
                      date.substring(6, 8).c_str(),
                      date.substring(4, 6).c_str(),
                      date.substring(0, 4).c_str());
        Serial.printf("Hora: %s:%s:%s UTC\n",
                      time.substring(0, 2).c_str(),
                      time.substring(2, 4).c_str(),
                      time.substring(4, 6).c_str());
      }
      Serial.println("**********************\n");
    }
    else
    {
      Serial.println("Aguardando fix GPS... (coordenadas ainda inválidas)");
      Serial.println("=============================\n");
    }
  }
  else
  {
    Serial.println("Dados SINF incompletos");
  }
}

double convertSINFCoordinate(String coordStr, bool isLatitude)
{
  if (coordStr.length() == 0)
    return 0.0;

  double rawCoord = coordStr.toDouble();
  if (rawCoord == 0.0)
    return 0.0;

  double degrees = floor(rawCoord / 100.0);
  double minutes = rawCoord - (degrees * 100.0);
  double decimalDegrees = degrees + (minutes / 60.0);

  return decimalDegrees;
}

void processNMEA(String nmea)
{

  if (nmea.startsWith("$GPGSV"))
  {
    int firstComma = nmea.indexOf(',');
    int secondComma = nmea.indexOf(',', firstComma + 1);
    int thirdComma = nmea.indexOf(',', secondComma + 1);

    if (thirdComma > secondComma)
    {
      String satCount = nmea.substring(thirdComma + 1, nmea.indexOf(',', thirdComma + 1));
      int newSatCount = satCount.toInt();
      if (newSatCount > 0 && satelliteCount == 0)
      {
        satelliteCount = newSatCount;
        Serial.println("Satélites (NMEA): " + String(satelliteCount));
      }
    }
  }
}

double convertToDecimalDegrees(String coordStr, String direction)
{

  if (coordStr.length() == 0)
    return 0.0;

  double rawCoord = coordStr.toDouble();
  if (rawCoord == 0.0)
    return 0.0;

  double degrees = floor(rawCoord / 100.0);
  double minutes = rawCoord - (degrees * 100.0);
  double decimalDegrees = degrees + (minutes / 60.0);

  if (direction == "S" || direction == "W")
  {
    decimalDegrees = -decimalDegrees;
  }

  return decimalDegrees;
}
