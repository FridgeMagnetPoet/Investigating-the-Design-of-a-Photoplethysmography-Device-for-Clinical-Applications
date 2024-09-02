/* This code serves as the ESP32 code used during the preliminary performance evaluation of the project:
 * Investigating the Design of a  Photoplethysmography Device for Clinical Applications
 * By A. Aspeling, 2024
 */

#include <Arduino.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESP32_FTPClient.h>
#include <Adafruit_ADS1X15.h>
#include <cstring>
#include <EEPROM.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>

// SETTINGS
const int PRINT_TO_SERIAL = true;
const int SEND_TO_FTP = true;
const int ADC_INDX = 500; // calculated as (time in milliseconds)/18 and rounded
const int TIM_INDX = 50;

char FTP_server[] = "192.168.58.80";

//char device_name[] = "VEMD_RED";
//char device_name[] = "VEMD_GREEN";
//char device_name[] = "ONSM_RED";
char device_name[] = "ONSM_GREEN";

// Init Investigation
int LED_num = 0;
int LED_dutyCycle = 0;

// Define Pins
#define RG_PIN_1 22
#define RG_PIN_2 25
#define IR_PIN_1 19
#define IR_PIN_2 26
#define ADS_ALT 5
#define I2C_SDA 4
#define I2C_SCL 33

// Init PWM Properties
const int LED_freq = 5000;
const int LED_res = 8;

// Init ADS1115 ADC Properties
Adafruit_ADS1115 ads;
String ADC_dataString_1 = "";
String ADC_dataString_2 = "";
String ADC_timeString = "";
int16_t ADC_data[ADC_INDX];
unsigned long ADC_time[TIM_INDX];
int total_indx = 0;

// Init WiFi Properties
const char* WiFi_SSID = "Watermelon";
const char* WiFi_PASS = "ekgaanjouniesenie";

// Init FTP Properties
char FTP_user[] = "esp32";
char FTP_pass[] = "esp32";
uint16_t FTP_port = 2221;
ESP32_FTPClient ftp(FTP_server, FTP_port, FTP_user, FTP_pass, 5000, 2);
unsigned int file_nameCounter;

// Init Webserver Properties
String buffer_dataString = "";
bool buffer_clearFlag = false;

AsyncWebServer server(80);

String sendToWebServer() 
{
  buffer_clearFlag = true;
  return buffer_dataString;
}

void setup()
{
  //---------------------// Serial Setup //---------------------//
  Serial.begin(115200);
  delay(500);

  Serial.println("*****************************************");
  Serial.println("*** ESP32 is Ready ***\n");

  //---------------------// WiFi Setup //---------------------//
  
  WiFi.mode(WIFI_MODE_STA);             // Put ESP32 in Station Mode
  WiFi.begin(WiFi_SSID, WiFi_PASS);
  Serial.print("Connecting to WiFi");

  while(WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }

  Serial.println("\nConnected to the WiFi Network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
  
  //---------------------// Setup ADS1115 ADC //---------------------//
  ads.setGain(GAIN_ONE);                // Change Gain of ADC to +/- 4.096V
  ads.setDataRate(RATE_ADS1115_64SPS);  // Change Sample Rate to 64SPS
  Wire.begin(I2C_SDA, I2C_SCL);         // Change Default SCL and SDA Pins to PCB Values
  ads.begin();
  delay(500);

  //---------------------// Test ADS1115 Connection //---------------------//
  if(!ads.begin())
  {
    Serial.println("!!! Failed to Initialise ADS !!!\n");
    while(1);
  }
  else
  {
    Serial.println("*** ADS1115 is Ready ***\n");
  }

  //---------------------// Test SPIFFS Connection //---------------------//
  if(!SPIFFS.begin())
  {
   Serial.println("!!! Failed to Mount SPIFFS !!!\n");
   return;
  }

  //---------------------// Setup LEDs //---------------------//

  ledcSetup(0, LED_freq, LED_res);
  ledcSetup(1, LED_freq, LED_res);
  ledcSetup(2, LED_freq, LED_res);
  ledcSetup(3, LED_freq, LED_res);
  ledcAttachPin(RG_PIN_1, 0);
  ledcAttachPin(RG_PIN_2, 1);
  ledcAttachPin(IR_PIN_1, 2);
  ledcAttachPin(IR_PIN_2, 3);
  
  Serial.println("*** LEDs are Ready ***\n");

  //---------------------// Setup Server //---------------------//
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/index.html");
  });

  server.on("/ppg", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send_P(200, "text/plain", sendToWebServer().c_str());
  });

  server.begin();

  Serial.println("*** WebServer Started ***\n");
  
  Serial.println(">>> Please Connect to Server via IP <<<");
 
  //*********// Grace Time //*********//
  delay(5000);

  //---------------------// Investigate Characteristics //---------------------//
  Serial.println(">>> MEASUREMENT START <<<");

  testBrightness(2, 0);   // RG with 2 LEDs
  testBrightness(4, 0);   // RG with 4 LEDs
  testBrightness(2, 2);   // IR with 2 LEDs
  testBrightness(4, 2);   // IR with 4 LEDs

  Serial.println(">>> MEASUREMENT END <<<");

  if(SEND_TO_FTP)
  {
    //*********// Set FTP File Names //*********//  
    char *file_dataName = new char[strlen(device_name) + 15];
    char *file_timeName = new char[strlen(device_name) + 15];

    EEPROM.begin(sizeof(file_nameCounter));
    EEPROM.get(0, file_nameCounter);

    if(file_nameCounter > 100)
    {
      file_nameCounter = 0;
    }

    sprintf(file_dataName, "%s_data_%u.txt", device_name, file_nameCounter);
    sprintf(file_timeName, "%s_time_%u.txt", device_name, file_nameCounter);

    file_nameCounter++;

    EEPROM.put(0, file_nameCounter);
    EEPROM.commit();
    EEPROM.end();

    Serial.print(">>> Device File Name: ");
    Serial.print(file_dataName);
    Serial.println(" <<<\n");

    //*********// Get Data for FTP Transfer //*********//  
    unsigned char* ADC_dataChar_1 = (unsigned char*)ADC_dataString_1.c_str();
    unsigned char* ADC_dataChar_2 = (unsigned char*)ADC_dataString_2.c_str();
    unsigned char* ADC_timeChar = (unsigned char*)ADC_timeString.c_str();

    int length_data_1 = 0;
    int length_data_2 = 0;
    int length_time = 0;

    while(ADC_dataChar_1[length_data_1] != '\0')
    {
      length_data_1++;
    }

    if(ADC_INDX >= 700)
    {
      while(ADC_dataChar_2[length_data_2] != '\0')
      {
        length_data_2++;
      }
    }

    while(ADC_timeChar[length_time] != '\0')
    {
      length_time++;
    }

    //*********// Start FTP and Send //*********// 
    Serial.println("");
    ftp.OpenConnection();

    // Create the new file and send the data
    ftp.InitFile("Type A");
    ftp.NewFile(file_dataName);
    ftp.WriteData(ADC_dataChar_1, length_data_1);
    ftp.CloseFile();

    if(total_indx >= 10000)
    {
      ftp.InitFile("Type A");
      ftp.AppendFile(file_dataName);
      ftp.WriteData(ADC_dataChar_2, length_data_2);
      ftp.CloseFile();
    }
    
    // Create the new file and send the time
    ftp.InitFile("Type A");
    ftp.NewFile(file_timeName);
    ftp.WriteData(ADC_timeChar, length_time);
    ftp.CloseFile();

    ftp.CloseConnection();
    
    delete[] file_dataName;
    delete[] file_timeName;
  }

  server.end();
  WiFi.disconnect();

  if(PRINT_TO_SERIAL)
  {
    Serial.println("---------- DATA ----------");
    Serial.println("---------- 1 ----------");
    Serial.println(ADC_dataString_1);

    if(total_indx >= 10000)
    {
      Serial.println("---------- 2 ----------");
      Serial.println(ADC_dataString_2);
    }

    Serial.println("---------- TIME ----------");
    Serial.println(ADC_timeString);
  }

  Serial.println("\n*** ESP32 Operation Completed ***");
  Serial.println("*****************************************");

  esp_deep_sleep_start();
}

void loop()
{

}

void testBrightness(int num, int channel)
{
  int duty_cycle = 0; 

  for(int j = 0; j < 3; j++)
  {
    duty_cycle = 100 + 75*j;

    ledcWrite(channel, duty_cycle);

    if(num == 4)
    {
      ledcWrite((channel + 1), duty_cycle);
    }

    delay(1000);
    
    for(int i = 0; i < ADC_INDX; i++)
    {
      ADC_data[i] = ads.readADC_SingleEnded(0);

      if(i < TIM_INDX)
      {
        ADC_time[i] = millis();
        ADC_timeString += String(ADC_time[i]);
        ADC_timeString += "\n";
      }

      if(total_indx < 10000)
      {
        ADC_dataString_1 += String(ADC_data[i]);
        ADC_dataString_1 += "\n";
      }

      else
      {
          ADC_dataString_2 += String(ADC_data[i]);
          ADC_dataString_2 += "\n";
      }
      
      buffer_dataString += String(ADC_data[i]);
      buffer_dataString += "\n";
      
      if(buffer_clearFlag)
      {
        buffer_dataString = "";
        buffer_clearFlag = false;
      }

      total_indx++;
    }
  }

  ledcWrite(channel, 0);

  if(num == 4)
  {
    ledcWrite((channel + 1), 0);
  }

  delay(1000);

  return;
}
