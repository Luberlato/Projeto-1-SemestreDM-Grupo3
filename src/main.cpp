#include <Arduino.h>
#include <DHT.h>



// Definindo o pino digital conectado ao sensor DHT22
#define DHTPIN 14    // Altere para o pino que você está usando

// Definindo o tipo de sensor DHT (DHT22)
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321



// Inicializando o sensor DHT
DHT dht(DHTPIN, DHTTYPE);

void setup() {
// Iniciando a comunicação serial
Serial.begin(9600);
Serial.println("Teste do Sensor DHT22!");

// Iniciando o sensor
dht.begin();

// Aguardando o sensor estabilizar
delay(2000);
}

void loop() {
// Aguarda 2 segundos entre as leituras (o DHT22 tem taxa de amostragem lenta)
delay(2000);

// Lendo a umidade
float umidade = dht.readHumidity();
// Lendo a temperatura em Celsius (padrão)
float temperatura = dht.readTemperature();



// Verificando se alguma leitura falhou
if (isnan(umidade) || isnan(temperatura)) {
Serial.println("Falha ao ler o sensor DHT22!");
return;
}





// Exibindo os dados no monitor serial
Serial.print("Umidade: ");
Serial.print(umidade);
Serial.print("%\t");
Serial.print("Temperatura: ");
Serial.print(temperatura);
Serial.print("°C ");
Serial.println("------------------");



// Você pode adicionar aqui outras funcionalidades como:
// - Acionar um relé se a temperatura passar de um limite
// - Enviar dados para um servidor
// - Mostrar em um display LCD
}