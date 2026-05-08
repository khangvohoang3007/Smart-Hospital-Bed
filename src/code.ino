////// Type your Blynk config here
#define BLYNK_TEMPLATE_ID "....."
#define BLYNK_TEMPLATE_NAME "SMART HOSPITAL BED"
#define BLYNK_AUTH_TOKEN "....."
//////////////////////////
#define BLYNK_PRINT Serial

#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "MAX30100_PulseOximeter.h"
#include "DFRobot_DF2301Q.h"
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>

///////////Ultrasonic Sensor//////////////////
#define ECHO_PIN 12
#define TRIG_PIN 13

///////////Button&Led&Buzzer//////////////////
#define SSbutton 34
#define SSled 5
#define OLbutton 23
#define Buzzer 19
#define CHbutton 26

///////////OLED//////////////////
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define i2c_Address 0x3c

///////////Delay display Serial Monitor//////////////////
#define REPORTING_PERIOD_MS 1000

///////////Setting Ultrasonic Sensor Warning//////////////////
#define THRESHOLD_DISTANCE 50
#define DELAY_TIME 3000


volatile int waitSTATE = 0, displaySTATE = 0;
volatile unsigned long lastInterruptTime = 0;
volatile bool callHelpFlag = false;

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

PulseOximeter pox;
uint32_t tsLastReport = 0;

long duration;
int distance, systemSTATE;

DFRobot_DF2301Q_I2C asr;

Servo sg90_head;
int servoPin_head = 17;
Servo sg90_leg;
int servoPin_leg = 16;

BlynkTimer timer;

const char* auth = BLYNK_AUTH_TOKEN;
const char* ssid = "type here"; // type your wifi name
const char* pass = "type here"; // typur wifi password

int blinkOled = 0;
float r;
int p;

static const unsigned char PROGMEM drop_iconHEART[] = {
  B00000000, B00000000,
  B00001110, B00111000,
  B00011111, B01111100,
  B00111111, B11111110,
  B01111111, B11111111,
  B01111110, B00111111,
  B01111110, B00111111,
  B01111000, B00001111,
  B01111000, B00001111,
  B00111000, B00001110,
  B00011110, B00111100,
  B00001110, B00111000,
  B00000111, B11110000,
  B00000011, B11100000,
  B00000001, B11000000,
  B00000000, B10000000
};

static const unsigned char PROGMEM drop_iconOXY[] = {
  B00000000, B00000110,
  B00000001, B10001001,
  B00000111, B11100010,
  B00001111, B11110111,
  B00011100, B00111000,
  B00011000, B00011000,
  B00110000, B00001100,
  B00110000, B00001100,
  B00110000, B00001100,
  B00110000, B00001100,
  B00011000, B00011000,
  B00011100, B00111000,
  B00001111, B11110000,
  B00000111, B11100000,
  B00000001, B10000000,
  B00000000, B00000000
};


///////////Debounce for Oled display button//////////////////
void IRAM_ATTR debounce_OLbutton() 
{
  unsigned long interruptTime = millis();

  if (interruptTime - lastInterruptTime > 200) 
  {
    displaySTATE++;
    if (displaySTATE == 3) displaySTATE = 0;
    lastInterruptTime = interruptTime;
  }
}

///////////Debounce for Ultrasonic Waring button//////////////////
void IRAM_ATTR debounce_SSbutton() 
{
  unsigned long interruptTime = millis();

  if (interruptTime - lastInterruptTime > 200) 
  {
    waitSTATE++;
    if (waitSTATE == 2) waitSTATE = 0;
    lastInterruptTime = interruptTime;
  }
}

///////////Debounce for Calling Help button//////////////////
void IRAM_ATTR debounce_CHbutton() 
{
  unsigned long interruptTime = millis();

  if (interruptTime - lastInterruptTime > 200) 
  {
    callHelpFlag = true;
    lastInterruptTime = interruptTime;
  }
}

///////////Display on OLED//////////////////
void displayOL() 
{
  pox.update();
  switch (displaySTATE) 
    {
      case 1:
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(35, 0);
        display.println("HEART RATE");
        drawScaledBitmap(15, 15, drop_iconHEART, 16, 16, 2);
        display.setTextSize(2);
        display.setCursor(55, 25);
        display.print(pox.getHeartRate());
        display.setCursor(40, 45);
        display.println("(Bpm)");
        display.display();
        delay(1);
        break;

      case 2:
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(25, 0);
        display.println("PULSE OXIMETRY");
        drawScaledBitmap(20, 15, drop_iconOXY, 16, 16, 2);
        display.setTextSize(2);
        display.setCursor(65, 25);
        display.print(pox.getSpO2());
        display.setCursor(55, 45);
        display.println("(%)");
        display.display();
        delay(1);
        break;

      default:
        display.clearDisplay();
        display.display();
        delay(1);
        break;
    }
}

///////////Display larger ICON//////////////////
void drawScaledBitmap(int x, int y, const uint8_t *bitmap, int width, int height, int scale) 
{
  for (int j = 0; j < height; j++) 
  {
    for (int i = 0; i < width; i++) 
    {
      if (bitmap[j * (width / 8) + i / 8] & (1 << (7 - (i % 8)))) 
      {
        display.fillRect(x + i * scale, y + j * scale, scale, scale, SH110X_WHITE);
      }
    }
  }
}

///////////Annouce when have heart beat//////////////////
void onBeatDetected() 
{
  Serial.println("Beat!");
}

///////////Take variable from Ultrasonic Sensor//////////////////
void UltraSonic_Sensor() 
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH);
  distance = duration / 58.2;
}

///////////Turn on/off check if Patient still on bed//////////////////
void checkPATIENT() 
{
  // Check Patient Assistance
  if (callHelpFlag) 
  {
    Blynk.logEvent("call_help");
    Serial.println("Goi nguoi ho tro: Success");
    callHelpFlag = false;
  }

  // Check Patient on bed
  if (waitSTATE == 0) 
  {
    displayOL();

    digitalWrite(SSled, LOW);
    digitalWrite(Buzzer, LOW);

    distance = 0;
    systemSTATE = 0;
  } 
  else 
  {
    digitalWrite(SSled, HIGH);

    // Check health patient state
    if(pox.getHeartRate()<=20 || pox.getSpO2()<=90)
    {
      Blynk.logEvent("emergency_case");

      digitalWrite(Buzzer, HIGH);

      //Annoucing EMERGENCY SITUATION!!!
      display.clearDisplay();
      display.setCursor(0,0);  
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.print("EMERGENCY SITUATION!!");
      display.setCursor(0,15);  
      display.setTextColor(SH110X_WHITE);
      display.print("Press button to stop");
      display.display();

      pox.update();
    }
    else
    {
      displayOL();
      UltraSonic_Sensor();
      
      // Check waiting state
      if (systemSTATE == 0) 
      {
        systemSTATE = 1;
      }
    }

  }

  // Waiting process
  if (systemSTATE == 1) 
  {
    unsigned long interruptTime = millis();
    if (interruptTime - lastInterruptTime > 10000) 
    {
      lastInterruptTime = interruptTime;
      if (distance > 50) 
      {
        systemSTATE = 2;
      }
    }
  }

  if (systemSTATE == 2) 
  {
    digitalWrite(Buzzer, HIGH);
    Blynk.logEvent("emergency_case");
    if (distance <= 50) 
    {
      systemSTATE = 0;  // Quay lại trạng thái tắt
      digitalWrite(Buzzer, LOW);
    }
  }
}

///////////Display on Serial Monitor//////////////////
void displaySM_Control_SendBlynk() 
{
  
  uint8_t CMDID = asr.getCMDID();

  pox.update();
  r = pox.getHeartRate();  //Read the Heart Rate
  p = pox.getSpO2(); // Read the SpO2

  if (millis() - tsLastReport > REPORTING_PERIOD_MS) 
  {
    Serial.print("Heart rate:");
    Serial.print(pox.getHeartRate());
    Serial.print("bpm / SpO2:");
    Serial.print(pox.getSpO2());
    Serial.println("%");

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    //Blynk set up for Heart Rate and SpO2
    Blynk.virtualWrite(V0, r);
    Blynk.virtualWrite(V1, p);

    tsLastReport = millis();

    Serial.println("------------------------------");
  }

  if (CMDID != 0) 
  {
    switch (CMDID) 
    {
      //Nang dau giuong
      case 5:                                                                                 
        Serial.println("Nang Dau Giuong: Success");  
        sg90_head.write(35);
        break;

      //Ha dau giuong.
      case 6:                                                                               
        Serial.println("Ha Dau Giuong: Success");  
        sg90_head.write(0);
        break;

      //Nang chan
      case 8:                                                                               
        Serial.println("Nang chan: Success");  
        sg90_leg.write(0);
        break;

      //Ha chan
      case 7:                                                                               
        Serial.println("Ha chan: Success");
        sg90_leg.write(35);  
        break;

      //Tro ve nhu cu
      case 9:                                                                               
        Serial.println("Tro ve nhu cu: Success"); 
        sg90_leg.write(0); 
        sg90_head.write(0);
        break;
      
      //Goi nguoi ho tro
      case 10:                                                                               
        Serial.println("Goi nguoi ho tro: Success");  
        Blynk.logEvent("call_help");
        break;


      default:
          Serial.print("CMDID = ");  //Printing command ID
          Serial.println(CMDID);
    }
  }

}



void setup() 
{
  ///Baud rate for serial monitor
  Serial.begin(115200);

  //Oled setup
  display.begin(i2c_Address, true);

  //Display connecting to "WIFI NAME"
  display.clearDisplay(); 
  display.setCursor(0,0);  
  display.setTextSize(1); 
  display.setTextColor(SH110X_WHITE); 
  display.println("Connecting to"); 
  display.setTextSize(1); 
  display.setCursor(0,15); 
  display.print(ssid);
  display.display();

  //Connect Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  //Check if wifi are connected
  while (WiFi.status() != WL_CONNECTED)
  {
    //Delay to see if Wifi has done connection, if not ...
    delay(500);

    //Blink "..." on OLED
    if (blinkOled == 0) 
    {
      display.setTextColor(SH110X_WHITE);
      display.setCursor(80,0); 
      display.print("...");
      display.display();

      blinkOled=1;
    } 
    else 
    {
      display.setTextColor(SH110X_BLACK);
      display.setCursor(80,0); 
      display.print("...");
      display.display();

      blinkOled=0;
    }
  }

  // Connect to Blynk sever
  Blynk.begin(auth, ssid, pass);

  //Annoucing successfully connected to WiFi
  display.clearDisplay();
  display.setCursor(25,0);  
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.print("WiFi CONNECTED");
  display.display();

  delay(3000);

  ///Button&Led&Sensor&Buzzer pins setup
  pinMode(CHbutton, INPUT);
  pinMode(SSbutton, INPUT);
  pinMode(OLbutton, INPUT);
  pinMode(SSled, OUTPUT);
  pinMode(Buzzer, OUTPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  sg90_head.attach(servoPin_head);
  sg90_head.write(0);
  sg90_leg.attach(servoPin_leg);
  sg90_leg.write(0);

  attachInterrupt(digitalPinToInterrupt(OLbutton), debounce_OLbutton, FALLING);
  attachInterrupt(digitalPinToInterrupt(SSbutton), debounce_SSbutton, FALLING);
  attachInterrupt(digitalPinToInterrupt(CHbutton), debounce_CHbutton, FALLING);

  ///Initialize program
  waitSTATE = 0;
  displaySTATE = 0;
  systemSTATE = 0;

  digitalWrite(SSled, LOW);

  // Init the DFRobot
  while (!(asr.begin())) 
  {
    Serial.println("Communication with device failed, please check connection");
    delay(3000);
  }
  Serial.println("Begin ok!");

  asr.setVolume(7);
  asr.setMuteMode(0);
  asr.setWakeTime(20);

  uint8_t wakeTime = 0;
  wakeTime = asr.getWakeTime();
  Serial.print("wakeTime = ");
  Serial.println(wakeTime);

  ///Initialize MAX30100
  Serial.print("Initializing pulse oximeter..");
  if (!pox.begin()) 
  {
    Serial.println("FAILED");
    for (;;)
      ;
  } 
  else 
  {
    Serial.println("SUCCESS");
  }
  pox.setOnBeatDetectedCallback(onBeatDetected);

}



void loop() 
{
  Blynk.run();
  checkPATIENT();
  displaySM_Control_SendBlynk();
}
