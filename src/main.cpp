#include <Arduino.h>
#include <HardwareSerial.h>

// Definição dos pinos UART para comunicação com o SIM808
#define SIM808_RX_PIN 16  // Conectar ao TX do SIM808
#define SIM808_TX_PIN 17  // Conectar ao RX do SIM808

// Criar uma instância de HardwareSerial para comunicação com o SIM808
HardwareSerial sim808(1);

// Variáveis de controle
unsigned long lastGPSRequest = 0;
unsigned long lastStatusCheck = 0;
bool gpsInitialized = false;
int satelliteCount = 0;

void setupGPS();
void requestGPSData();
void checkGPSStatus();
void sendATCommand(const char* command, int delayTime = 1000);
void processResponse(String response);
void processSINF(String sinfData);
void processNMEA(String nmea);
double convertToDecimalDegrees(String coordStr, String direction);
double convertSINFCoordinate(String coordStr, bool isLatitude);

void setup() {
  // Iniciar comunicação serial com o computador
  Serial.begin(115200);
  
  // Iniciar comunicação serial com o SIM808
  sim808.begin(9600, SERIAL_8N1, SIM808_RX_PIN, SIM808_TX_PIN);
  
  Serial.println("=== INICIANDO GPS SIM808 ===");
  
  delay(2000); // Aguardar estabilização
  
  // Configurar o módulo GPS
  setupGPS();
}

void loop() {
  // Verificar se há dados disponíveis do SIM808
  if (sim808.available()) {
    String response = sim808.readStringUntil('\n');
    response.trim();
    
    if (response.length() > 0) {
      processResponse(response);
    }
  }
  
  // A cada 3 segundos, solicitar dados GPS
  if (millis() - lastGPSRequest > 3000) {
    lastGPSRequest = millis();
    requestGPSData();
  }
  
  // A cada 15 segundos, verificar status do GPS
  if (millis() - lastStatusCheck > 15000) {
    lastStatusCheck = millis();
    checkGPSStatus();
  }
}

void setupGPS() {
  Serial.println("Configurando módulo GPS...");
  
  // Verificar se o módulo responde
  sendATCommand("AT", 500);
  
  // Ligar o GPS
  Serial.println("Ligando GPS...");
  sendATCommand("AT+CGPSPWR=1", 2000);
  
  // Configurar modo GPS standalone
  sendATCommand("AT+CGPSMODE=1", 1000);
  
  // Reset cold start
  sendATCommand("AT+CGPSRST=0", 2000);
  
  gpsInitialized = true;
  Serial.println("GPS inicializado. Solicitando dados...");
}

void requestGPSData() {
  // Solicitar informações GPS no formato SINF
  sim808.println("AT+CGPSINF=0");
  delay(100);
}

void checkGPSStatus() {
  Serial.println("\n--- STATUS GPS ---");
  sendATCommand("AT+CGPSSTATUS?", 1000);
  Serial.print("Satélites: ");
  Serial.println(satelliteCount);
  Serial.println("-----------------\n");
}

void sendATCommand(const char* command, int delayTime) {
  Serial.println("CMD: " + String(command));
  sim808.println(command);
  delay(delayTime);
  
  // Ler resposta
  unsigned long timeout = millis() + (delayTime + 500);
  while (millis() < timeout && sim808.available()) {
    String response = sim808.readStringUntil('\n');
    response.trim();
    if (response.length() > 0) {
      processResponse(response);
    }
  }
}

void processResponse(String response) {
  // Debug: mostrar respostas não-NMEA
  if (!response.startsWith("$G") && !response.startsWith("+CGPSINF")) {
    Serial.println("SIM808: " + response);
  }
  
  // Processar dados SINF (resposta do AT+CGPSINF=0)
  if (response.startsWith("+CGPSINF:") || response.startsWith("SINF:")) {
    processSINF(response);
  }
  
  // Processar dados NMEA tradicionais
  else if (response.startsWith("$GPRMC") || response.startsWith("$GPGGA") || 
           response.startsWith("$GPGSV") || response.startsWith("$GPGSA")) {
    processNMEA(response);
  }
}

void processSINF(String sinfData) {
  Serial.println("DADOS SINF: " + sinfData);
  
  // Formato SINF: 0,latitude,longitude,altitude,data_hora,fix,satelites,velocidade,curso
  // Exemplo: SINF: 0,2336.925300,4634.237600,760.600000,20250610122555.000,0,12,1.055640,24.139999
  
  // Remover prefixo se presente
  String data = sinfData;
  if (data.startsWith("+CGPSINF: ")) {
    data = data.substring(10);
  } else if (data.startsWith("SINF: ")) {
    data = data.substring(6);
  }
  
  // Separar os campos por vírgula
  String fields[10];
  int fieldIndex = 0;
  int startIndex = 0;
  
  for (int i = 0; i <= data.length() && fieldIndex < 10; i++) {
    if (i == data.length() || data.charAt(i) == ',') {
      fields[fieldIndex] = data.substring(startIndex, i);
      fieldIndex++;
      startIndex = i + 1;
    }
  }
  
  if (fieldIndex >= 7) {
    String status = fields[0];        // 0 = sem fix, outros = com fix
    String latStr = fields[1];        // Latitude DDMM.MMMMMM
    String lonStr = fields[2];        // Longitude DDDMM.MMMMMM
    String altStr = fields[3];        // Altitude
    String dateTime = fields[4];      // Data/hora
    String fixType = fields[5];       // Tipo de fix
    String satStr = fields[6];        // Número de satélites
    
    // Atualizar contagem de satélites
    satelliteCount = satStr.toInt();
    
    Serial.println("\n=== DADOS GPS PROCESSADOS ===");
    Serial.println("Status: " + status);
    Serial.println("Satélites: " + String(satelliteCount));
    Serial.println("Lat bruta: " + latStr);
    Serial.println("Lon bruta: " + lonStr);
    
    // Verificar se temos coordenadas válidas
    if (latStr.length() > 4 && lonStr.length() > 4 && 
        latStr.toDouble() != 0.0 && lonStr.toDouble() != 0.0) {
        
      // Converter coordenadas para formato decimal
      double latitude = convertSINFCoordinate(latStr, true);  // true = latitude
      double longitude = convertSINFCoordinate(lonStr, false); // false = longitude
      
      // CORREÇÃO IMPORTANTE: Para Brasil, coordenadas devem ser negativas
      // Latitude Sul (Brasil está no hemisfério Sul)
      if (latitude > 0 && latitude < 60) { // Assumindo Brasil/América do Sul
        latitude = -latitude;
      }
      
      // Longitude Oeste (Brasil está no hemisfério Oeste)
      if (longitude > 0 && longitude < 180) { // Assumindo Brasil/América
        longitude = -longitude;
      }
      
      Serial.println("\n*** COORDENADAS GPS ***");
      Serial.printf("Latitude:  %.6f° %s\n", abs(latitude), latitude >= 0 ? "N" : "S");
      Serial.printf("Longitude: %.6f° %s\n", abs(longitude), longitude >= 0 ? "E" : "W");
      
      if (altStr.length() > 0) {
        Serial.printf("Altitude: %.1f m\n", altStr.toDouble());
      }
      
      Serial.printf("Satélites: %d\n", satelliteCount);
      
      // Processar data/hora se disponível
      if (dateTime.length() >= 14) {
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
      
    } else {
      Serial.println("Aguardando fix GPS... (coordenadas ainda inválidas)");
      Serial.println("=============================\n");
    }
  } else {
    Serial.println("Dados SINF incompletos");
  }
}

double convertSINFCoordinate(String coordStr, bool isLatitude) {
  if (coordStr.length() == 0) return 0.0;
  
  double rawCoord = coordStr.toDouble();
  if (rawCoord == 0.0) return 0.0;
  
  // Para SINF, as coordenadas já vêm no formato correto DDMM.MMMMMM
  // Precisamos converter para graus decimais
  double degrees = floor(rawCoord / 100.0);
  double minutes = rawCoord - (degrees * 100.0);
  double decimalDegrees = degrees + (minutes / 60.0);
  
  // Para o formato SINF, coordenadas Sul e Oeste são negativas
  // Mas geralmente já vêm com o sinal correto
  return decimalDegrees;
}

void processNMEA(String nmea) {
  // Manter processamento NMEA para compatibilidade
  if (nmea.startsWith("$GPGSV")) {
    // Extrair número de satélites do NMEA se SINF não estiver disponível
    int firstComma = nmea.indexOf(',');
    int secondComma = nmea.indexOf(',', firstComma + 1);
    int thirdComma = nmea.indexOf(',', secondComma + 1);
    
    if (thirdComma > secondComma) {
      String satCount = nmea.substring(thirdComma + 1, nmea.indexOf(',', thirdComma + 1));
      int newSatCount = satCount.toInt();
      if (newSatCount > 0 && satelliteCount == 0) {
        satelliteCount = newSatCount;
        Serial.println("Satélites (NMEA): " + String(satelliteCount));
      }
    }
  }
}

double convertToDecimalDegrees(String coordStr, String direction) {
  // Função mantida para compatibilidade com NMEA
  if (coordStr.length() == 0) return 0.0;
  
  double rawCoord = coordStr.toDouble();
  if (rawCoord == 0.0) return 0.0;
  
  double degrees = floor(rawCoord / 100.0);
  double minutes = rawCoord - (degrees * 100.0);
  double decimalDegrees = degrees + (minutes / 60.0);
  
  if (direction == "S" || direction == "W") {
    decimalDegrees = -decimalDegrees;
  }
  
  return decimalDegrees;
}