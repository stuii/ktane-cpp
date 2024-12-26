#include "Arduino.h"
#include <Wire.h>
#include <ArduinoJson.h>
#include <stdlib.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

struct Port {
  String name;
  int count;
};

struct Label {
  String label;
  bool lit;
};

void generateSerialNumber(int length = 8);
void initializeSerialDisplay();
void displaySerialNumber();
void blankSerialNumber();

void setupGame();
void provisionModules();
void startGame();

void initializeMenuDisplay();
void displayMenu();
String getTimeForDisplay();
void rotaryEncoderButtonPressed();

void seedRandomness();
void doScanForRequests();
void initializeRequestPins();
void discoverModules();
byte sendCommand(byte address, String command);
int findRequestPin();
void enableModule(byte i2cAddress, int requestPin);
void enableModuleInterrupt();
void incomingRequest();

void generateLabels();
void generateBatteries();
void generatePorts();
bool intIssetInArray(int haystack[], int search);

#define MODULE_START_ADDRESS 0x00  // Starting I2C address
#define MODULE_END_ADDRESS 0x7F    // Ending I2C address
const int REQUEST_PINS[] = { /*23,*/ 24, 25, 26, 27, 28, 29, 30, 31/*, 32, 33*/ };

int ACTIVE_MODULES = 0;
int MODULE_COUNT = 8;
int MODULE_ADDRESSES[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int ASSIGNED_REQUEST_PINS[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

const bool debug = true;
bool scanForRequest = false;

/* PIN DEFINITIONS */
const byte RANDOMNESS_SOURCE = A1;
const int REQUEST_INTERRUPT_PIN = 2;
const int MENU_DISPLAY_CS_PIN = 46;
const int MENU_DISPLAY_RST_PIN = 44;
const int MENU_DISPLAY_DC_PIN = 5;
const int SERIAL_DISPLAY_CS_PIN = 10; // default 5
const int SERIAL_DISPLAY_RST_PIN = 2; // default 2
const int SERIAL_DISPLAY_DC_PIN = 5; // default 0
const int SERIAL_DISPLAY_BUSY_PIN = 15; // default 15
const int SERIAL_DISPLAY_WIDTH = 250; // SSD1680
const int SERIAL_DISPLAY_HEIGHT = 122; // DEPG0213BN


/* GAME BOUNDARIES */
// Ports
const int maxPortsTotal = 5;
const int maxPortsPerType = 2;
const String possiblePorts[] = {"VGA", "PS2", "RJ45", "RCA"};
// Labels
const int maxLabels = 4;
const String possibleLabels[] = {"SND", "CLR", "CAR", "IND", "FRQ", "SIG", "NSA", "MSA", "TRN", "BOB", "FRK"};
// Batteries
const int maxBatteriesTotal = 8;
const String possibleBatteryTypes[] = {"AA", "D"};


/* GAME SETTINGS */
int baseLives = 3;
int baseTime = 180;
String serialNumber = "";
Port bombPorts[maxPortsTotal] = {};
Label bombLabels[maxLabels] = {};
int generatedLabelCount = 0;

/* EINK SERIAL DISPLAY */
const int SERIAL_DISPLAY_ROTATION = 1;
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> serialDisplay(
  GxEPD2_213_BN(SERIAL_DISPLAY_CS_PIN, SERIAL_DISPLAY_DC_PIN, SERIAL_DISPLAY_RST_PIN, SERIAL_DISPLAY_BUSY_PIN)
);

/* MENU DISPLAY */
const int MENU_DISPLAY_ROTATION = 3;
Adafruit_ST7789 menuDisplay = Adafruit_ST7789(MENU_DISPLAY_CS_PIN, MENU_DISPLAY_DC_PIN, MENU_DISPLAY_RST_PIN);
int menuCursorPosition = 1;
int selectedMenuPosition = 0;

void setup()
{
  if (debug) {
    Serial.begin(9600);
  }

  seedRandomness();
  //delay(random(100,300));
  seedRandomness();
  initializeRequestPins();
 // initializeSerialDisplay();
 // initializeMenuDisplay();

  pinMode(18, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(18), rotaryEncoderButtonPressed, CHANGE);
  
  setupGame();
  provisionModules();

  Wire.begin();
 // discoverModules();
 // enableModuleInterrupt();

 // displaySerialNumber();
 // delay(5000);
 // blankSerialNumber();
  /*
  propagateSettingsToModules();
  requestReadyStatusFromModules();
  displaySerialNumber();
  */
}

void loop()
{
  doScanForRequests();
}

void seedRandomness() {
  randomSeed(analogRead(RANDOMNESS_SOURCE));
}

void generateSerialNumber(int length) {
  String letterList[] = {"A","B","C","D","E","F","G","H","J","K","L","M","N","P","Q","R","S","T","U","V","W","X","Y","Z","A","B","C","D","F","G","H","J","K","L","M","N","P","Q","R","S","T","V","W","X","Y","Z","1","2","3","4","5","6","7","8","9"};  int listLength = 56;
  String output = "";
  for (int i = 0; i < length; i++) {
    output += letterList[random(0, listLength - 1)];
  }
  serialNumber = output;
}

void initializeRequestPins() {
  for (int i = 0; i < 8; i++) {
    pinMode(REQUEST_PINS[i], INPUT);
  }
}

void discoverModules() {
  Serial.println("Starting I2C Discovery...");
  
  JsonDocument pingAction;
  pingAction["action"] = "ping";
  String pingCommand;
  serializeJson(pingAction, pingCommand);
  
  JsonDocument enableRequestPinAction;
  enableRequestPinAction["action"] = "erp";
  String enableRequestPinCommand;
  serializeJson(enableRequestPinAction, enableRequestPinCommand);
  
  JsonDocument disableRequestPinAction;
  disableRequestPinAction["action"] = "drp";
  String disableRequestPinCommand;
  serializeJson(disableRequestPinAction, disableRequestPinCommand);

  for (byte currentAddress = MODULE_START_ADDRESS; currentAddress <= MODULE_END_ADDRESS; currentAddress++) {
    byte error = sendCommand(currentAddress, pingCommand);

    if (error == 0) {
      sendCommand(currentAddress, enableRequestPinCommand);
      delay(25);

      int pin = findRequestPin();
      if (pin != 0) {
        enableModule(currentAddress, pin);
      }

      sendCommand(currentAddress, disableRequestPinCommand);
      delay(50);
    }
  }

  Serial.println("I2C Scan Complete.");
  Serial.print("Found ");
  Serial.print(ACTIVE_MODULES);
  Serial.println(" moduled");
  for(int i = 0; i < ACTIVE_MODULES; i++) {
    Serial.print("Active Module #");
    Serial.print(i);
    Serial.print(" found @ address 0x");
    Serial.print(MODULE_ADDRESSES[i], HEX);
    Serial.print(" with request pin ");
    Serial.println(ASSIGNED_REQUEST_PINS[i]);
  }
}

int findRequestPin() {
  for (int i = 0; i < 8; i++) {
    if (digitalRead(REQUEST_PINS[i]) == HIGH) {
      return REQUEST_PINS[i];
    }
  }
  return 0;
}

byte sendCommand(byte address, String command) {
  Wire.beginTransmission(address);
  Wire.print(command);
  return Wire.endTransmission();
}

void enableModule(byte i2cAddress, int requestPin) {
  Serial.println("enabled");
  MODULE_ADDRESSES[ACTIVE_MODULES] = i2cAddress;
  ASSIGNED_REQUEST_PINS[ACTIVE_MODULES] = requestPin;
  ACTIVE_MODULES++;
}

void incomingRequest() {
  Serial.print("Interrupt received from pin ");
  Serial.println(findRequestPin());
  scanForRequest = true;
}

void enableModuleInterrupt() {
  pinMode(REQUEST_INTERRUPT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(REQUEST_INTERRUPT_PIN), incomingRequest, RISING);
}

void doScanForRequests() {
  if (scanForRequest) {
    for (int i = 0; i < 8; i++) {
      if (digitalRead(ASSIGNED_REQUEST_PINS[i]) == HIGH) {
        bool keepTransmission = true;
        while(keepTransmission) {
          Wire.requestFrom(MODULE_ADDRESSES[i], 6, false);
          while(Wire.available())
          { 
            char c = Wire.read();
            if (c == '\0') {
              keepTransmission = false;
            }
            if (keepTransmission) {
            Serial.print(c); // TODO
            }
          }
        }
      }
    }
    scanForRequest = false;
  }
}

void initializeSerialDisplay() {
  serialDisplay.init(115200,true,50,false);
  serialDisplay.clearScreen();
}

void displaySerialNumber() {
  serialDisplay.setRotation(SERIAL_DISPLAY_ROTATION);
  serialDisplay.setFullWindow();
  serialDisplay.firstPage();
  serialDisplay.fillScreen(GxEPD_BLACK);

  serialDisplay.setCursor(0,0);
  
  serialDisplay.fillRect(10, 18, 70, 16, GxEPD_WHITE);
  serialDisplay.setTextColor(GxEPD_BLACK);
  serialDisplay.setFont(&FreeMono9pt7b);
  serialDisplay.setCursor(12, 30);
  serialDisplay.print("SERIAL");

  serialDisplay.drawRect(10, 34, 230, 53, GxEPD_WHITE);
  
  serialDisplay.setTextColor(GxEPD_WHITE);
  serialDisplay.setFont(&FreeMonoBold24pt7b);
  serialDisplay.setCursor(11, 72); // 47h 224w
  serialDisplay.print(serialNumber);
  serialDisplay.nextPage();
}

void blankSerialNumber() {
  serialDisplay.fillScreen(GxEPD_BLACK);
  serialDisplay.nextPage();
}

void initializeMenuDisplay() {
  menuDisplay.init(240, 320); // Init ST7789 320x240
  menuDisplay.setRotation(MENU_DISPLAY_ROTATION);
  menuDisplay.fillScreen(ST77XX_BLACK);
}

void displayMenu() {
  menuDisplay.fillRect(0, 0, 20, 240, ST77XX_BLACK);

  menuDisplay.setTextWrap(false);
  menuDisplay.setTextColor(ST77XX_WHITE);
  menuDisplay.setTextSize(2);

  int yOffset = (menuCursorPosition*30) - 30;
  menuDisplay.fillTriangle(10, 8 + yOffset, 18, 16 + yOffset, 10, 24 + yOffset, ST77XX_WHITE);

  menuDisplay.setCursor(30, 10);

  menuDisplay.setTextColor(ST77XX_WHITE);
  if (selectedMenuPosition == 1) {
    menuDisplay.setTextColor(ST77XX_YELLOW);
  }
  menuDisplay.println("Lives:");

  menuDisplay.fillRect(260, 0, 60, 80, ST77XX_BLACK);

  if (baseLives == 3) {
    menuDisplay.fillRect(289, 7, 25, 20, ST77XX_WHITE);
    menuDisplay.setTextColor(ST77XX_BLACK);
  } else {
    menuDisplay.drawRect(289, 7, 25, 20, ST77XX_WHITE);
    menuDisplay.setTextColor(ST77XX_WHITE);
  }
  menuDisplay.setCursor(296, 10);
  menuDisplay.println("3");

  if (baseLives == 1) {
    menuDisplay.fillRect(262, 7, 25, 20, ST77XX_WHITE);
    menuDisplay.setTextColor(ST77XX_BLACK);
  } else {
    menuDisplay.drawRect(262, 7, 25, 20, ST77XX_WHITE);
    menuDisplay.setTextColor(ST77XX_WHITE);
  }
  menuDisplay.setCursor(270, 10);
  menuDisplay.println("1");

  menuDisplay.setCursor(30, 40);
  menuDisplay.setTextColor(ST77XX_WHITE);
  if (selectedMenuPosition == 2) {
    menuDisplay.setTextColor(ST77XX_YELLOW);
  }
  menuDisplay.println("Time:");

  menuDisplay.setTextColor(ST77XX_WHITE);
  menuDisplay.setCursor(262, 40);
  String displayTime = getTimeForDisplay();
  menuDisplay.println(displayTime);

  
  menuDisplay.setCursor(30, 200);
  menuDisplay.println("START");
}

String getTimeForDisplay() {
  int minutes = (int) floor(baseTime / 60);
  int seconds = baseTime - (minutes * 60);
  String output = "";
  output += minutes;
  output += ":";
  output += seconds < 10 ? "0" : "";
  output += seconds;

  return output;
}

void rotaryEncoderButtonPressed() {
  baseTime += 10;
  displayMenu();
}

void testMenu() {
  
  displayMenu();
  delay(500);
  menuCursorPosition = 2;
  displayMenu();
  delay(500);
  menuCursorPosition = 1;
  displayMenu();
  delay(500);
  menuCursorPosition = 2;
  displayMenu();
  delay(500);
  menuCursorPosition = 1;
  displayMenu();
  delay(500);
  selectedMenuPosition = 1;
  displayMenu();
  delay(500);
  baseLives = 1;
  displayMenu();
  delay(500);
  baseLives = 3;
  displayMenu();
  delay(500);
  baseLives = 1;
  displayMenu();
  delay(500);
  selectedMenuPosition = 0;
  displayMenu();
  delay(500);
  menuCursorPosition = 2;
  displayMenu();
  delay(500);
  selectedMenuPosition = 2;
  displayMenu();
  delay(250);
  baseTime -= 10;
  displayMenu();
  delay(250);
  baseTime -= 10;
  displayMenu();
  delay(250);
  baseTime -= 10;
  displayMenu();
  delay(250);
  baseTime -= 10;
  displayMenu();
  delay(250);
  baseTime -= 10;
  displayMenu();
  delay(250);
  baseTime -= 10;
  displayMenu();
  delay(100);
  for (int z = 0; z < 100; z++) {
    baseTime += 10;
    displayMenu();
    delay(80);
  }
}

void setupGame() {
  generateSerialNumber();
  generateLabels();
}

void provisionModules() {
  JsonDocument provision;
  provision["action"] = "provision";
  provision["data"]["serial"] = serialNumber;
  provision["data"]["lives"] = baseLives;
  provision["data"]["time"] = baseTime;
  //provision["data"]["ports"] = bombPorts;
  //provision["data"]["batteries"] = bombLabels;
  for (int i = 0; i < generatedLabelCount; i++) {
    provision["data"]["labels"][i]["label"] = bombLabels[i].label;
    provision["data"]["labels"][i]["lit"] = bombLabels[i].lit;
  }
  
  String provisionCommand;
  serializeJson(provision, provisionCommand);

  Serial.println(provisionCommand);

  
}

void generateLabels() {
  int labelsToGenerate = random(0, maxLabels);
  Serial.println(labelsToGenerate);

  int usedLabels[4] = {-1, -1, -1, -1};
  for (int i = 0; i < labelsToGenerate; i++) {
      int labelIndex = random(0, 11);
      while (intIssetInArray(usedLabels, labelIndex)) {
        labelIndex = random(0, 11);
      }
      String label = possibleLabels[labelIndex];
      bombLabels[i].label = label;
      bombLabels[i].lit = random(0, 1023) % 3 == 1 ? true : false;
      generatedLabelCount++;
  }
}

bool intIssetInArray(int haystack[], int search) {
  int size = 0;
  for (int i = 0; haystack[i]; i++) {
    size++;
  }

  for (int j = 0; j < size; j++) {
    if (haystack[j] == search) {
      return true;
    }
  }
  return false;
}