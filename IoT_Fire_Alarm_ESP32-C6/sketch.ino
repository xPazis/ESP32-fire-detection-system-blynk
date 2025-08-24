//Blynk Initialise
//#define BLYNK_TEMPLATE_ID "YOUR TEMPLATE ID"
//#define BLYNK_TEMPLATE_NAME "YOUR TEMPLATE NAME"
//#define BLYNK_AUTH_TOKEN "YOUR AUTHORISATION TOKEN"

#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <WiFiClient.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <string.h>


//Wifi
char SSID[] = "Wokwi-GUEST";
char PASS[] = "";
//Blynk
#define V_SMOKE_VALUE V1    // Virtual Pin for Smoke Value
#define V_CELSIUS_VALUE V2  // Virtual Pin for Celsius Temperature
#define V_FIRE_ALARM V3     // Virtual Pin for Fire Alarm Status (0 for no fire, 1 for fire)
#define V_SCREEN_MESSAGE V4 // Virtual Pin for the message of the I2C Screen
//Pins
#define LED_PIN 12        //LED 
#define BUTTON_PIN 9      //BUTTON 
#define SOUND_PIN 13      //BUZZER
#define A_SMOKE_PIN A1    //ANALOG PIN OF GAS SENSOR 
#define TEMPERATURE_PIN A5  //ANALOG PIN OF TEMPERATURE SENSOR
//Screen
#define I2C_ADDR    0x27  //DEFAULT I2C ADDRESS
#define LCD_COLUMNS 20    //SCREEN COLLUMNS
#define LCD_LINES   4     //SCREEN LINES
String message ="";

LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_LINES);  //SCREEN CONSTRUCTOR

//VARIABLES AND CONSTANTS
int FIRE_DANGER = LOW;                  // Cuurent fire threat level
//Button
int buttonState = HIGH;                 // Current state of the button
//Temperature
float lastTemp;                         //Last value of temperature
const float RATE_OF_RISE_TEMP = 5.0;    //Maximum number temperature can rise in x seconds (celsius)
const float BETA = 3950;                // should match the Beta Coefficient of the thermistor
const float FIRE_THRESHOLD_TEMP = 55.0; //Top Celsius Temperature before alarm
const int SMOKE_THRESHOLD_TEMP = 3650;  //Top Smoke Value Read before alarm

//smoke
int lastSmoke;                          //Last value of gas concentration
const int RATE_OF_RISE_SMOKE = 50;      //Maximum number gas concentration can rise in x seconds (ppm)
//time
const unsigned int RATE_OF_RISE_INTERVAL = 10000; //Time from one check to another (10 seconds)
unsigned long lastCheckTime = 0;        //Last time check happened


void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN,OUTPUT);
  pinMode(BUTTON_PIN,INPUT_PULLUP);
  //Screen initialize
  Wire.begin(21,22); // Initialize I2C with custom pins
  lcd.init();
  lcd.backlight();
  delay(200);                         //Warm up the temperature detector
  //delay(30000);                     //Warm up the smoke detector (not neccecary in wokwi)
  lastTemp = analogRead(TEMPERATURE_PIN); //Initiate Value
  lastSmoke = analogRead(A_SMOKE_PIN);    //Initiate Value
  //Initialise WiFi Connection
  Blynk.begin(BLYNK_AUTH_TOKEN, SSID, PASS);
}

//Clear the LCD Screen
void resetScreen(){
  lcd.setCursor(0,0);
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("                    ");
  lcd.setCursor(0,2);
  lcd.print("                    ");
  lcd.setCursor(0,3);
  lcd.print("                    ");
}

//Print the threat message depending on the threat
void printScreen(int x,float temperature,int gas){
  resetScreen();
  switch(x){
    //button
    case 1:
      // Serial.println("Fire Danger: Button Pressed");
      lcd.setCursor(0,1);
      lcd.print("FIRE DANGER:");
      lcd.setCursor(0,2);
      lcd.print("BUTTON PRESSED");
      break;
    //temperature
    case 2:
      // Serial.print("Fire Danger (Fixed Temperature): ");
      // Serial.print(temperature);
      // Serial.println(" ℃");
      lcd.setCursor(0,0);
      lcd.print("FIRE DANGER:");
      lcd.setCursor(0,1);
      lcd.print("FIXED TEMPERATURE:");
      lcd.setCursor(0,2);
      lcd.print(temperature);
      break;
    //gas
    case 3:
      // Serial.print("Fire Danger (Fixed Smoke Concentration): ");
      // Serial.print(gas);
      // Serial.println(" ppm");
      lcd.setCursor(0,0);
      lcd.print("FIRE DANGER:");
      lcd.setCursor(0,1);
      lcd.print("FIXED SMOKE");
      lcd.setCursor(0,2);
      lcd.print("CONCENTRATION:");
      lcd.setCursor(0,3);
      lcd.print(gas);
      lcd.print(" PPM");
      break;
    //Sudden rise in temperature
    case 4:
      // Serial.print("Fire Danger (Rate of Rise / Temperature): ");
      // Serial.print(lastTemp);
      // Serial.print(" ℃");
      // Serial.print(" -> ");
      // Serial.print(temperature);
      // Serial.println(" ℃");
      lcd.setCursor(0,0);
      lcd.print("FIRE DANGER");
      lcd.setCursor(0,1);
      lcd.print("RATE OF RISE");
      lcd.setCursor(0,2);
      lcd.print("TEMPERATURE");
      lcd.setCursor(0,3);
      lcd.print(lastTemp);
      lcd.print(" -> ");
      lcd.print(temperature);
      break;
    //sudden rise in gas concentration
    case 5:
      // Serial.print("Fire Danger (Rate of Rise / Smoke): ");
      // Serial.print(lastSmoke);
      // Serial.print(" ppm");
      // Serial.print(" -> ");
      // Serial.print(gas);
      // Serial.println(" ppm");
      lcd.setCursor(0,0);
      lcd.print("FIRE DANGER");
      lcd.setCursor(0,1);
      lcd.print("RATE OF RISE");
      lcd.setCursor(0,2);
      lcd.print("SMOKE");
      lcd.setCursor(0,3);
      lcd.print(lastSmoke);
      lcd.print(" PPM");
      lcd.print(" -> ");
      lcd.print(gas);
      lcd.print(" PPM");
      break;
    default:
      // Serial.print("NO FIRE");
      lcd.setCursor(0,0);
      lcd.print("FALSE ALARM");
  }
  delay(200);
}

void beep(){
    digitalWrite(LED_PIN, HIGH);
    tone(SOUND_PIN, 262, 250);  //262 hertz , 250 milliseconds
    delay(250);
    digitalWrite(LED_PIN, LOW);
    delay(250);
}

void sendMessages(int x,float y){
  //Data sending to blynk
    Blynk.virtualWrite(V_SCREEN_MESSAGE,message);
    Blynk.virtualWrite(V_SMOKE_VALUE, x);
    Blynk.virtualWrite(V_CELSIUS_VALUE, y);
    Blynk.virtualWrite(V_FIRE_ALARM, FIRE_DANGER);
}

void loop() {
  Blynk.run();
  int counter = 0;
  //readings
  int buttonReading = digitalRead(BUTTON_PIN);

  //temp readings and temperature calculation
  int temperatureValue = analogRead(TEMPERATURE_PIN);
  float celsius = 1 / (log(1 / (4095. / temperatureValue - 1)) / BETA + 1.0 / 298.15) - 273.15;

  //smoke readings
  int smokeValue = analogRead(A_SMOKE_PIN);

  //type of threat 0. nothing 1.button 2.temperature 3.gas 4.rise in temperature 5. rise in gas concentration
  int threat = 0;

  //Button check
  if (buttonReading) {  //0 If pressed, 1 if not pressed
    FIRE_DANGER = LOW;
    message = "No Fire";  //Blynk message
  }
  else{
    FIRE_DANGER = HIGH;
    threat = 1;
    printScreen(threat,celsius,smokeValue); //I2C Screen display
    message = "Fire Danger: Button Pressed"; //Blynk message
    counter +=1;
  }


  //temperature sensor check
  if (celsius > FIRE_THRESHOLD_TEMP) {
    FIRE_DANGER = HIGH;
    threat = 2;
    printScreen(threat,celsius,smokeValue);
    message = "Fire Danger (Fixed Temperature)";
    counter +=1;
  }

  //gas sensor check
  if (smokeValue > SMOKE_THRESHOLD_TEMP){
    FIRE_DANGER = HIGH;
    threat = 3;
    printScreen(threat,celsius,smokeValue);
    message = "Fire Danger (Fixed Smoke Concentration)";
    counter +=1;
  }

  //sudden rise in temperature or smoke, if sudden rise detected, activate alarm for a small period of time
  unsigned long currentTime = millis();
  if (currentTime - lastCheckTime >= RATE_OF_RISE_INTERVAL) {
    if (celsius - lastTemp > RATE_OF_RISE_TEMP) {
      FIRE_DANGER = HIGH;
      threat = 4;
      printScreen(threat,celsius,smokeValue);
      message = "Fire Danger (Rate of Rise / Temperature)";
      sendMessages(smokeValue,celsius);
      for(int i=0; i<5; i++){
        beep();  
      }
    }
    if(smokeValue - lastSmoke > RATE_OF_RISE_SMOKE){
      FIRE_DANGER = HIGH;
      threat = 5;
      printScreen(threat,celsius,smokeValue);
      message = "Fire Danger (Rate of Rise / Smoke)";
      sendMessages(smokeValue,celsius);
      for(int i=0; i<5; i++){
        beep();  
      }
    }
    sendMessages(smokeValue,celsius);
    lastSmoke = smokeValue;
    lastTemp = celsius;
    lastCheckTime = currentTime;
  }

  if(counter == 0){
    FIRE_DANGER = LOW;
    message = "Safe Environment";
  }

  //when fire exists
  if (FIRE_DANGER == HIGH) {
    beep();
    sendMessages(smokeValue,celsius);
  } else {
    digitalWrite(LED_PIN, LOW);
    resetScreen();
  }
}

