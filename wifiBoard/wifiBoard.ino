
#include<ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define UDP_PORT 5210
struct Temp {
  float value;
} temp_resource;
ESP8266WebServer http_rest_server(HTTP_REST_PORT);
WiFiUDP udp;

struct config {
  float P;
  float I;
  float D;
  float setpoint;
  float fan;
  boolean on;
} cfg_resource;
enum MSG {
  DATA = 0
};

#ifdef ESP32
#define BAUD_RATE 57600
#else
#define BAUD_RATE 57600
#endif
void init_temp_resource()
{
  temp_resource.value = 99999;
}
byte readFromArduino(char *msg, byte numChars, boolean noWait) {
  char rc = '\0';
  char endMarker = '\n';
  byte ndx = 0;
  boolean newData = false;
  unsigned long startTime = millis();
  while ( newData == false) {
    while (Serial.available() > 0 ) {
      rc = Serial.read();

      if (rc != endMarker) {
        msg[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else {
        msg[ndx] = '\0'; // terminate the string
        newData = true;
      }
    }
    // If I don't receive the new message within
    // 10ms, bail.
    if (millis() - startTime > 50) {
      ndx = 0;
      msg[0] = '\0';
      newData = true;
    }
  }
  return ndx;
}
float get_temp_from_board(){
  StaticJsonDocument<JSON_OBJECT_SIZE(1)> doc;
  char buf[64];
  doc["command"] = "GET TEMP";
  size_t bytesWritten = serializeJson(doc, buf, sizeof(buf));
  //buf[bytesWritten] = '\n';
  Serial.println(buf);
  char msg[64];
  size_t numRead = readFromArduino(msg, 64,false);
  Serial.print("Num Read: ");
  Serial.println(numRead);
  float temp = atof(msg);
  return temp;
}
void get_temp() {
  float temp = get_temp_from_board();
  temp_resource.value = temp;

  StaticJsonDocument<40> jsonDocument;
  char JSONMessageBuffer[64];
  jsonDocument["value"] = temp_resource.value;
  serializeJsonPretty(jsonDocument, JSONMessageBuffer, sizeof(JSONMessageBuffer));
  http_rest_server.send(200, "application/json", JSONMessageBuffer);
}
void get_config() {
  StaticJsonDocument<200> doc;
  char buf[200];
  doc["command"] = "GET CONFIG";
  size_t bytesWritten = serializeJson(doc, buf, sizeof(buf));
  buf[bytesWritten] = '\n';
  Serial.write(buf);
  char msg[128];
  size_t numRead = readFromArduino(msg, 128,false);
  http_rest_server.send(200, "application/json", msg);
}
void json_to_resource(JsonObject& jsonBody) {
  int id, gpio, value;
  value = jsonBody["value"];
  Serial.println(value);
  temp_resource.value = value;
}

void set_config() {
  String body = http_rest_server.arg("plain");
  http_rest_server.send(200,"text/plain","OK");
  Serial.println(body);
}

void config_rest_server_routing() {
  http_rest_server.on("/", HTTP_GET, []() {
    http_rest_server.send(200, "text/html",
                          "Good morning, Kyle.");
  });
  http_rest_server.on("/temps", HTTP_GET, get_temp);
  http_rest_server.on("/config", HTTP_GET, get_config);
  http_rest_server.on("/set", HTTP_POST, set_config);
}
void setup() {
  Serial.begin(BAUD_RATE);
  //swSer.begin(115200);
  Serial.println();
  WiFi.begin("thewardrobe", "turkishdelight");
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  udp.begin(UDP_PORT);
  //Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), UDP_PORT);


  config_rest_server_routing();

  http_rest_server.begin();

}
char buf[128];
byte idx = 0;
char incomingPacket[255];  // buffer for incoming packets
char  replyPacket[] = "Hi there! Got the message :-)";  // a reply string to send back
void loop() {
  http_rest_server.handleClient();
  char msg[128];
  size_t numRead = readFromArduino(msg, 128, true);
  if ( numRead > 0 ){
    Serial.print("Sending message: ");
    Serial.println(msg);
    udp.beginPacket("192.168.2.63", 5210);
    udp.write(msg);
    udp.endPacket();
  }

  int packetSize = udp.parsePacket();
  if ( packetSize ){
    // receive incoming UDP packets
    Serial.printf("Received %d bytes from %s, port %d\n", packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
    int len = udp.read(incomingPacket, 255);
    if (len > 0)
    {
      incomingPacket[len] = 0;
    }
    Serial.printf("UDP packet contents: %s\n", incomingPacket);

    // send back a reply, to the IP address and port we got the packet from
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(replyPacket);
    udp.endPacket();
  }
}
