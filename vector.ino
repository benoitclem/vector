
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>

#include "ssd1306_i2c.h"
#include "icons.h"

// STATE
typedef enum {
  CONF,
  MENU,
  QUOTE,
  BRIEFS,
  MESSAGE,
} stateMachine;
stateMachine state;

// Briefs
#define BRIEFS_SZ 512
int nBriefs = 0;
int currBrief = 0;
char briefs[BRIEFS_SZ];

// Quote
#define QUOTE_SZ 128
int nQuoteFrames = 0;
int currQuote = 0;
char quote[QUOTE_SZ];

// Buttons 
typedef enum {
  UP_BUTTON,
  ENTER_BUTTON,
  DOWN_BUTTON,
  NONE,
} buttonEvent;

buttonEvent bEvent = NONE;

// Persistant Infos
#define SSID_EEPROM_IDX 0
#define PASS_EEPROM_IDX 32
#define NAME_EEPROM_IDX 64
#define GREE_EEPROM_IDX 96
typedef struct {
   uint32_t A;
   char B[256];
} persistentData;

// DNS Defs
#define CONNECTION_RETRIES 40
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
uint8_t mac[6];

// WebServer
ESP8266WebServer server(80);

// Wifi stuffs
int numSsid = -1;
String ssid = "";
String pass = "";

// Screen
SSD1306 display(0x3c, 5, 4);
int xSz = 128;
int ySz = 64;

// IRQs
void gpioIrq0() {
  Serial.printf("Button 0 clicked\r\n");
  if(bEvent == NONE)
    bEvent = UP_BUTTON;
}

void gpioIrq12() {
  Serial.printf("Button 12 clicked\r\n");
  if(bEvent == NONE)
    bEvent = DOWN_BUTTON;
}

void gpioIrq14() {
  Serial.printf("Button 14 clicked\r\n");
  if(bEvent == NONE)
    bEvent = ENTER_BUTTON;
}

// ====== Persistent Configuration ======

void dumpEEPROM() {
  for(int i = 1; i <= 512;i++) {
    Serial.printf("0x%02x ",EEPROM.read(i-1));
    if(i%16 == 0) {
      Serial.printf("\r\n");
    }
  }
}

void storeSsid(String ssid) {
  EEPROM.write(SSID_EEPROM_IDX,ssid.length());
  for(int i = 0; i< ssid.length(); i ++) 
    EEPROM.write(SSID_EEPROM_IDX+i+1,ssid[i]);
}

void getSsid(String &ssid) {
  int n = int(EEPROM.read(SSID_EEPROM_IDX));
  Serial.printf("ssid length %d\r\n",n);
  if((n != 0) && (n != 0xff)) {
    for(int i = 0; i<n; i++) {
      ssid += char(EEPROM.read(SSID_EEPROM_IDX+1+i));
    }
  }
}

void storePassword(String pwd) {
  EEPROM.write(PASS_EEPROM_IDX,pwd.length());
  for(int i = 0; i< pwd.length(); i ++) 
    EEPROM.write(PASS_EEPROM_IDX+i+1,pwd[i]);  
}

void getPassword(String &pwd) {
  int n = int(EEPROM.read(PASS_EEPROM_IDX));
  Serial.printf("pass length %d\r\n",n);
  if((n != 0) && (n != 0xff)) {
    for(int i = 0; i<n; i++) {
      pwd += char(EEPROM.read(PASS_EEPROM_IDX+1+i));
    }
  }
}

// ====== WIFI AP ======

void apHandleRoot() {
  String str = "<html><h1>Please select your access point</h1><form method='get' action='config'><label>SSID: </label><select name='ssid' length=32>";
  for(int i = 0; i<numSsid; i++) {
    str += "<option>" + WiFi.SSID(i);
  }
  str += "</select><input name='pass' length=64><input type='submit'></form>";
  server.send(200, "text/html", str);
  delay(100);
}

void apHandleConfig() {
  String qssid = server.arg("ssid");
  String qpass = server.arg("pass");
  String str = "Thanks, your box will restart automatically now";
  server.send(200, "text/html", str);
  delay(100);
  Serial.println("Storing " + qssid + " " + qpass); 
  storeSsid(qssid);
  storePassword(qpass);
  EEPROM.commit();
  delay(1000);
  ESP.reset();
}

// ====== Content Loading ======

char content[512];
int contentCurr = 0;
int contentMaxSz = 512;

int loadContent(const char *request, char* b, int bMax) {
  int bCurr = 0;
  // Load content here.
  WiFiClient client;
  if (!client.connect("www.carbon14.arichard.fr", 3333)) {
    Serial.println("connection failed");
    return -1;
  }
  // Forge a request with the request type and the mac address which is our unique identifier
  client.printf("{\"request\":\"%s\",\"id\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"szMax\":120,\"spaceFont\":8}",request,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  // uint32_t t0 = ESP.getCycleCount();
  while(bCurr < bMax) {
    if(client.available()>0) {
      char c = client.read();
      if(c != '\r') {
        b[bCurr++] = c;
      } else {
        break;
      }
    }
  }
  for(int i = 0; i< bCurr; i++) {
    Serial.printf("%c",b[i]);
    if(((i%16) == 0) && (i!= 0)) {
      Serial.printf("\r\n");
    }
  }
  Serial.printf("\r\n");
  return bCurr;
}

void loadQuote(void) {
  int contentSz = loadContent("quote",content,contentMaxSz);
  if((contentSz > 0) && (contentSz<QUOTE_SZ)) {
    strncpy(quote,content,contentSz);
    quote[contentSz] = '\0';
    nQuoteFrames = 0;
    for(int i =0; i< strlen(quote); i++) {
      // The quote screens are separated by semicolons
      if(quote[i] == ';')
        nQuoteFrames++;
    }
    Serial.println(quote);
    Serial.println(nQuoteFrames);
  } else {
    Serial.println("Error getting quote");
  }
}

void loadBriefs(void) {
  int contentSz = loadContent("briefs",content,contentMaxSz);
  if((contentSz > 0) && (contentSz<BRIEFS_SZ)) {
    strncpy(briefs,content,contentSz);
    briefs[contentSz] = '\0';
    nBriefs = 0;
    for(int i =0; i< strlen(briefs); i++) {
      // The briefs screens are separated by semicolons
      if(briefs[i] == ';')
        nBriefs++;
    }
    Serial.println(briefs);
    Serial.println(nBriefs);
  } else {
    Serial.println("Error getting briefs");
  }
}

void loadAll(void) {
  // Could do a signe request..
  loadQuote();
  loadBriefs();
}

// ====== Screens =======

typedef void (*drawScreen)(int x, int y, int a);

void drawWeatherMenuFrame(int x,int y, int a) {
  display.drawString(xSz/2 - 8 -  ((7*8) /2) + x, ySz/2 - 4 + y, "Wheater");
  //display.drawMyFmt(x + 16 , y + 16,bite);
}

void drawQuoteMenuFrame(int x, int y, int a) {
  display.drawString(xSz/2 - 8 -  ((5*8) /2) + x, ySz/2 - 4 + y, "Quote");
  //display.drawMyFmt(x + 16 , y + 16,bite);
}

void drawQuoteFrame(int x, int y, int a) {
  int segmBegin = 0;
  int lastBegin = 0;
  int commaToFind = a + 1;
  int yLines = 0;
  // Find the right segment
  for(int i = 0; i < strlen(quote); i++)  {
    // Each time we get a : this is a new line
    if(quote[i] == ':') {
      yLines++;
    }
    // Each time we get a ; this is a new segment starting
    if(quote[i] == ';') {
      commaToFind--;
      if(commaToFind == 0) {
        // We found the right segment save index
        lastBegin = segmBegin;
        break;
      } else {
        // not the right segment reset yLines
        segmBegin = i + 1;
        yLines = 0;
      }
    }
  }

  int yTextSize = yLines * 8;
  int currQuoteSize = 0;
  int lineIndex = 0;
  for(int i = lastBegin; i < strlen(quote); i++) {
    // We encounter a ";"
    if(quote[i] != ';') {
      if(quote[i] == ':') {
        char subQuote[15];
        strncpy(subQuote,quote+lastBegin,currQuoteSize);
        subQuote[currQuoteSize] = 0;
        //Serial.println(subQuote);
        int lx = (xSz-8)/2 - ((currQuoteSize*8)/2);
        int ly = ySz/2  - yTextSize/2 + lineIndex*8;
        display.drawString(lx + x, ly + y,subQuote);
        lastBegin += currQuoteSize + 1;
        currQuoteSize = 0;
        lineIndex++;
      } else {
        currQuoteSize++;
      }
    } else {
      break;
    }
  }
}

void drawMessageMenuFrame(int x, int y, int a) {
  display.drawString(xSz/2 - 8 -  ((10*8) /2) + x, ySz/2 - 4 + y, "No message");
  //display.drawMyFmt(x + 16 , y + 16,bite);
}

void drawBriefsMenuFrame(int x, int y, int a) {
  display.drawString(xSz/2 - 8 -  ((8*8) /2) + x, ySz/2 - 4 + y, "Call me?");
}


void drawBriefsFrame(int x, int y, int a) {
  int segmBegin = 0;
  int lastBegin = 0;
  int commaToFind = a + 1;
  int yLines = 0;
  // Find the right segment
  Serial.printf("briefs a = %d\r\n",a);
  for(int i = 0; i < strlen(briefs); i++)  {
    // Each time we get a : this is a new line
    if(briefs[i] == ':') {
      yLines++;
    }
    // Each time we get a ; this is a new segment starting
    if(briefs[i] == ';') {
      commaToFind--;
      if(commaToFind == 0) {
        // We found the right segment save index
        lastBegin = segmBegin;
        break;
      } else {
        // not the right segment reset yLines
        segmBegin = i + 1;
        yLines = 0;
      }
    }
  }

  Serial.printf("briefs lastBegin = %d\r\n",lastBegin);
  Serial.printf("briefs yLines = %d\r\n",yLines);

  int yTextSize = yLines * 8;
  int currBriefSize = 0;
  int lineIndex = 0;
  for(int i = lastBegin; i < strlen(briefs); i++) {
    // We encounter a ";"
    if(briefs[i] != ';') {
      if(briefs[i] == ':') {
        char subBrief[15];
        strncpy(subBrief,briefs+lastBegin,currBriefSize);
        subBrief[currBriefSize] = 0;
        //Serial.println(subQuote);
        int lx = (xSz-8)/2 - ((currBriefSize*8)/2);
        int ly = ySz/2  - yTextSize/2 + lineIndex*8;
        display.drawString(lx + x, ly + y,subBrief);
        lastBegin += currBriefSize + 1;
        currBriefSize = 0;
        lineIndex++;
      } else {
        currBriefSize++;
      }
    } else {
      break;
    }
  }
}

void drawAckFrame(int x, int y, int a) {
  
}

void drawMenu(int x, int y, int a) {
  
}

drawScreen frames[4] = {&drawWeatherMenuFrame,&drawQuoteMenuFrame,drawMessageMenuFrame,&drawBriefsMenuFrame};
int numbFrames = 4;
int dispFrameIndex = 0;

void drawMarkers() {
  int startPoint = ySz/2 - int((float(numbFrames) / 2.0) * 9);
  for(int i = 0; i<numbFrames; i ++) {
    display.drawMyFmt(xSz-8,startPoint+i*(9),marker);
  }
  display.setPixel(xSz-5,startPoint+dispFrameIndex*(9)+3);
  display.setPixel(xSz-4,startPoint+dispFrameIndex*(9)+3);
  display.setPixel(xSz-5,startPoint+dispFrameIndex*(9)+4);
  display.setPixel(xSz-4,startPoint+dispFrameIndex*(9)+4);
}

typedef enum {
  DOWN,
  UP,
  LEFT,
  RIGHT,
} scrollWay;

void switchScreen(drawScreen s1, int a1, drawScreen s2, int a2,scrollWay way) {
  if(way == DOWN) {
    if(state == MENU)
      dispFrameIndex--;
    for(int i = 0; i <= 64; i+=4) {
      display.clear();
      s1(0,i,a1); // screen shown,
      s2(0,i-64,a2); // screen appearing;
      drawMarkers();
      display.display();
    }
  } else if(way == UP) {
    if(state == MENU)
      dispFrameIndex++;
    for(int i = 0; i <= 64; i+=4) {
      display.clear();
      s1(0,-i,a1); // screen shown,
      s2(0,-i+64,a2); // screen appearing;
      drawMarkers();
      display.display();
    }
  } else if(way == LEFT) {
    for(int i = 0; i <= 128; i+=4) {
      display.clear();
      s1(i,0,a1); // screen shown,
      s2(i-128,0,a2); // screen appearing;
      drawMarkers();
      display.display();
    } 
  } else if(way == RIGHT) {
    for(int i = 0; i <= 128; i+=4) {
      display.clear();
      s1(-i,0,a1); // screen shown,
      s2(-i+128,0,a2); // screen appearing;
      drawMarkers();
      display.display();
    } 
  }
  drawMarkers();
  display.display();
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  
  delay(400); // Wait for the uart stuffs emitted by the chip
  //Serial.printf("\r\n\r\n\r\nVector\r\n");

  // Pin Modes
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  
  pinMode(2, OUTPUT);   // B
  pinMode(13, OUTPUT);  // G
  pinMode(16, OUTPUT);  // R

  // Setup button irq
  attachInterrupt(0, gpioIrq0, FALLING);
  attachInterrupt(12, gpioIrq12, FALLING);
  attachInterrupt(14, gpioIrq14, FALLING);

  // Initialize display
  display.init();
  display.clear();

  // Get Unique identifier
  WiFi.macAddress(mac);
  //Serial.printf("id: %02x:%02x:%02x:%02x:%02x:%02x\r\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  
#if true
  // Preemption if button 2 and 3 are pushed
  bool configured = false;
  int gpio12State = digitalRead(12);
  int gpio14State = digitalRead(14);
  //Serial.println(gpio12State);
  //Serial.println(gpio14State);
  if((gpio12State == 0) && (gpio14State == 0)){
    //Serial.println("Got preemption .. go to configuration");
  } else {
    getSsid(ssid);
    getPassword(pass);
    //Serial.println("Got Stored sid: " + ssid);
    //Serial.println("Gto Stored pass: " + pass);
    configured = (pass.length() != 0) &&(ssid.length() != 0);
  }

  if(!configured) {
    // Mode is Station
    //WiFi.mode(WIFI_STA);
    //WiFi.disconnect();
    //delay(100);

    // Run Configuration
    state = CONF;

    // Set AP ssid
    WiFi.softAP("zazap", "zagettpw");
    delay(100);

    // Start dns capture
    dnsServer.start(DNS_PORT, "*", apIP);

    // Show Wifi Configuration screen
    display.drawMyFmt(32,0,wifiLogo);
    display.display();

    numSsid = WiFi.scanNetworks();
    
    server.on("/", apHandleRoot);
    server.on("/config", apHandleConfig);
    server.begin();
  } else {
    // Run App
    state = MENU;
    int retries = 0;
    //Serial.println("Connect to " + ssid + " with " + pass);
    WiFi.begin(ssid.c_str(), pass.c_str());
    while ((WiFi.status() != WL_CONNECTED) and (retries < CONNECTION_RETRIES) ) {
      delay(250);
      display.clear();
      display.drawString((xSz/2) - (9*8)/2, (ySz/2) - 4, "conneting");
      switch((retries%3)+1) {
        case 1:
          display.drawString((xSz/2) - (3*8)/2 , (ySz/2) + 4, ".");
          break;
        case 2:
          display.drawString((xSz/2) - (3*8)/2 , (ySz/2) + 4, "..");
          break;
        case 3:
          display.drawString((xSz/2) - (3*8)/2 , (ySz/2) + 4, "...");
          break;
        default:
          break;
      }
      display.display();
      retries++;
    }
    if(retries >= CONNECTION_RETRIES) {
      //Serial.println("Could not connect");
      display.clear();
      display.drawString((xSz/2) - (10*8)/2, (ySz/2) - 4, "No network");
      display.drawString((xSz/2) - (11*8)/2, (ySz/2) + 4, "maintain UP");
      display.drawString((xSz/2) - (13*8)/2, (ySz/2) + 12, "to run config");
      display.display();
      delay(2000);
      if(gpio12State == 0) {
        // Reset
        storeSsid("");
        storePassword("");
        EEPROM.commit();
        ESP.reset();
      }
      delay(5000);
      ESP.reset();
    } else {
      //Serial.println("Got connection");
      display.clear();
      display.drawString((xSz/2) - (9*8)/2, (ySz/2) - 4, "Connected");
      display.display();
  
      loadAll();
      
      display.clear();
      dispFrameIndex = 0;
      frames[dispFrameIndex](0,0,0);
      drawMarkers();
      display.display();
    }
  }

#else 
  //Serial.println("Got connection");
  display.clear();
  display.drawString((xSz/2) - (9*8)/2, (ySz/2) - 4, "Connected");
  display.display();
  delay(1000);
  display.clear();
  state = RUN;
  drawFrame1(0,0);
  dispFrameIndex = 0;
  drawMarkers();
  display.display();
#endif
}

void loop() {
  // This 
  switch(state) {
  case CONF:
    dnsServer.processNextRequest();
    server.handleClient();
    break;
  default:
  case MENU:
    delay(500);
    Serial.println("Menu LOOP");
    if(bEvent == UP_BUTTON) {  // We clicked UP screens goes DOWN
      if(dispFrameIndex != 0) {
        switchScreen(frames[dispFrameIndex],0,frames[dispFrameIndex-1],0,DOWN);
      }
    }
    if(bEvent == DOWN_BUTTON) { // We clicled DOWN screens goes UP
      if(dispFrameIndex != (numbFrames-1)) {
        switchScreen(frames[dispFrameIndex],0,frames[dispFrameIndex+1],0,UP);
      }
    }
    if(bEvent == ENTER_BUTTON) {
      if(dispFrameIndex == 1) {
        currQuote = 0;
        switchScreen(frames[dispFrameIndex],0,drawQuoteFrame,currQuote,RIGHT);
        state = QUOTE;
      } else if(dispFrameIndex == 3) {
        currBrief = 0;
        switchScreen(frames[dispFrameIndex],0,drawBriefsFrame,currBrief,RIGHT);
        state = BRIEFS;
      }
    }
    bEvent = NONE;
    break;
  case QUOTE:
    delay(500);
    Serial.println("Quote LOOP");
    if(bEvent == UP_BUTTON) {  // We clicked UP screens goes DOWN
      if(currQuote != 0) {
        switchScreen(drawQuoteFrame,currQuote,drawQuoteFrame,currQuote-1,DOWN);
        currQuote--;
      }
    }
    if(bEvent == DOWN_BUTTON) { // We clicled DOWN screens goes UP
      if(currQuote != (nQuoteFrames-1)) {
        switchScreen(drawQuoteFrame,currQuote,drawQuoteFrame,currQuote+1,UP);
        currQuote++;
      }
    }
    if(bEvent == ENTER_BUTTON) {
        switchScreen(drawQuoteFrame,currQuote,frames[dispFrameIndex],0,LEFT);
        state = MENU;
    }
    bEvent = NONE;
    break;
  case BRIEFS:
    delay(500);
    Serial.println("Briefs LOOP");
    if(bEvent == UP_BUTTON) {  // We clicked UP screens goes DOWN
      if(currBrief != 0) {
        switchScreen(drawBriefsFrame,currBrief,drawBriefsFrame,currBrief-1,DOWN);
        currBrief--;
      }
    }
    if(bEvent == DOWN_BUTTON) { // We clicled DOWN screens goes UP
      if(currBrief != (nBriefs-1)) {
        switchScreen(drawBriefsFrame,currBrief,drawBriefsFrame,currBrief+1,UP);
        currBrief++;
      }
    }
    if(bEvent == ENTER_BUTTON) {
        switchScreen(drawBriefsFrame,currBrief,frames[dispFrameIndex],0,LEFT);
        state = MENU;
    }
    bEvent = NONE;
    break; 
  }

  /*
  // put your main code here, to run repeatedly:
  Serial.printf("Loop\n");
  delay(500);
  analogWrite(13, 256/8);
  analogWrite(16, 512/8);
  */
}
