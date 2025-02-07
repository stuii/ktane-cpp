#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>

#define I2C_ADDRESS 0x49  // Module's unique I2C address
String MODULE_TYPE = "TEST";
bool IS_NEEDY = true;

/* METHOD DEFINITIONS */
void provisionModule(JsonObject input);
void receiveMessage(int howMany);
void answerRequest();
void prepareIdent();
void sendReady();
void sendMistake();
void sendSolved();
void clockTick();

struct Label {
  String label;
  bool lit;
};

const int REQUEST_PIN = 4;
const int CLOCK_PIN = 2;
volatile int chunkIndex = 0;
int lastChunk = 0;

/*
 * 1 = boot
 * 2 = wait for provision
 * 3 = game setup & send ready
 * 4 = game
 */
int globalState = 1;

// Variables will change:
const int buttonPin = 8;
int btnState = LOW;
String receivedCommand = "";

String waitingCommand = "";
bool readyPrepared = false;

/* BASE SETTINGS */
String serialNumber;
int baseLives;
int baseTime;
int batteryCountAA;
int batteryCountD;
int portCountVGA;
int portCountRJ45;
int portCountRCA;
int portCountPS2;
Label bombLabels[4] = {};
int labelCount = 0;

/* GAME VARS */
volatile int currentTime = -1;
int currentLives = -1;

void setup() {
  Serial.begin(9600);
  pinMode(REQUEST_PIN, OUTPUT);
  digitalWrite(REQUEST_PIN, LOW);
  delay(1000);
  Wire.begin(I2C_ADDRESS);  // Join I2C bus as slave
  Wire.onReceive(receiveMessage);  // Register callback for when master requests data
  Wire.onRequest(answerRequest);  // Register callback for when master requests data

  //pinMode(buttonPin, INPUT);
  globalState = 2;
}

void loop() {
  switch (globalState) {
    case 1:

    break;
    case 2:
      
    break;
    case 3:
      // setup game
      delay(5000);
      sendReady();

      // enable clock Tick
      pinMode(CLOCK_PIN, INPUT);
      attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), clockTick, RISING);
    break;
    case 4:
      delay(10000);
      sendMistake();
    break;
  }
  /*btnState = digitalRead(buttonPin);
  if(btnState == HIGH)
  {
    delay(500);
    btnState = digitalRead(buttonPin);
    if(btnState == HIGH)
    {
    digitalWrite(REQUEST_PIN, HIGH);
    }
  }*/
}

void receiveMessage(int howMany) {
  String input = "";
  while (Wire.available()) { // peripheral may send less than requested
    input += (char) Wire.read(); // receive a byte as character
  }
  if (input == "\0") {
    Serial.println(receivedCommand);
    JsonDocument doc; 
    deserializeJson(doc, receivedCommand);

    const String action = doc["action"];

    if (action == "ping") {
      Wire.write("{\"ack\":true}");
    } else if (action == "erp") {
      digitalWrite(REQUEST_PIN, HIGH);
      Wire.write("{\"ack\":true}");
    } else if (action == "drp") {
      digitalWrite(REQUEST_PIN, LOW);
      Wire.write("{\"ack\":true}");
    } else if (action == "ident") {
      prepareIdent();
    } else if (action == "provision") {
      provisionModule(doc["data"].as<JsonObject>());
    }
    receivedCommand = "";
  } else {
    receivedCommand += input;
  }
}

void answerRequest() {
  // Ensure waitingCommand has content
  if (waitingCommand.length() == 0) {
    Wire.write(""); // Send an empty response if no message is prepared
    return;
  }

  // Convert String to char array
  static char message[128]; // Adjust size as needed for the longest expected message
  waitingCommand.toCharArray(message, waitingCommand.length() + 1); // +1 for null terminator

  // Calculate the number of chunks
  int totalChunks = (waitingCommand.length() + 5) / 6; // Round up to include partial chunks

  // Send the current chunk
  Wire.write(message + (chunkIndex * 6), min(6, waitingCommand.length() - (chunkIndex * 6)));

  // Update chunk index
  chunkIndex = (chunkIndex + 1) % totalChunks;

  // Reset the request pin only after the last chunk
  if (chunkIndex == 0) {
    Wire.write("\0");
    digitalWrite(REQUEST_PIN, LOW);
    waitingCommand = ""; // Clear the message after the last chunk
    Serial.println("sent command");
  }
}

void provisionModule(JsonObject input) {
  serialNumber = input["serial"].as<String>();
  baseLives = input["lives"];
  baseTime = input["time"];
  randomSeed(input["seed"]);
  batteryCountAA = input["batteries"]["AA"];
  batteryCountD = input["batteries"]["D"];
  portCountVGA = input["ports"]["VGA"];
  portCountRJ45 = input["ports"]["RJ45"];
  portCountRCA = input["ports"]["RCA"];
  portCountPS2 = input["ports"]["PS2"];
  
  // todo: ports
  if (input.containsKey("labels")) {
    JsonArray labels = input["labels"].as<JsonArray>();
    for (size_t i = 0; i < labels.size(); i++) {
      JsonObject labelObj = labels[i].as<JsonObject>();
      bombLabels[i].label = labelObj["label"].as<String>();
      bombLabels[i].lit = labelObj["lit"];
      labelCount++;
    }
  }

  currentLives = baseLives;
  currentTime = baseTime + 1;

  Serial.println("Module provisioned");

  globalState = 3;
}

void prepareIdent() {
  JsonDocument identDocument;
  identDocument["action"] = "ident";
  identDocument["isNeedy"] = IS_NEEDY;
  identDocument["type"] = MODULE_TYPE;
  
  String identCommand;
  serializeJson(identDocument, identCommand);

  waitingCommand = identCommand;
}

void sendReady()
{
  if (!readyPrepared) {
    JsonDocument readyDocument;
    readyDocument["action"] = "ready";
    
    String readyCommand;
    serializeJson(readyDocument, readyCommand);

    waitingCommand = readyCommand;
    digitalWrite(REQUEST_PIN, HIGH);
    readyPrepared = true;

    Serial.println("Ready prepared");
  }
}

void sendMistake()
{
  JsonDocument mistakeDocument;
  mistakeDocument["action"] = "mistake";
  
  String mistakeCommand;
  serializeJson(mistakeDocument, mistakeCommand);

  waitingCommand = mistakeCommand;
  digitalWrite(REQUEST_PIN, HIGH);
  Serial.println("started request");
}

void sendSolved()
{
  JsonDocument solvedDocument;
  solvedDocument["action"] = "solved";
  
  String solvedCommand;
  serializeJson(solvedDocument, solvedCommand);

  waitingCommand = solvedCommand;
  digitalWrite(REQUEST_PIN, HIGH);
}

void clockTick()
{
  globalState = 4;
  currentTime--;
}