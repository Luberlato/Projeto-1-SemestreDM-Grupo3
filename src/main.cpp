#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wifi.h>
#include "internet.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);

const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_id = "trackbox-central";
const char *mqtt_topic_sub = "trackbox/sensores";

LiquidCrystal_I2C lcd(0x27, 20, 4); // Criando o objeto

bool trava = false;
long lat;
long temp;
long longi;
long timesTemp;

void setup()
{
  Serial.begin(9600);

  // Iniciando o tratamento do LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("LAT: ");
  lcd.setCursor(0, 1);
  lcd.print("LONG: ");
  lcd.setCursor(0, 2);
  lcd.print("TEMP CARGA: ");
  lcd.setCursor(0, 3);
  lcd.print("TRAVA: ");
}

void loop()
{
  lcd.setCursor(6, 0);
  lcd.print(lat);
  if (trava)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" ALERTA!!!");
    lcd.setCursor(0, 1);
    lcd.print("Carga furtada");
    lcd.setCursor(0, 2);
    lcd.print("LAT: ");
    lcd.setCursor(0, 3);
    lcd.print("LONG: ");
  }
  else
  {
  }
}

void callback(char *topic, byte *payload, unsigned int Lenght)
{
   Serial.printf("mensagem recebida em: %s: ", topic);
  
 String mensagem = "";

 for (unsigned int i = 0; i < Lenght; i++)

  {
      char c = (char)payload[i];
    mensagem += c;
    
  }
   Serial.println(mensagem);

  //*******************JSON*************** */
  JsonDocument doc;
  deserializeJson(doc, mensagem);

  if (!doc["trava"].isNull())

  {
     trava = doc["trava"];
    
  }
if (!doc["timestamp"].isNull())

  {
    timesTemp = doc["timestamp"];
    
  }
if (!doc["latitude"].isNull())

  {
    lat = doc["Latitude"];
    
  }
  if (!doc["longitude"].isNull())

  {
     longi = doc["longitude"];
    
  }
  if (!doc["temperatura"].isNull())

 {
     temp = doc["temperatura"];
    
  }
}
