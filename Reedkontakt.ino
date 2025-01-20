#pragma region "Generics"

#include <LittleFS.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h> 
#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <HT_SSD1306Wire.h>
#include <ESP32MQTTClient.h>
#include <SimpleDHT.h>

#define HOSTNAME "E32-Reed"
#define ConfigFile "/config.json"
#define VBAT_Read    1
#define	ADC_Ctrl    37
#define	Pin1    2
#define	Pin2    3
#define	Pin3    4
#define	Pin4    5
#define	Pin5    6
#define	Pin6    7
#define	Pin7    41
#define	Pin8    42

#define	ResetPin 42

static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);
ESP32MQTTClient mqttClient; 

char mqtt_server[40];
char mqtt_username[13] = "";
char mqtt_password[32] = "";
bool bSaveConfig = false;

char *PubMQTT_Reed = "Reedkontakt";

char *PubMQTT_Voltage = "Spannung";
char *SubMQTT_Res = "Res";
String StatusBarText;



void setupOTA()
{ 
   ArduinoOTA.setHostname(HOSTNAME);
   ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]()
  {
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "Starte Update....");
    Serial.printf("Start Updating....Type:%s\n", (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem");
  });

  ArduinoOTA.onEnd([]()
  {
    display.clear();
    display.drawString(0, 0, "Update fertig!");
    Serial.println("Update fertig!");

    ESP.restart();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
    String pro = String(progress / (total / 100)) + "%";
    int progressbar = (progress / (total / 100));
   
    display.clear();
    display.drawProgressBar(0, 32, 120, 10, progressbar); 
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 15, pro);
    display.display();

    Serial.printf("Fortschritt: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error)
  {
    Serial.printf("Fehler[%u]: ", error);
    String info = "Fehler Info:";
    switch(error)
    {
      case OTA_AUTH_ERROR:
        info += "Auth Failed";
        Serial.println("Auth Failed");
        break;

      case OTA_BEGIN_ERROR:
        info += "Begin Failed";
        Serial.println("Begin Failed");
        break;

      case OTA_CONNECT_ERROR:
        info += "Connect Failed";
        Serial.println("Connect Failed");
        break;

      case OTA_RECEIVE_ERROR:
        info += "Receive Failed";
        Serial.println("Receive Failed");
        break;

      case OTA_END_ERROR:
        info += "End Failed";
        Serial.println("End Failed");
        break;
    }

    display.clear();
    display.drawString(0, 0, info);
    ESP.restart();
  });

  ArduinoOTA.begin();
}

void VextON(void)
{
  digitalWrite(Vext, LOW);
}

void VextOFF(void)
{
  digitalWrite(Vext, HIGH);
}

String readVextVoltage() 
{  
  const int resolution = 12;
  const int adcMax = pow(2,resolution) - 1;
  const float adcMaxVoltage = 3.3;

  // Spannungsteiler
  const int R1 = 390;
  const int R2 = 100;
 
  const float measuredVoltage = 4.2;
  const float reportedVoltage = 4.095;
  const float factor = (adcMaxVoltage / adcMax) * ((R1 + R2)/(float)R2) * (measuredVoltage / reportedVoltage); 

  digitalWrite(ADC_Ctrl,LOW);
  delay(100);

  int analogValue = analogRead(VBAT_Read);
  digitalWrite(ADC_Ctrl,HIGH);

  float floatVoltage = factor * analogValue;
  return String(floatVoltage);

}


void WiFiEvent(WiFiEvent_t event)
{
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch (event) {
        case ARDUINO_EVENT_WIFI_READY: 
            StatusBarText = "WiFi interface ready";
            break;
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
           StatusBarText = "Completed scan for access points";
            break;
        case ARDUINO_EVENT_WIFI_STA_START:
            StatusBarText = "WiFi client started";
            break;
        case ARDUINO_EVENT_WIFI_STA_STOP:
            StatusBarText = "WiFi clients stopped";
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
          StatusBarText = "Connected to access point";
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            StatusBarText = "Disconnected from WiFi access point";
            break;
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
            StatusBarText = "Authentication mode of access point has changed";
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            StatusBarText = "Obtained IP address...";
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            StatusBarText = "Lost IP address and IP address is reset to 0";
            break;
        case ARDUINO_EVENT_WPS_ER_SUCCESS:
            StatusBarText = "WiFi Protected Setup (WPS): succeeded in enrollee mode";
            break;
        case ARDUINO_EVENT_WPS_ER_FAILED:
            StatusBarText = "WiFi Protected Setup (WPS): failed in enrollee mode";
            break;
        case ARDUINO_EVENT_WPS_ER_TIMEOUT:
            StatusBarText = "WiFi Protected Setup (WPS): timeout in enrollee mode";
            break;
        case ARDUINO_EVENT_WPS_ER_PIN:
           StatusBarText = "WiFi Protected Setup (WPS): pin code in enrollee mode";
            break;
        case ARDUINO_EVENT_WIFI_AP_START:
            StatusBarText = "WiFi access point started";
            break;
        case ARDUINO_EVENT_WIFI_AP_STOP:
            StatusBarText = "WiFi access point stopped";
            break;
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            StatusBarText = "Client connected";
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            StatusBarText = "Client disconnected";
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
           StatusBarText = "Assigned IP address to client";
            break;
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
           StatusBarText = "Received probe request";
            break;
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
            StatusBarText = "AP IPv6 is preferred";
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            StatusBarText = "STA IPv6 is preferred";
            break;
        case ARDUINO_EVENT_ETH_GOT_IP6:
           StatusBarText = "Ethernet IPv6 is preferred";
            break;
        case ARDUINO_EVENT_ETH_START:
          StatusBarText = "Ethernet started";
            break;
        case ARDUINO_EVENT_ETH_STOP:
            StatusBarText = "Ethernet stopped";
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
           StatusBarText = "Ethernet connected";
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            StatusBarText = "Ethernet disconnected";
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            StatusBarText = "Obtained IP address";
            break;
        default: break;
    }

    Serial.println(StatusBarText);
}


void saveConfigCallback () 
{
  Serial.println("saveConfigCallback()");
  bSaveConfig = true;
}


void setupWIFI() 
{

  Serial.println("Starte Dateisystem...");

  if (!LittleFS.begin()) {
    Serial.println("Fehler im Dateisystem.");
    LittleFS.format();
  }   
  
    if (LittleFS.exists(ConfigFile)) 
    { 
      Serial.println("Lese Config-Datei");
      File configFile = LittleFS.open(ConfigFile, FILE_READ);
                 
      if (!configFile) {
        Serial.println("Datei kann nicht gelesen werden.");
        return;
      }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, configFile);
        configFile.close();

       if (error) 
       {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.c_str());

        } else {
                           
          int server_len =  doc["mqtt_server"].as<String>().length() + 1;
          doc["mqtt_server"].as<String>().toCharArray(mqtt_server, server_len);

          int username_len =  doc["mqtt_username"].as<String>().length() + 1;
          doc["mqtt_username"].as<String>().toCharArray(mqtt_username, username_len);

          int password_len =  doc["mqtt_password"].as<String>().length() + 1;
          doc["mqtt_password"].as<String>().toCharArray(mqtt_password, password_len);
                 
        }     
    }
  
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(WiFiEvent);  
 
  WiFiManager wifiManager;

  wifiManager.setDebugOutput(false);
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server-IP", "", 40);
  WiFiManagerParameter custom_mqtt_username("mqtt_username", "MQTT Benutzername", "", 13);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT Password", "", 32);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_username);
  wifiManager.addParameter(&custom_mqtt_password);
    
   if(digitalRead(ResetPin))
   {  
      LittleFS.format();
      wifiManager.resetSettings();

      display.clear();
      display.setFont(ArialMT_Plain_16);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.drawString(64, 24, "Reset WiFi Config");
      display.display();
      delay(2000); 
   }

  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 16, "Hostname:");
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 32, HOSTNAME);
  display.display();
  
  wifiManager.autoConnect(HOSTNAME);

  if (bSaveConfig) {

      Serial.println("Config-Datei gespeichert");
  
      strcpy(mqtt_server, custom_mqtt_server.getValue());
      strcpy(mqtt_username, custom_mqtt_username.getValue());
      strcpy(mqtt_password, custom_mqtt_password.getValue());
    
      JsonDocument doc;
      doc["mqtt_server"] = mqtt_server;
      doc["mqtt_username"] = mqtt_username;
      doc["mqtt_password"] = mqtt_password;
    
      File configFile = LittleFS.open(ConfigFile, FILE_WRITE);

      if (!configFile) 
      {
          Serial.println("Fehler beim Speichern");
      } else {
          serializeJson(doc, configFile);
          configFile.close();
          serializeJsonPretty(doc, Serial);
      }   
    }
  }

void setupMQTT()
{
  mqttClient.enableDebuggingMessages(false);
  mqttClient.setURI(mqtt_server, mqtt_username, mqtt_password);
  mqttClient.enableLastWillMessage(HOSTNAME, "Offline");
  mqttClient.setKeepAlive(30);
  mqttClient.loopStart();
}

void InitSetup()
{
  
  pinMode(ResetPin, INPUT);
  pinMode(VBAT_Read, INPUT);
  pinMode(Vext, OUTPUT);
  pinMode(ADC_Ctrl, OUTPUT);

  VextON();

  display.init();
  display.screenRotate(ANGLE_180_DEGREE);
  display.clear();
  display.display();
  
  display.setContrast(255);
  Serial.begin(115200);
 
  Serial.println("Laden...");   
 
  while (!Serial) {
    ; // Warte, bis serielle Schnittstelle bereit.
  }

  setupWIFI();
  setupOTA();
  setupMQTT();
 
}


void onMqttConnect(esp_mqtt_client_handle_t client)
{    
    if (mqttClient.isMyTurn(client))
    {
        mqttClient.publish(HOSTNAME, "", 0, true);
        mqttClient.subscribe(SubMQTT_Res, [](const String &payload)
                             { log_i("%s: %s", subscribeTopic, payload.c_str()); });

    }
}

void handleMQTT(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){
  auto *event = static_cast<esp_mqtt_event_handle_t>(event_data);
  mqttClient.onEventCallback(event);
}

void Publish2MQTT(String publishTopic, String value, int qos = 0, bool retain = false)
{
  if(mqttClient.isConnected())
  {
    char *topic;
    String strTopic = String(String(HOSTNAME) + String("/") + String(publishTopic));
    mqttClient.publish(strTopic, value, qos, retain);
   
  }
}

void loop()
{
  ArduinoOTA.handle();

  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  String Batterie = readVextVoltage(); 
  Publish2MQTT(PubMQTT_Voltage, Batterie);

  if(StatusBarText == "")
  {
    display.drawString(0, 0, HOSTNAME);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(128, 0, "V: " + Batterie);
  } else {
    display.drawString(0, 0, StatusBarText);
    StatusBarText = "";
  }
 
  Sensor();
  display.display(); 
  
  delay(500); 
}

#pragma endregion
  
void setup()
{
  InitSetup();
  pinMode(Pin1, INPUT);
}

void Sensor()
{
    String Reed1;
 
  if (digitalRead(Pin1) == LOW)
  {  
      Reed1 = "Frei";
  } else {
      Reed1 = "Belegt";
  }

  
  Publish2MQTT(PubMQTT_Reed, Reed1);  
 
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 16, "Reedkont.");
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(128, 16, Reed1);

 
}

