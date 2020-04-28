#include <SoftwareSerial.h>
#include <ArduinoJson.h>

#include <PID_v1.h>

#include <Adafruit_MAX31856.h>

// Use software SPI: CS, DI, DO, CLK

#define SSR 4
#define MOTOR 3
#define DRDY 2
#define BAUD_RATE 57600
#define MAX_TEMP 270
Adafruit_MAX31856 maxthermo = Adafruit_MAX31856(5, 6, 7, 8);
// use hardware SPI, just pass in the CS pin
//Adafruit_MAX31856 maxthermo = Adafruit_MAX31856(5);
//pipeline to talk to the esp8266
SoftwareSerial swSer(14, 15);
unsigned long myTime = 0;
const size_t CAPACITY = JSON_OBJECT_SIZE(6);
StaticJsonDocument<CAPACITY> doc;
JsonObject cfgDoc = doc.to<JsonObject>();
struct Config {
  double P;
  double I;
  double D;
  double setpoint;
  double fan;
  boolean on;
};
Config cfg;
double setpoint, input, output;
PID myPID(&input, &output, &(cfg.setpoint), 0, 0, 0, DIRECT);
void setup() {
  Serial.begin(BAUD_RATE);
  swSer.begin(BAUD_RATE);
  pinMode(SSR, OUTPUT);
  pinMode(MOTOR, OUTPUT);
  digitalWrite(SSR, LOW);
  digitalWrite(MOTOR, LOW);
  Serial.print("increasing speed\n");
  //    for (int i = 0; i < 255; i++) {
  //      analogWrite(MOTOR, i);
  //      delay(50);
  //    }
  Serial.println("MAX31856 thermocouple test");
  maxthermo.begin();
  maxthermo.setThermocoupleType(MAX31856_TCTYPE_K);
  maxthermo.Config();

  myPID.SetOutputLimits(0, 1.0);
  myPID.SetMode(AUTOMATIC);
  myPID.SetSampleTime(100);

  Serial.print("Thermocouple type: ");
  switch (maxthermo.getThermocoupleType() ) {
    case MAX31856_TCTYPE_B: Serial.println("B Type"); break;
    case MAX31856_TCTYPE_E: Serial.println("E Type"); break;
    case MAX31856_TCTYPE_J: Serial.println("J Type"); break;
    case MAX31856_TCTYPE_K: Serial.println("K Type"); break;
    case MAX31856_TCTYPE_N: Serial.println("N Type"); break;
    case MAX31856_TCTYPE_R: Serial.println("R Type"); break;
    case MAX31856_TCTYPE_S: Serial.println("S Type"); break;
    case MAX31856_TCTYPE_T: Serial.println("T Type"); break;
    case MAX31856_VMODE_G8: Serial.println("Voltage x8 Gain mode"); break;
    case MAX31856_VMODE_G32: Serial.println("Voltage x8 Gain mode"); break;
    default: Serial.println("Unknown"); break;
  }
  if ( cfg.on ) {
    analogWrite(MOTOR, round(cfg.fan * 255));
  }
  cfgDoc["P"] = 0.005;
  cfgDoc["I"] = 0.001;
  cfgDoc["D"] = 0.0;
  cfgDoc["setpoint"] = 200;
  cfgDoc["fan"] = 1.0;
  cfgDoc["on"] = false;

  cfg.P = cfgDoc["P"];
  cfg.I = cfgDoc["I"];
  cfg.D = cfgDoc["D"];
  cfg.setpoint = cfgDoc["setpoint"];
  cfg.fan = cfgDoc["fan"];
  cfg.on = cfgDoc["on"];

  myPID.SetTunings(cfg.P, cfg.I, cfg.D);
}

int toggle = 1;
unsigned long controlTime = 0;
unsigned long windowSize = 200;
const byte numChars = 128;
char msg[numChars];
char inChar;
int counter = 0;
boolean shutdown = false;
boolean newData = false;
unsigned long nextCycleTime = 0;
size_t read() {
  size_t numRead = 0;
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;
  while (swSer.available() > 0) {
    rc = swSer.read();

    if (rc != endMarker) {
      msg[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    }
    else {
      msg[ndx] = '\0'; // terminate the string
      ndx = 0;
      newData = true;
    }
    if (newData) {
      Serial.print("ESP: ");
      Serial.println(msg);
      handleMessage(msg);
      newData = false;
    }
  }
  return numRead;
}
void handleMessage(char *msg) {
  Serial.print("What I received: ");
  Serial.println(msg);
  StaticJsonDocument<100> doc;
  deserializeJson(doc, msg);
  Serial.println("deserialized json");
  if (strcmp(doc["command"], "GET TEMP") == 0) {
    Serial.println("received temperature request");
    swSer.println(input);
  }
  else if (strcmp(doc["command"], "GET CONFIG") == 0) {
    Serial.println("received config request");
    char JSONMessageBuffer[200];
    cfgDoc["P"] = cfg.P;
    cfgDoc["I"] = cfg.I;
    cfgDoc["D"] = cfg.D;
    cfgDoc["setpoint"] = cfg.setpoint;
    cfgDoc["fan"] = cfg.fan;
    cfgDoc["on"] = cfg.on;
    serializeJson(cfgDoc, JSONMessageBuffer, sizeof(JSONMessageBuffer));
    Serial.println("what I want to send back");
    Serial.println(JSONMessageBuffer);
    swSer.println(JSONMessageBuffer);
  }
  else if (strcmp(doc["command"], "SET CONFIG") == 0) {
    Serial.println("received config command");

    cfgDoc["P"] = doc["P"];
    cfgDoc["I"] = doc["I"];
    cfgDoc["D"] = doc["D"];
    cfgDoc["setpoint"] = doc["setpoint"];
    cfgDoc["on"] = doc["on"];
    cfgDoc["fan"] = doc["fan"];
    cfg.P = cfgDoc["P"];
    cfg.I = cfgDoc["I"];
    cfg.D = cfgDoc["D"];
    cfg.setpoint = cfgDoc["setpoint"];
    cfg.fan = cfgDoc["fan"];
    cfg.on = cfgDoc["on"];
    //char mybuff[128];
    //sprintf(mybuff,"Config is now: P -> %f I -> %f D -> %f setpoint -> %f fan -> %f on -> %d\n",
    //cfg.P, cfg.I, cfg.D, cfg.setpoint, cfg.fan, cfg.on);
    //Serial.print(mybuff);
    myPID.SetTunings(cfg.P, cfg.I, cfg.D);
    if ( cfg.on && cfg.fan == 0 ) {
      cfg.fan = 0.5;
      shutdown = false;
    }
    Serial.println("Value of on bit: ");
    Serial.println(cfg.on);
    analogWrite(MOTOR, round(cfg.fan * 255));
  }
  else {
    //swSer.write();
  }

}
void loop() {
  myTime = millis();
  //Serial.println(myTime);
  read();
  // Check and print any faults
  if ( myTime > nextCycleTime ) {
    counter++;
    input = maxthermo.readThermocoupleTemperature() * 1.8 + 32;
    if ( counter % 50 == 0) {
      Serial.print(myTime);
      Serial.print(": Thermocouple Temp: ");
      Serial.println(input);
      counter = 0;
      swSer.println(input);
      Serial.print("output: ");
      Serial.println(output);
    }
    if ( inBadState(maxthermo.readFault()) ||
         !cfg.on) {
      if ( !shutdown) {
        Serial.println("Shutting down: ");
        turnOffSystem();
      }
    } else {

      //Serial.println("computing PID");
      myPID.Compute();
    }
    nextCycleTime = myTime + 100 - millis() % 100;
    if ( false ) {
      Serial.print(myTime);
      Serial.print(": ");
      Serial.print("next cycle time: ");
      Serial.println(nextCycleTime);
    }
  }

  controlRelay(output, windowSize, &controlTime);
}

void waitTillNextCycle(unsigned long samplePeriod) {
  long waitTime = samplePeriod - (millis() % samplePeriod);
  if ( waitTime > 0) {
    delay(waitTime);
  }
}

void controlRelay(double output, unsigned long windowSize, unsigned long *controlTime) {
  unsigned long myTime = millis();
  if (myTime - *controlTime > windowSize) {
    //time to shift the Relay Window
    *controlTime += windowSize;
  }
  if (output * windowSize > myTime - *controlTime){
    digitalWrite(SSR, HIGH);
  }
  else{
    digitalWrite(SSR, LOW);
  }
}
//
boolean turnOffSystem() {
  if ( !shutdown ) {
    Serial.println("System is shutting down!");
  }
  shutdown = true;
  if ( input > 120 ) {
    Serial.println("Temperature is too high, setting fan on high");
    // run fan full blast to cool
    // it off, and set the SSR window
    // to zero, effectively turning it off
    output = 0.0;
    cfg.fan = 1.0;
    analogWrite(MOTOR, round(cfg.fan * 255));
  }
  else {
    // ensure the whole thing is
    // spun down.
    output = 0.0;
    cfg.fan = 0.0;
    analogWrite(MOTOR, round(cfg.fan * 255));
  }
}
boolean inBadState(uint8_t fault) {
  boolean error = false;
  if (fault) {
    if (fault & MAX31856_FAULT_CJRANGE) Serial.println("Cold Junction Range Fault");
    if (fault & MAX31856_FAULT_TCRANGE) Serial.println("Thermocouple Range Fault");
    if (fault & MAX31856_FAULT_CJHIGH)  Serial.println("Cold Junction High Fault");
    if (fault & MAX31856_FAULT_CJLOW)   Serial.println("Cold Junction Low Fault");
    if (fault & MAX31856_FAULT_TCHIGH)  Serial.println("Thermocouple High Fault");
    if (fault & MAX31856_FAULT_TCLOW)   Serial.println("Thermocouple Low Fault");
    if (fault & MAX31856_FAULT_OVUV)    Serial.println("Over/Under Voltage Fault");
    if (fault & MAX31856_FAULT_OPEN)    Serial.println("Thermocouple Open Fault");
    error = true;
  }
  // Flag error mode if the temp is greater than 300 celsius
  if (input > 300.0) {
    Serial.print("System is overheating! Temp is ");
    Serial.println(input);
    error = true;
  }
  // don't let the duty cycle be greater than zero
  // when the fan is basically off
  if (output > 0.0 && cfg.fan < 0.3 ) {
    error = true;
    Serial.println("Roaster is on but fan is too low!");
  }

  return error;
}
