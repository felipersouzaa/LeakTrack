#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include "api.h"  // Certifique-se de que api.h contém as definições de FIREBASE_HOST, FIREBASE_AUTH, WIFI_SSID e WIFI_PASSWORD
#include "time.h"

#define INPUT_PIN_1 4

#define INPUT_PIN_2 5

FirebaseData firebaseData;
FirebaseJson json;

typedef struct {
    String nomeDoSensor;
    String posicaoDoSensor;
    float litrosPorMin = 0;
    float qtdAguaTotal = 0;
    String date = "";
} WaterFlowSensorData;

double flow1 = 0; // Litros de volume de água que passa
double flow2 = 0; // Litros de volume de água que passa
volatile unsigned long pulseFreq1 = 0; // Volátil para uso em ISR
volatile unsigned long pulseFreq2 = 0; // Volátil para uso em ISR
unsigned long lastTime = 0;
unsigned long currentTime = 0;
// Endereço do servidor NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800; // Ajuste para seu fuso horário
const int daylightOffset_sec = 0; // Ajuste para horário de verão, se aplicável

void pulse1();
void pulse2();
void wifiConnect();
void sendDataToFirebase(WaterFlowSensorData data);
void postDataToFirebase(WaterFlowSensorData data);
void firebaseInit();

WaterFlowSensorData sensor1;
WaterFlowSensorData sensor2;

FirebaseConfig config;
FirebaseAuth auth;

void setup() {
    Serial.begin(9600);
    Serial.println("ESP Ligado");

    pinMode(INPUT_PIN_1, INPUT);
    pinMode(INPUT_PIN_2, INPUT);

    attachInterrupt(digitalPinToInterrupt(INPUT_PIN_1), pulse1, RISING);
    attachInterrupt(digitalPinToInterrupt(INPUT_PIN_2), pulse2, RISING);

    wifiConnect();
    firebaseInit();

    lastTime = millis();

    // Inicialize a sincronização de tempo
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Falha ao obter a hora");
        return;
    }
    char timeString[64];
    strftime(timeString, sizeof(timeString), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    
    flow1 = ((1000.0 / (millis() - lastTime)) * pulseFreq1); // Cálculo do fluxo com base na especificação do sensor
    flow2 = ((1000.0 / (millis() - lastTime)) * pulseFreq2);
    lastTime = millis();
    Serial.print("Flow 1: ");
    Serial.print(flow1, DEC);
    Serial.println(" L");

    Serial.print("Flow 2: ");
    Serial.print(flow2, DEC);
    Serial.println(" L");
    Serial.println(" ------------------    -----------------       -----------------");

    sensor1.date = timeString;
    sensor1.litrosPorMin = (flow1 / 60);
    sensor1.nomeDoSensor = "Sensor1";
    sensor1.posicaoDoSensor = "1 andar";
    sensor1.qtdAguaTotal = flow1;

    sensor2.date = timeString;
    sensor2.litrosPorMin = (flow2 / 60);
    sensor2.nomeDoSensor = "Sensor2";
    sensor2.posicaoDoSensor = "2 andar";
    sensor2.qtdAguaTotal = flow2;
    
    sendDataToFirebase(sensor1);
    postDataToFirebase(sensor1);
    sendDataToFirebase(sensor2);
    postDataToFirebase(sensor2);

    // Resetar a frequência de pulsos para a próxima medição
    pulseFreq1 = 0;
    pulseFreq2 = 0;
    delay(100);
}

void pulse1() {
    pulseFreq1++;
}

void pulse2() {
    pulseFreq2++;
}

void wifiConnect() {
    pinMode(2, OUTPUT);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        digitalWrite(2, HIGH);
        delay(200);
        digitalWrite(2, LOW);
        delay(200);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
}

void firebaseInit() {
    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    Firebase.setReadTimeout(firebaseData, 1000 * 60);
    Firebase.setwriteSizeLimit(firebaseData, "tiny");
    Serial.println("--------------------------------");
    Serial.println("Connected...");

    if (!Firebase.beginStream(firebaseData, "restart")) {
        Serial.println(firebaseData.errorReason());
    }
}

void sendDataToFirebase(WaterFlowSensorData data) {
    String path = "/lastData/";
    path += data.nomeDoSensor;
    path += "/";
    json.clear();
    json.add("NomeDoSensor", data.nomeDoSensor);
    json.add("date", data.date);
    json.add("PosicaoDoSensor", data.posicaoDoSensor);
    json.add("litrosPorMin", data.litrosPorMin);
    json.add("qtdAguaTotal", data.qtdAguaTotal);

    Serial.println(json.raw());

    if (Firebase.setJSON(firebaseData, path, json)) {
        Serial.println("Data sent successfully");
    } else {
        Serial.println("Failed to send data");
        Serial.println(firebaseData.errorReason());
    }
}

void postDataToFirebase(WaterFlowSensorData data) {
    String path = "/sensorData/";
    path += data.nomeDoSensor;
    path += "/";
    json.clear();
    json.add("NomeDoSensor", data.nomeDoSensor);
    json.add("date", data.date);
    json.add("PosicaoDoSensor", data.posicaoDoSensor);
    json.add("litrosPorMin", data.litrosPorMin);
    json.add("qtdAguaTotal", data.qtdAguaTotal);

    Serial.println(json.raw());

    if (Firebase.pushJSON(firebaseData, path, json)) {
        Serial.println("Data sent successfully");
    } else {
        Serial.println("Failed to send data");
        Serial.println(firebaseData.errorReason());
    }
}
