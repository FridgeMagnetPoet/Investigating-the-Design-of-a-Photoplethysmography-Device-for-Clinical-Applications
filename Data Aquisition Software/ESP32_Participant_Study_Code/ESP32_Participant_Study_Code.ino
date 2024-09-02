/* This code serves as the ESP32 code used during the participant study of the project:
 * Investigating the Design of a  Photoplethysmography Device for Clinical Applications
 * By A. Aspeling, 2024
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESP32_FTPClient.h>
#include <Adafruit_ADS1X15.h>
#include <EEPROM.h>
#include <Wire.h>
#include <string.h>

// SETTINGS
const int ADC_INDX = 6000; // calculated as (time in milliseconds)/18 and rounded

char FTP_server[] = "192.168.176.58";

char device_name[] = "ONSM_RED";

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
const int GREEN_DUTY_CYCLE = 100;
const int RED_DUTY_CYCLE = 175;
const int IR_DUTY_CYCLE = 250;

// Init ADS1115 ADC Properties
Adafruit_ADS1115 ads;
String ADC_dataString_RG = "";
String ADC_dataString_IR = "";
String ADC_timeString_RG = "";
String ADC_timeString_IR = "";
int16_t ADC_data = 0;
unsigned long ADC_time = 0;

// Init WiFi Properties
const char* WiFi_SSID = "Watermelon";
const char* WiFi_PASS = "ekgaanjouniesenie";

// Init FTP Properties
char FTP_user[] = "esp32";
char FTP_pass[] = "esp32";
uint16_t FTP_port = 2221;
ESP32_FTPClient ftp(FTP_server, FTP_port, FTP_user, FTP_pass, 5000, 2);
unsigned int file_nameCounter;

void setup() 
{
  //---------------------// Serial Setup //---------------------//
  Serial.begin(115200);
  delay(500);

  Serial.println("*****************************************");
  Serial.println("*** ESP32 is Ready ***\n");

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

  //---------------------// Setup LEDs //---------------------//

  ledcSetup(0, LED_freq, LED_res);
  ledcSetup(1, LED_freq, LED_res);
  ledcSetup(2, LED_freq, LED_res);
  ledcAttachPin(RG_PIN_1, 0);
  ledcAttachPin(RG_PIN_2, 1);
  ledcAttachPin(IR_PIN_1, 2);
  
  Serial.println("*** LEDs are Ready ***\n");

  //---------------------// Measurements //---------------------//
  Serial.println(">>> MEASUREMENT START <<<");

  bool rg_flag = false;

  if(strcmp(device_name, "ONSM_GREEN") == 0)
  {
    rg_flag = false;
  }

  else if(strcmp(device_name, "ONSM_RED") == 0)
  {
    rg_flag = true;
  }
  
  for(int i = 0; i < ADC_INDX; i++)
  {
    if((i%(ADC_INDX/2)) == 0)
    {
      if(rg_flag == true)
      {
        // Change to IR LEDs
        ledcWrite(0, 0);
        ledcWrite(1, 0);

        ledcWrite(2, IR_DUTY_CYCLE);

        rg_flag = false;
      }

      else
      {
        // Change to RG LEDs
        ledcWrite(2, 0);
        
        if(strcmp(device_name, "ONSM_GREEN") == 0)
        {
          ledcWrite(0, GREEN_DUTY_CYCLE);
          ledcWrite(1, GREEN_DUTY_CYCLE);
        }

        else if(strcmp(device_name, "ONSM_RED") == 0)
        {
          ledcWrite(0, RED_DUTY_CYCLE);
          ledcWrite(1, RED_DUTY_CYCLE);
        }
        
        rg_flag = true;
      }

      delay(1000);
    }

    ADC_data = ads.readADC_SingleEnded(0);
    ADC_time = millis();

    // Save IR
    if(rg_flag == false)
    {
      ADC_dataString_IR += String(ADC_data);
      ADC_dataString_IR += "\n";

      ADC_timeString_IR += String(ADC_time);
      ADC_timeString_IR += "\n";
    }

    // Save RG
    else if(rg_flag == true)
    {
      ADC_dataString_RG += String(ADC_data);
      ADC_dataString_RG += "\n";

      ADC_timeString_RG += String(ADC_time);
      ADC_timeString_RG += "\n";
    }
  }

  ledcWrite(0, 0);
  ledcWrite(1, 0);
  ledcWrite(2, 0);

  Serial.println(">>> MEASUREMENT END <<<");

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
  Serial.print("\n");

  //---------------------// Send to FTP //---------------------//
  //*********// Set FTP File Names //*********//  
  char *file_dataName_RG = new char[strlen(device_name) + 18];
  char *file_dataName_IR = new char[strlen(device_name) + 18];
  char *file_timeName_RG = new char[strlen(device_name) + 18];
  char *file_timeName_IR = new char[strlen(device_name) + 18];

  EEPROM.begin(sizeof(file_nameCounter));
  EEPROM.get(0, file_nameCounter);

  if(file_nameCounter > 150)
  {
    file_nameCounter = 0;
  }

  sprintf(file_dataName_RG, "%s_data_RG_%u.txt", device_name, file_nameCounter);
  sprintf(file_dataName_IR, "%s_data_IR_%u.txt", device_name, file_nameCounter);
  sprintf(file_timeName_RG, "%s_time_RG_%u.txt", device_name, file_nameCounter);
  sprintf(file_timeName_IR, "%s_time_IR_%u.txt", device_name, file_nameCounter);

  file_nameCounter++;

  EEPROM.put(0, file_nameCounter);
  EEPROM.commit();
  EEPROM.end();

  Serial.print(">>> File Name: ");
  Serial.print(file_dataName_RG);
  Serial.println(" <<<\n");

  //*********// Get Data for FTP Transfer //*********//  
  unsigned char* ADC_dataChar_RG = (unsigned char*)ADC_dataString_RG.c_str();
  unsigned char* ADC_dataChar_IR = (unsigned char*)ADC_dataString_IR.c_str();
  unsigned char* ADC_timeChar_RG = (unsigned char*)ADC_timeString_RG.c_str();
  unsigned char* ADC_timeChar_IR = (unsigned char*)ADC_timeString_IR.c_str();

  int length_data_RG = 0;
  int length_data_IR = 0;
  int length_time_RG = 0;
  int length_time_IR = 0;

  while(ADC_dataChar_RG[length_data_RG] != '\0')
  {
    length_data_RG++;
  }

  while(ADC_dataChar_IR[length_data_IR] != '\0')
  {
    length_data_IR++;
  }

  while(ADC_timeChar_RG[length_time_RG] != '\0')
  {
    length_time_RG++;
  }

  while(ADC_timeChar_IR[length_time_IR] != '\0')
  {
    length_time_IR++;
  }

  //*********// Start FTP and Send //*********// 
  Serial.println("");
  ftp.OpenConnection();
  delay(100);

  // Create the new file and send the data
  ftp.InitFile("Type A");
  ftp.NewFile(file_dataName_RG);
  ftp.WriteData(ADC_dataChar_RG, length_data_RG);
  ftp.CloseFile();

  delay(100);

  ftp.InitFile("Type A");
  ftp.NewFile(file_dataName_IR);
  ftp.WriteData(ADC_dataChar_IR, length_data_IR);
  ftp.CloseFile();

  delay(100);

  // Create the new file and send the time
  ftp.InitFile("Type A");
  ftp.NewFile(file_timeName_RG);
  ftp.WriteData(ADC_timeChar_RG, length_time_RG);
  ftp.CloseFile();

  delay(100);

  ftp.InitFile("Type A");
  ftp.NewFile(file_timeName_IR);
  ftp.WriteData(ADC_timeChar_IR, length_time_IR);
  ftp.CloseFile();

  delay(100);

  ftp.CloseConnection();

  delay(1000);

  Serial.println(">>> FTP END <<<");

  delete[] file_dataName_RG;
  delete[] file_dataName_IR;
  delete[] file_timeName_RG;
  delete[] file_timeName_IR;

  WiFi.disconnect();

  Serial.println("---------- DATA ----------");
  Serial.println("---------- RG ----------");
  Serial.println(ADC_dataString_RG);
  Serial.println("---------- IR ----------");
  Serial.println(ADC_dataString_IR);
  Serial.println("---------- TIME ----------");
  Serial.println("---------- RG ----------");
  Serial.println(ADC_timeString_RG);
  Serial.println("---------- IR ----------");
  Serial.println(ADC_timeString_IR);

  Serial.println("\n*** ESP32 Operation Completed ***");
  Serial.println("*****************************************");

  esp_deep_sleep_start();
}

void loop() 
{
}
