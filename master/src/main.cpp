#include "Arduino.h"
#include <Wire.h>
#include <ArduinoJson.h>
#include <stdlib.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <RotaryEncoder.h>
#include <TimerOne.h>
#include <TimerFive.h>
#include <SD.h>
#include <TMRpcm.h>

/*
 * TODOS:
 * - SD Card
 * - PCM speaker
 * - Battery Display
 * - Ports Display
 * -> external clock (with piezo)
 */

struct Label {
  String label;
  bool lit;
};

void generateSerialNumber(int length = 8);
void initializeSerialDisplay();
void displaySerialNumber();
void blankSerialNumber();
void initializeLabelLEDs();
void initializeMistakeLEDs();
void enableMistakeLED();

void initializeClock();
void startClock();
void stopClock();

void setupGame();
void provisionModules();
void startGame();

void initializeMenuDisplay();
void displayMenu();
String getTimeForDisplay();
void blankMenuDisplay();
void displayTextOnMenuDisplay(String message);

void initializeLabelDisplays();
void displayLabels();
void checkRotEnc();
void checkRotEncButton();

void seedRandomness();
void doScanForRequests(bool alwaysCheck = false);
void initializeRequestPins();
void discoverModules();
byte sendCommand(byte address, String command);
void broadcastToAllModules(String command);
String readFromModule(int address);
int findRequestPin();
void enableModule(byte i2cAddress, int requestPin);
void enableModuleInterrupt();
void incomingRequest();

void generateLabels();
void generateBatteries();
void generatePorts();
bool intIssetInArray(int haystack[], int search);

bool checkReady();
void checkSolved();
void checkMistakes();
void markModuleAsSolved(int moduleId);
void addMistakeFromModule(int moduleId);

#define MODULE_START_ADDRESS 0x00  // Starting I2C address
#define MODULE_END_ADDRESS 0x7F    // Ending I2C address
const int REQUEST_PINS[] = { /*23,*/ 24, 25, 26, 27, 28, 29, 30, 31/*, 32, 33*/ };
int LABEL_PINS[] = { 11, 12, 14, 16 };
int LABEL_LED_PINS[] = { 36, 37, 38, 39 };
Adafruit_ST7735* LABEL_DISPLAYS[4] = {};
int LABEL_LEDS[4] = {};

int ACTIVE_MODULES = 0;
int MODULE_COUNT = 8;
int MODULE_ADDRESSES[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int ASSIGNED_REQUEST_PINS[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
bool SOLVED_MODULES[] = { true, true, true, true, true, true, true, true, true, true, true };
bool NEEDY_MODULES[] = { false, false, false, false, false, false, false, false, false, false, false };
bool READY_MODULES[] = { true, true, true, true, true, true, true, true, true, true, true };
String MODULE_TYPES[] = { "", "", "", "", "", "", "", "", "", "", "" };
int MISTAKE_TRACE[] = { -1, -1, -1 };
int randomnessSeed = 0;

const bool debug = true;
bool scanForRequest = false;

/* PIN DEFINITIONS */
const uint8_t RANDOMNESS_SOURCE = A1;
const int REQUEST_INTERRUPT_PIN = 3;
const int MENU_DISPLAY_CS_PIN = 46;
const int MENU_DISPLAY_RST_PIN = 44;
const int MENU_DISPLAY_DC_PIN = 5;
const int SERIAL_DISPLAY_CS_PIN = 10; // default 5
const int SERIAL_DISPLAY_RST_PIN = 2; // default 2
const int SERIAL_DISPLAY_DC_PIN = 5; // default 0
const int SERIAL_DISPLAY_BUSY_PIN = 15; // default 15
const int SERIAL_DISPLAY_WIDTH = 250; // SSD1680
const int SERIAL_DISPLAY_HEIGHT = 122; // DEPG0213BN
const int ROTENC_BTN = 19;
const int ROTENC_A = 22;
const int ROTENC_B = 23;
const int CLOCK_PIN = 45;
const int MISTAKE_A_LED = 32;
const int MISTAKE_B_LED = 33;

/* GAME BOUNDARIES */
// Ports
const int maxPortsTotal = 5;
const int maxPortsPerType = 2;
const String possiblePorts[] = {"VGA", "PS2", "RJ45", "RCA"};

// Labels
const int maxLabels = 4;
const String possibleLabels[] = {"SND", "CLR", "CAR", "IND", "FRQ", "SIG", "NSA", "MSA", "TRN", "BOB", "FRK"};

// Batteries
const int maxBatteriesTotal = 6;
const int maxBatteriesAA = 6;
const int maxBatteriesD = 2;
const String possibleBatteryTypes[] = {"AA", "D"};


/* GAME SETTINGS */
int baseLives = 3;
int currentLives = -1;
int baseTime = 480;
int currentTime = -1;
String gameResult = "";
String serialNumber = "";
Label bombLabels[maxLabels] = {};
int generatedLabelCount = 0;
int portCountVGA = 0;
int portCountPS2 = 0;
int portCountRJ45 = 0;
int portCountRCA = 0;
int batteryCountAA = 0;
int batteryCountD = 0;

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

/* ROTARY ENCODER */
RotaryEncoder encoder(ROTENC_A, ROTENC_B, RotaryEncoder::LatchMode::FOUR3);
int globalPos = 0;
int rotEncButtonState = HIGH;            // the current reading from the input pin
int rotEncLastButtonState = HIGH;  // the previous reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long rotEncLastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long rotEncDebounceDelay = 50;    // the debounce time; increase if the output flickers

int globalState = 1;
/*
 * 1  boot
 * 2  menu / settings
 * 3  provisioning
 * 4  wait for ready
 * 5  countdown
 * 6  enable displays
 * 7  game
 * 8  game ended
 */

void setup()
{
  if (debug) {
    Serial.begin(9600);
  }
  seedRandomness();
  initializeMenuDisplay();
  displayTextOnMenuDisplay("Please wait...");
  initializeRequestPins();
  //initializeSerialDisplay();
  initializeLabelLEDs();
  initializeMistakeLEDs();
  Wire.begin();
  discoverModules();
  blankMenuDisplay();
  displayMenu();

  checkSolved();

  globalState = 2;
}

void loop()
{
  switch (globalState) {
    case 1:
      //doScanForRequests();
      break;
    case 2:
      checkRotEncButton();
      checkRotEnc();
      break;
    case 3:
      displayTextOnMenuDisplay("Setting up Game");
      setupGame();
      displayTextOnMenuDisplay("Prov'ing Modules");
      provisionModules();
      initializeClock();
      //initializeLabelDisplays();
      globalState = 4;
      break;
    case 4:
      // if all ready: globalState = 5;
      displayTextOnMenuDisplay("Wating on Modules");
      doScanForRequests(true);
      if (checkReady()) {
        globalState = 5;
      }
      break;
    case 5:
      // if all ready: globalState = 5;
      displayTextOnMenuDisplay("Ready?");
      //displayLabels();
      //displaySerialNumber();
      displayTextOnMenuDisplay("3");
      delay(1000);
      displayTextOnMenuDisplay("2");
      delay(1000);
      displayTextOnMenuDisplay("1");
      delay(1000);
      enableModuleInterrupt();
      startClock();
      globalState = 6;
      break;
    case 6:
      blankMenuDisplay();
      globalState = 7;
      break;
    case 7:
      doScanForRequests();
      break;
    case 8:
      blankSerialNumber();
      displayTextOnMenuDisplay(gameResult);
      globalState = 99;
      break;
  }
}

void seedRandomness() {
  randomnessSeed = analogRead(RANDOMNESS_SOURCE);
  randomSeed(randomnessSeed);
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
  
  JsonDocument identificationAction;
  identificationAction["action"] = "ident";
  String identificationCommand;
  serializeJson(identificationAction, identificationCommand);

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

      sendCommand(currentAddress, identificationCommand);
      delay(100);
      String identData = readFromModule(currentAddress);

      JsonDocument doc; 
      deserializeJson(doc, identData);

      const String moduleType = doc["type"];
      const bool needy = doc["isNeedy"];

      MODULE_TYPES[ACTIVE_MODULES - 1] = moduleType;
      NEEDY_MODULES[ACTIVE_MODULES - 1] = needy;
      READY_MODULES[ACTIVE_MODULES - 1] = false;
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
  int commandLength = command.length();
  for (int i = 0; i < commandLength; i += 32) {
    String chunk = command.substring(i, min(i + 32, commandLength));
      Wire.beginTransmission(address);
      Wire.print(chunk);
      Wire.endTransmission();
      delay(10); // Short delay to ensure the slave can process the data
  }
  Serial.print("Sent command to 0x");
  Serial.println(address, HEX);
  Wire.beginTransmission(address);
  Wire.print('\0');
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

void doScanForRequests(bool alwaysCheck) {
  if (scanForRequest || alwaysCheck) {
    for (int i = 0; i < 8; i++) {
      if (digitalRead(ASSIGNED_REQUEST_PINS[i]) == HIGH) {
        String command = readFromModule(MODULE_ADDRESSES[i]);
        
        JsonDocument doc; 
        deserializeJson(doc, command);

        const String action = doc["action"];

        if (action == "solved") {
          markModuleAsSolved(i);
        } else if (action == "mistake") {
          addMistakeFromModule(i);
        } else if (action == "ready") {
          READY_MODULES[i] = true;
        }
      }
    }
    scanForRequest = false;
  }
}

String readFromModule(int address)
{
  String returnValue = "";
  bool keepTransmission = true;
  while(keepTransmission) {
    Wire.requestFrom(address, 6, false);
    while(Wire.available())
    { 
      char c = Wire.read();
      if (c == '\0') {
        keepTransmission = false;
      }
      if (keepTransmission) {
        returnValue += c;
      }
    }
  }
  return returnValue;
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
  pinMode(ROTENC_BTN, INPUT);
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
  if (menuCursorPosition == 3) {
    yOffset = 190;
  }
  menuDisplay.fillTriangle(10, 8 + yOffset, 18, 16 + yOffset, 10, 24 + yOffset, ST77XX_WHITE);

  menuDisplay.setCursor(30, 10);

  menuDisplay.setTextColor(ST77XX_WHITE);
  if (selectedMenuPosition == 1) {
    menuDisplay.setTextColor(ST77XX_YELLOW);
  }
  menuDisplay.println("Lives:");

  menuDisplay.fillRect(240, 0, 80, 80, ST77XX_BLACK);

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
  if (baseTime >= 600) {
    menuDisplay.setCursor(250, 40);
  } else {
    menuDisplay.setCursor(262, 40);
  }
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


void setupGame() {
  generateSerialNumber();
  generateLabels();
  generateBatteries();
  generatePorts();
  currentLives = baseLives;
  currentTime = baseTime + 1;
}

void provisionModules() {
  JsonDocument provision;
  provision["action"] = "provision";
  provision["data"]["serial"] = serialNumber;
  provision["data"]["lives"] = baseLives;
  provision["data"]["time"] = baseTime;
  provision["data"]["seed"] = randomnessSeed;
  provision["data"]["ports"]["VGA"] = portCountVGA;
  provision["data"]["ports"]["RJ45"] = portCountRJ45;
  provision["data"]["ports"]["RCA"] = portCountRCA;
  provision["data"]["ports"]["PS2"] = portCountPS2;
  provision["data"]["batteries"]["AA"] = batteryCountAA;
  provision["data"]["batteries"]["D"] = batteryCountD;
  for (int i = 0; i < generatedLabelCount; i++) {
    provision["data"]["labels"][i]["label"] = bombLabels[i].label;
    provision["data"]["labels"][i]["lit"] = bombLabels[i].lit;
  }
  
  String provisionCommand;
  serializeJson(provision, provisionCommand);

  broadcastToAllModules(provisionCommand);

  Serial.println(provisionCommand);
}

void generateLabels() {
  int labelsToGenerate = random(0, maxLabels);
  labelsToGenerate = 4;
  
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

void generateBatteries() {
  batteryCountAA = random(0, maxBatteriesAA);
  batteryCountD = random(0, min(maxBatteriesTotal - batteryCountAA, maxBatteriesD));
}

void generatePorts() {
  portCountVGA = random(0, maxPortsPerType);
  portCountPS2 = random(0, min(maxPortsPerType, maxPortsTotal - portCountVGA));
  portCountRJ45 = random(0, min(maxPortsPerType, maxPortsTotal - portCountVGA - portCountPS2));
  portCountRCA = random(0, min(maxPortsPerType, maxPortsTotal - portCountVGA - portCountPS2 - portCountRJ45));
}

void broadcastToAllModules(String command) {
  for (int i = 0; i < ACTIVE_MODULES; i++) {
    if (MODULE_ADDRESSES[i] == 0xFF) { continue; }
    sendCommand(MODULE_ADDRESSES[i], command);
  }
}
void initializeLabelDisplays() {
  const size_t n = sizeof(LABEL_PINS) / sizeof(LABEL_PINS[0]);

  for (size_t i = 0; i < n - 1; i++)
  {
      size_t j = random(0, n - i);
      int t = LABEL_PINS[i];
      int ledT = LABEL_LED_PINS[i];
      LABEL_PINS[i] = LABEL_PINS[j];
      LABEL_LED_PINS[i] = LABEL_LED_PINS[j];
      LABEL_PINS[j] = t;
      LABEL_LED_PINS[j] = ledT;
}

  for (int i = 0; i < 4; i++) {
    LABEL_DISPLAYS[i] = new Adafruit_ST7735(LABEL_PINS[i], /* DC */ 6, /* MOSI */ 7, /* SCK */ 8, /* RST */ 9);
    LABEL_DISPLAYS[i]->initR(INITR_MINI160x80_PLUGIN);
    LABEL_DISPLAYS[i]->setRotation(3);
    LABEL_DISPLAYS[i]->fillScreen(ST7735_WHITE);
  }
}
void displayLabels() {
  for (int i = 0; i < 4; i++) {
    LABEL_DISPLAYS[i]->setTextWrap(false);
    LABEL_DISPLAYS[i]->setTextSize(7);
    LABEL_DISPLAYS[i]->setTextColor(ST77XX_BLACK);
    LABEL_DISPLAYS[i]->setCursor(20, 20);
    if (generatedLabelCount >= i) {
      LABEL_DISPLAYS[i]->println(bombLabels[i].label);
      if (bombLabels[i].lit) {
        digitalWrite(LABEL_LED_PINS[i], HIGH);
      }
    }
  }
}
  
void checkRotEnc() {
  encoder.tick();
  int newPos = encoder.getPosition();
  if (globalPos != newPos) {
    int direction = (globalPos > newPos ? -1 : 1);
    if (selectedMenuPosition == 0) {
      int newCursorPosition = menuCursorPosition + direction;
      if (newCursorPosition <= 1) {
        menuCursorPosition = 1;
      } else if (newCursorPosition >= 3) {
        menuCursorPosition = 3;
      } else {
        menuCursorPosition = newCursorPosition;
      }
    } else if (selectedMenuPosition == 1) {
      if (baseLives == 1 && direction == 1) {
        baseLives = 3;
      } else if (baseLives == 3 && direction == -1) {
        baseLives = 1;
      }
    } else if (selectedMenuPosition == 2) {
      if (direction == -1) {
        baseTime -= 30;
      } else if (direction == 1) {
        baseTime += 30;
      }
      if (baseTime <= 60) {
        baseTime = 60;
      }
      if (baseTime >= 900) {
        baseTime = 900;
      }

    }
    globalPos = newPos;
    displayMenu();
  }
}

void checkRotEncButton() {
   int reading = digitalRead(ROTENC_BTN);
  if (reading != rotEncLastButtonState) {
    rotEncLastDebounceTime = millis();
  }

  if ((millis() - rotEncLastDebounceTime) > rotEncDebounceDelay) {
    if (reading != rotEncButtonState) {
      rotEncButtonState = reading;
      if (rotEncButtonState == LOW) {
        if (selectedMenuPosition == 0) {
        selectedMenuPosition = menuCursorPosition;
        } else {
          selectedMenuPosition = 0;
        }
        if (selectedMenuPosition == 3) {
          globalState = 3;
          menuDisplay.fillRect(0, 0, 320, 240, ST77XX_BLACK);
        } else {
          displayMenu();
        }
      }
    }
  }
  rotEncLastButtonState = reading;
}

void blankMenuDisplay()
{
  menuDisplay.fillRect(0, 0, 320, 240, ST77XX_BLACK);
}

void displayTextOnMenuDisplay(String message)
{
  int messageLength = message.length();
  int letterWidth = 18;

  menuDisplay.fillRect(0, 0, 320, 240, ST77XX_BLACK);
  menuDisplay.setTextWrap(false);
  menuDisplay.setTextColor(ST77XX_WHITE);
  menuDisplay.setTextSize(3);
  menuDisplay.setCursor(((320 - (messageLength * letterWidth)) / 2), 110);

  menuDisplay.setTextColor(ST77XX_WHITE);
  menuDisplay.println(message);
}

void initializeLabelLEDs()
{
  for (int i = 0; i < 4; i++) {
    pinMode(LABEL_LED_PINS[i], OUTPUT);
  }
}

void checkSolved()
{
  bool solved = true;
  for (int i = 0; i < 11; i++) {
    if (!SOLVED_MODULES[i]) {
      solved = false;
    }
  }
  if (solved) {
    gameResult = "success";
    globalState = 8;
  }
}

void initializeClock()
{
  Timer5.initialize(1000000);
}

void startClock()
{
  Timer5.pwm(CLOCK_PIN, 100);
}

void stopClock()
{
  Timer5.stop();
}

void initializeMistakeLEDs()
{
  pinMode(MISTAKE_A_LED, OUTPUT);
  pinMode(MISTAKE_B_LED, OUTPUT);
}

void enableMistakeLED()
{
  int mistakeCount = baseLives - currentLives;
  if (mistakeCount == 1) {
    digitalWrite(MISTAKE_A_LED, HIGH);
  }
  if (mistakeCount == 2) {
    digitalWrite(MISTAKE_B_LED, HIGH);
  }
}

void markModuleAsSolved(int moduleId)
{
  SOLVED_MODULES[moduleId] = true;
  checkSolved();
}

void addMistakeFromModule(int moduleId)
{
  currentLives--;
  int mistakeCount = baseLives - currentLives;

  MISTAKE_TRACE[mistakeCount - 1] = moduleId;
  enableMistakeLED();
  checkMistakes();
}

void checkMistakes()
{
  if (currentLives == 0) {
    gameResult = "failed";
    globalState = 8;
  }
}

bool checkReady()
{
  for (int i = 0; i < 11; i++) {
    if (!READY_MODULES[i]) {
      return false;
    }
  }
  return true;
}