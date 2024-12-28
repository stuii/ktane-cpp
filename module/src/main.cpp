#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>

#define I2C_ADDRESS 0x69  // Module's unique I2C address

/* METHOD DEFINITIONS */
void provisionModule(JsonObject input);
void receiveMessage(int howMany);
void answerRequest();


struct Label {
  String label;
  bool lit;
};

const int REQUEST_PIN = 4;
volatile int chunkIndex = 0;

// Variables will change:
const int buttonPin = 8;
int btnState = LOW;
String receivedCommand = "";


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

void setup() {
  Serial.begin(9600);
  pinMode(REQUEST_PIN, OUTPUT);
  delay(1000);
  Wire.begin(I2C_ADDRESS);  // Join I2C bus as slave
  Wire.onReceive(receiveMessage);  // Register callback for when master requests data
  Wire.onRequest(answerRequest);  // Register callback for when master requests data

  pinMode(buttonPin, INPUT);
    digitalWrite(REQUEST_PIN, LOW);
    
}

void loop() {
  
  btnState = digitalRead(buttonPin);
  if(btnState == HIGH)
  {
    delay(500);
    btnState = digitalRead(buttonPin);
    if(btnState == HIGH)
    {
    digitalWrite(REQUEST_PIN, HIGH);
    }
  }
}

void receiveMessage(int howMany) {
  String input = "";
  while (Wire.available()) { // peripheral may send less than requested
    input += (char) Wire.read(); // receive a byte as character
  }
  if (input == '\0') {
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
    } else if (action == "provision") {
      provisionModule(doc["data"].as<JsonObject>());
    }
    receivedCommand = "";
  } else {
    receivedCommand += input;
  }
}

void answerRequest() {
  char message[] = "{\"action\"\:\"life\"}\0";
  Wire.write(message + (chunkIndex * 6), 6);
  chunkIndex = (chunkIndex + 1) % static_cast <int> (floor(sizeof(message) / 6));
    digitalWrite(REQUEST_PIN, LOW);
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
    for (int i = 0; i < labels.size(); i++) {
      JsonObject labelObj = labels[i].as<JsonObject>();
      bombLabels[i].label = labelObj["label"].as<String>();
      bombLabels[i].lit = labelObj["lit"];
      labelCount++;
    }
  }

Serial.println(serialNumber);
Serial.println(baseLives);
Serial.println(baseTime);
Serial.println(batteryCountAA);
Serial.println(batteryCountD);
Serial.println(portCountVGA);
Serial.println(portCountRJ45);
Serial.println(portCountRCA);
Serial.println(portCountPS2);
for (int i = 0; i < labelCount; i++) {
  Serial.print(bombLabels[i].label);
  Serial.println(bombLabels[i].lit);
}
}