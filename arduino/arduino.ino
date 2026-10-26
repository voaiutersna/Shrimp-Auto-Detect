//   TDS     : VCC -> 5V, GND -> GND, A (signal) -> A1
//   DS18B20 : VCC -> 5V, GND -> GND, DATA -> D2 (+ 4.7k resistor between DATA and 5V)

#include <WiFiS3.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//  wifi name and password
const char* WIFI_SSID     = "Galaxy A54 5G 5056";
const char* WIFI_PASSWORD = "pokezaja";

// network adderss look with ipconfig in terminal to send message bewtween same wifi
const char* SERVER_HOST = "172.31.5.214";
const int   SERVER_PORT = 3000;
const bool  USE_HTTPS   = false;
const char* SERVER_PATH = "/api/readings";
const char* API_KEY     = "change-me-123"; 
const unsigned long SEND_INTERVAL = 1000;   


#define TdsSensorPin A1
#define ONE_WIRE_BUS 2
#define VREF 5.0   
#define SCOUNT 30

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

WiFiClient    plainClient;
WiFiSSLClient sslClient;

int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
float averageVoltage = 0, tdsValue = 0, temperature = 25;  // 25 = default/fallback

int getMedianNum(int bArray[], int iFilterLen);
void connectWiFi();
void sendData();

void setup()
{
  Serial.begin(9600);
  delay(1500);
  pinMode(TdsSensorPin, INPUT);
  sensors.begin();
  sensors.setWaitForConversion(false);  
  sensors.requestTemperatures();        

  for (int i = 0; i < SCOUNT; i++) analogBuffer[i] = 0;

  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("WiFi module not found!");
    while (true);
  }
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION)
    Serial.println("Please update the WiFi firmware (Tools > Firmware Updater)");

  connectWiFi();
}

void loop()
{
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U)
  {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);
    analogBufferIndex = (analogBufferIndex + 1) % SCOUNT;
  }

  static unsigned long tempTimepoint = millis();
  if (millis() - tempTimepoint > 1000U)
  {
    tempTimepoint = millis();
    float t = sensors.getTempCByIndex(0);   
    if (t != DEVICE_DISCONNECTED_C)       
      temperature = t;
    sensors.requestTemperatures();          
  }

  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U)
  {
    printTimepoint = millis();
    for (int i = 0; i < SCOUNT; i++)
      analogBufferTemp[i] = analogBuffer[i];

    averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 1024.0;
    float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
    float v = averageVoltage / compensationCoefficient;
    tdsValue = (133.42 * v * v * v - 255.86 * v * v + 857.39 * v) * 0.5;

    Serial.print("Temp: ");
    Serial.print(temperature, 1);
    Serial.print(" C  |  TDS: ");
    Serial.print(tdsValue, 0);
    Serial.println(" ppm");
  }

  // 4) Send to the website
  static unsigned long sendTimepoint = 0;
  if (millis() - sendTimepoint > SEND_INTERVAL)
  {
    sendTimepoint = millis();
    sendData();
  }
}

void connectWiFi()
{
  Serial.print("Connecting WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    start = millis();
    while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - start < 5000)
      delay(100);
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("WiFi failed (will retry next time)");
  }
}

void sendData()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return;
  }

  Client& client = USE_HTTPS ? (Client&)sslClient : (Client&)plainClient;

  if (!client.connect(SERVER_HOST, SERVER_PORT))
  {
    Serial.println("Could not connect to server");
    return;
  }

  String body = String("{\"temperature\":") + String(temperature, 2) +
                ",\"tds\":" + String(tdsValue, 1) + "}";

  client.print(String("POST ") + SERVER_PATH + " HTTP/1.1\r\n");
  client.print(String("Host: ") + SERVER_HOST + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print(String("x-api-key: ") + API_KEY + "\r\n");
  client.print(String("Content-Length: ") + body.length() + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  unsigned long start = millis();
  while (!client.available() && client.connected() && millis() - start < 5000)
    delay(10);
  String status = client.readStringUntil('\n');
  Serial.print("Server: ");
  if (status.length() > 0)
    Serial.println(status);
  else
    Serial.println("(no response)");

  client.stop();
}

int getMedianNum(int bArray[], int iFilterLen)
{
  int bTab[iFilterLen];
  for (int i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  for (int j = 0; j < iFilterLen - 1; j++)
    for (int i = 0; i < iFilterLen - j - 1; i++)
      if (bTab[i] > bTab[i + 1])
      {
        int t = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = t;
      }
  if (iFilterLen & 1)
    return bTab[(iFilterLen - 1) / 2];
  return (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
}