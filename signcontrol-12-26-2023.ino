
// Dependencies:
// esp32 2.0.11 board 
// IRemote library 4.2.0

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include <SD_MMC.h>

#include <ESPmDNS.h>

#include "esp_camera.h"

#include "remote.h"
#include "camera.h"
#include "wifi_config.h"

#define ONBOARD_BTN_PIN 0
#define IR_TX_PIN 13
#define IR_RX_PIN 14

#define IR_SEND_PIN IR_TX_PIN
#define IR_RECEIVE_PIN IR_RX_PIN

#include <IRremote.hpp>

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

WebServer server(80);

void setup()
{
  esp_log_level_set("*", ESP_LOG_VERBOSE);

  setupPlatform();
  setupIr();
  setupWifi();
  setupServer();
  setupCamera();

  Serial.println("\n=setup end");
  Serial.flush();
}

void setupPlatform()
{
  SD_MMC.begin("/sdcard", true); // Disable SD support, we need the pins it would use

  Serial.begin(115200);
  //while (!Serial) {
  //   ; // wait for serial port to connect. Needed for native USB port only
  //}

  Serial.println("\n=setup start");
}

void setupIr()
{
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial.print("Ready to receive IR signals of protocols: ");
  IrReceiver.printActiveIRProtocols(&Serial);
  Serial.printf("\nReceive IR signalsat pin %d\n", IR_RECEIVE_PIN);

  IrSender.begin();
  Serial.printf("Send IR signals at pin %d\n", IR_SEND_PIN);
}

void setupWifi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting to WiFi Network ..");

  while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      delay(250);
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Connect to the same network and visit the above IP in a browser to access the control UI")

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }
}

void setupServer()
{  
  // Configure routes
  server.on("/", handleRoot);
  server.on(UriBraces("/sendIr/{}"), HTTP_GET, handleSendIR);
  server.on("/cam.jpg", HTTP_GET, handleCamImg);
  //server.on("/setMessage", HTTP_POST, handleSetSignMessage);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("WebServer started");
}

void handleRoot()
{
  Serial.println("/ BEGIN");
  server.send(200, "text/html", getPageContent());
  Serial.println("/ END");
}

void handleSendIR()
{
  Serial.println("/sendIr BEGIN");
  String code = server.pathArg(0);
  Serial.printf("Got code '%s'\n", code);
  sendIR(code);
  Serial.println("/sendIr END");
}

void handleCamImg()
{
  Serial.println("/cam.jpg BEGIN");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "image/jpeg", "");

  camera_fb_t* img = getImage();
  server.sendContent_P((const char*)img->buf, img->len);
  finishImage(img);
  
  server.sendContent("");
  server.client().stop(); // Stop is needed because we sent no content length
  Serial.println("/cam.jpg END");
}

void handleNotFound() {
  Serial.println("404 BEGIN");
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  Serial.println("404 END");
}

void loop()
{
  if (IrReceiver.decode())
  {
    Serial.println("Got IR");
    IrReceiver.printIRResultShort(&Serial); 
    IrReceiver.printIRSendUsage(&Serial);
    Serial.println();
    IrReceiver.resume();
  }

  server.handleClient();
  delay(2);//allow the cpu to switch to other tasks
}

void sendIR(String hexStr)
{
  Serial.printf("Value to send: %s\n", hexStr);
  char* endPtr;
  int code = strtol(hexStr.c_str(), &endPtr, HEX);
  Serial.printf("Sending value: %s\n", String(code, HEX));

  sendIR(code);
}

void sendIR(int code)
{
  IrReceiver.stop();
  IrSender.sendNEC(0x35 , code, 1);
  IrReceiver.start();
  delay(KEY_DELAY_MS);
}


char* getPageContent()
{
  return "\
<!DOCTYPE HTML>\
<html>\
<head>\
<style type='text/css'>\
body\
{\
    font-family: sans-serif;\
}\
\
.kb\
{\
    background-color: black;\
    display: grid;\
}\
\
.kb .key\
{\
    color: white;\
    border: 1px solid darkgrey;\
    border-radius: 3px;\
    display: inline-block;\
    padding: 5px;\
    margin: 3px;\
    font-size: 28px;\
    position: relative;\
    cursor: pointer;\
}\
\
.kb .key:hover\
{\
    left: 2px;\
    top: 2px;\
}\
\
.kb .key:active\
{\
    left: 4px;\
    top: 3px;\
    color: grey;\
}\
\
.kb .key .alt\
{\
    color: yellow;\
    display: inline-block;\
    position: relative;\
    bottom: 10px;\
    font-size: 14px;\
}\
\
.camimg\
{\
    width: 800px;\
    height: 600px;\
    border: none;\
}\
\
</style>\
<script>\
function send(key)\
{\
    fetch('/sendIr/' + key);\
}\
\
setInterval(function updateImage()\
{\
    document.querySelector('.camimg').contentWindow.location.reload(true);\
}, 600);\
</script>\
</head>\
\
<body>\
<h1>Sign Control</h1>\
<iframe class='camimg' src='cam.jpg'/></iframe>\
<div class='kb'>\
    <div class='left-side'>\
        <div class='key' onclick='send(0x1B)'>Esc</div>\
        <div class='key' onclick='send(0x4)'>Width</div>\
        <div class='key' onclick='send(0x7)'>CAPS</div>\
        <div class='key' onclick='send(0x5)'>Fonts</div>\
        <div class='key' onclick='send(0x2)'>Color</div>\
        <div class='key fn' onclick='send(0x9)'>FN</div>\
    </div>\
    <div class='row'>\
        <div class='key' onclick='send(0x1C)'>Power</div>\
        <div class='key' onclick='send(0x1)'>Symbol</div>\
        <div class='key' onclick='send(0x11)'>Animation</div>\
        <div class='key' onclick='send(0x14)'>Image</div>\
        <div class='key' onclick='send(0x12)'>Icon</div>\
        <div class='key' onclick='send(0x15)'>Neon</div>\
        <div class='key' onclick='send(0x16)'>Stretch</div>\
        <div class='key' onclick='send(0x19)'>Font Size</div>\
        <div class='key' onclick='send(0x17)'>F1</div>\
        <div class='key' onclick='send(0x1A)'>F2</div>\
    </div>\
    <div class='row'>\
        <div class='key' onclick='send(0x31)'>1<div class='alt'>!</div></div>\
        <div class='key' onclick='send(0x32)'>2<div class='alt'>@</div></div>\
        <div class='key' onclick='send(0x33)'>3<div class='alt'>#</div></div>\
        <div class='key' onclick='send(0x34)'>4<div class='alt'>$</div></div>\
        <div class='key' onclick='send(0x35)'>5<div class='alt'>%</div></div>\
        <div class='key' onclick='send(0x36)'>6<div class='alt'>^</div></div>\
        <div class='key' onclick='send(0x37)'>7<div class='alt'>&amp;</div></div>\
        <div class='key' onclick='send(0x38)'>8<div class='alt'>*</div></div>\
        <div class='key' onclick='send(0x39)'>9<div class='alt'>(</div></div>\
        <div class='key' onclick='send(0x30)'>0<div class='alt'>)</div></div>\
    </div>\
    <div class='row'>\
        <div class='key' onclick='send(0x51)'>Q<div class='alt'>+</div></div>\
        <div class='key' onclick='send(0x57)'>W<div class='alt'>-</div></div>\
        <div class='key' onclick='send(0x45)'>E<div class='alt'>=</div></div>\
        <div class='key' onclick='send(0x52)'>R<div class='alt'>/</div></div>\
        <div class='key' onclick='send(0x54)'>T<div class='alt'>\</div></div>\
        <div class='key' onclick='send(0x59)'>Y<div class='alt'>&lt;</div></div>\
        <div class='key' onclick='send(0x55)'>U<div class='alt'>&gt;</div></div>\
        <div class='key' onclick='send(0x49)'>I<div class='alt'>{</div></div>\
        <div class='key' onclick='send(0x4F)'>O<div class='alt'>}</div></div>\
        <div class='key' onclick='send(0x50)'>P<div class='alt'>:</div></div>\
    </div>\
    <div class='row'>\
        <div class='key' onclick='send(0x41)'>A</div>\
        <div class='key' onclick='send(0x53)'>S</div>\
        <div class='key' onclick='send(0x44)'>D</div>\
        <div class='key' onclick='send(0x46)'>F<div class='alt'>[</div></div>\
        <div class='key' onclick='send(0x47)'>G<div class='alt'>]</div></div>\
        <div class='key' onclick='send(0x48)'>H<div class='alt'>&quot;</div></div>\
        <div class='key' onclick='send(0x4A)'>J<div class='alt'>'</div></div>\
        <div class='key' onclick='send(0x4B)'>K<div class='alt'>.</div></div>\
        <div class='key' onclick='send(0x4C)'>L<div class='alt'>;</div></div>\
        <div class='key' onclick='send(0x2A)'>?<div class='alt'>~</div></div>\
    </div>\
    <div class='row'>\
        <div class='key' onclick='send(0x5A)'>Z</div>\
        <div class='key' onclick='send(0x58)'>X</div>\
        <div class='key' onclick='send(0x43)'>C</div>\
        <div class='key' onclick='send(0x56)'>V</div>\
        <div class='key' onclick='send(0x42)'>B</div>\
        <div class='key' onclick='send(0x4E)'>N</div>\
        <div class='key' onclick='send(0x4D)'>M</div>\
        <div class='key' onclick='send(0x21)'>,</div>\
    </div>\
    <div class='row'>\
\
        <div class='key' onclick='send(0x1E)'>Copy</div>\
        <div class='key' onclick='send(0x1D)'>Paste</div>\
        <div class='key' onclick='send(0x18)'>Attribute</div>\
\
        <div class='key space' onclick='send(0x20)'>SPACE</div>\
\
        <div class='key' onclick='send(0x3)'>Outline</div>\
    </div>\
    <div class='arrows'>\
        <div class='key' onclick='send(0x22)'>^</div>\
        <div class='key' onclick='send(0x25)'>&gt;</div>\
        <div class='key' onclick='send(0x23)'>v</div>\
        <div class='key' onclick='send(0x24)'>&lt;</div>\
    </div>\
    <div class='right-side'>\
        <div class='key' onclick='send(0x6)'>Menu</div>\
        <div class='key' onclick='send(0x8)'>Delete</div>\
        <div class='key' onclick='send(0xD)'>Enter</div>\
    </div>\
</div>\
</body>\
</html>\
";  
}

/*
void handleSetSignMessage()
{
  Serial.println("/setMessage BEGIN");
  String message = server.arg("message");
  Serial.printf("Got message '%s'\n", message);

  // Modify message procedure as per https://www.tvliquidator.com/ledsignkeyboardinstructions.pdf

  sendIR(IR_MENU);
  sendIR(IR_DOWN_OR_EFFECT_DOWN);  // Scroll to Mod MSG
  sendIR(IR_ENTER);                // Select Mod MSG
  sendIR(IR_ENTER);                // Select 1st MSG
  delay(1000);                     // Wait for "Modify your message" scrolling

  for (int i=0; i < 30 /* Best guess max msg length * /; i++)
  {
    sendIR(IR_DELETE);
  }

  for (int j=0; j < message.length(); j++)
  {
    char c = message.charAt(j);
    sendIr(c);
  }

  sendIR(IR_ENTER);                // Done setting text of message
  delay(1000);                     // Wait for "Select a start effect" scrolling
                                   // "3D" option displayed
  sentIR(IR_DOWN_OR_EFFECT_DOWN)   // "Auto" option                                 
  sendIR

  Serial.println("/setMessage END");
}
*/

/*
enum ReqReadState { READ_REQ, READ_BODY, DONE };

void handleClientRequest(WiFiClient client)
{
  Serial.println("Client connected");
  
  String request = "";
  // an http request ends with a blank line
  bool currentLineIsBlank = true;
  while (client.connected())
  {
    Serial.println("Still connected");
    if (client.available())
    {
      Serial.println("Data available");
      char c = client.read();
      Serial.printf("Read '%s'\n", c);
      request += c;

      if (c == '\n' && currentLineIsBlank)
      {
        processRequest(client, request, "");
        break;
      }
      
      if (c == '\n') 
      {
        // you're starting a new line
        currentLineIsBlank = true;
      }
      else if (c != '\r') 
      {
        // you've gotten a character on the current line
        currentLineIsBlank = false;
      }
    }
  }
  delay(500); // give the web browser time to receive the data
  client.stop();
  Serial.println("\nClient disconnected");
}

void handleClientRequest2(WiFiClient client)
{
  Serial.println("Client connected");
  
  
  String request = "";
  String body = "";
  // an http request ends with a blank line
  bool currentLineIsBlank = true;
  ReqReadState reqReadState = READ_REQ;

  Serial.println("Starting to read request");

  while (reqReadState != DONE)
  {
    
    if (!client.connected())
    {
      Serial.println("Lost client connection");
      reqReadState = DONE;
    }
    else
    {
      Serial.println("Still connected");
      char c;
      if (client.available())
      {
        Serial.println("Data available");
        c = client.read();
        Serial.printf("Read '%s'\n", c);
      }
      else
      {
        Serial.println("Out of data");
        reqReadState = DONE;
      }

      switch (reqReadState)
      {
        case READ_REQ:
          Serial.println("READ_REQ");
          request += c;

          if (c == '\n' && currentLineIsBlank)
          {
            reqReadState = READ_BODY;
            break;
          }

          if (c == '\n') { currentLineIsBlank = true; } // starting a new line
          else if (c != '\r') { currentLineIsBlank = false; } // got a character on the current line

          break;
        case READ_BODY:
          Serial.println("READ_BODY");
          body += c;
          break;
        case DONE:
          Serial.println("DONE");
          Serial.println(request);
          Serial.println("===");
          Serial.println(body);

          processRequest(client, request, body);
          break;
      }
    }
  }
  delay(100); // give the web browser time to receive the data
  client.stop();
  Serial.println("\nClient disconnected");
}

void writeHeaders(WiFiClient client, const char* contentType )
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: " + String(contentType));
    client.println("Connection: close");
    //client.println("Refresh: 5");  // refresh the page automatically every 5 sec
}

void processRequest(WiFiClient client, String request, String body)
{ 
  Serial.println("processRequest");
  
  char *method_cstr = strtok(const_cast<char*>(request.c_str()), " ");
  Serial.println(method_cstr);
  char *path_cstr = strtok(NULL, " ");
  Serial.println(path_cstr);
  char *rtype_cstr = strtok(NULL, "\n");
  Serial.println(rtype_cstr);
  
  String method = String(method_cstr);
  Serial.println(method);
  String path = String(path_cstr);
  Serial.println(path);

  Serial.printf("Request: '%s'\t'%s'\t'%s'\n", method, path, rtype_cstr);

  if (method.equals(String("POST")))
  {
    Serial.println("Got POST");
    if (path.startsWith("/send/"))
    {
      Serial.println("Got send command");
      sendIR(path.substring(6));
    }
  }
  else if (method.equals(String("GET"))) 
  {
    Serial.println("Got GET");
    if (path.equals(String("/")))
    {
      writeHeaders(client, "text/html");
      writePage(client);
    }
    else if (path.equals(String("/cam.jpg")))
    {
      writeHeaders(client, "text/html");
      
      //writeImage(client);
    }
  }
}
*/