
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <Ticker.h>

#include "ssd1306_i2c.h"
#include "icons.h"

// STATE
typedef enum {
  WELCOME,
  MENU,
  QUOTE,
  BRIEFS,
  MESSAGE,
  VALIDATE,
} stateMachine;
stateMachine state;

typedef enum {
  LOGO,
  GREETINGS,
  CONF,
} stateWelcomeType;
stateWelcomeType stateWelcome;

// Weather
typedef struct _weatherData{
  char datehour[5];
  char idLogo;
  uint8_t night;
  int8_t temp;
} weatherData;
char wthData[8] = {25,12,15,00,00,1,0,0};
weatherData *wData = (weatherData*) &wthData;

// Message
#define MSG_SZ 256
int nMsgFrames = 0;
int currMsg = 0;
int msgStatus = 2; // 0 No messge, 1 Message read, 2 Message to read
char message[MSG_SZ];

// Welcome
int nWelcomeFrames = 0;
int currWelcome = 0;
char welcome[128];

// Quote
#define QUOTE_SZ 256
int nQuoteFrames = 0;
int currQuote = 0;
char quote[QUOTE_SZ];

// Briefs
#define BRIEFS_SZ 512
int nBriefs = 0;
int currBrief = 0;
char briefs[BRIEFS_SZ];

// Timer
Ticker timer;

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
String cname = "";

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

void store(int whereToStore, String strToStore) {
  EEPROM.write(whereToStore,strToStore.length());
  for(int i = 0; i< strToStore.length(); i ++) 
    EEPROM.write(whereToStore+i+1,strToStore[i]);
}

void get(int whereToGet, String &strToGet) {
  int n = int(EEPROM.read(whereToGet));
  //Serial.printf("Thing to read is length of %d\r\n",n);
  if((n != 0) && (n != 0xff)) {
    for(int i = 0; i<n; i++) {
      strToGet += char(EEPROM.read(whereToGet+1+i));
    }
  } 
}

void storeSsid(String ssid) {
  store(SSID_EEPROM_IDX,ssid);
}

void getSsid(String &ssid) {
  get(SSID_EEPROM_IDX,ssid);
}

void storePassword(String pwd) {
  store(PASS_EEPROM_IDX,pwd);
}

void getPassword(String &pwd) {
  get(PASS_EEPROM_IDX,pwd);
}

void storeName(String nme) {
  store(NAME_EEPROM_IDX,nme);
}

void getName(String &nme) {
  get(NAME_EEPROM_IDX,nme); 
}
// ==== Id Icon to Icon =====

const char *idToIcon(int id, bool night){
  switch(id) {
    case 1:
      if(night)
        return moonyLogo;
      else
        return sunnyLogo;
    break;
    case 2:
      if(night)
        return mostlyMoonyLogo;
      else
        return mostlySunnyLogo;
      break;
    case 3:
    case 4:
      return cloudyLogo;
      break;
    case 9:
    case 10:
      return rainyLogo;
      break;
    case 11:
      return thunderLogo;
      break;
    case 13:
      return snowyLogo;
      break;
    case 50:
      return foggyLogo;
      break;
    default:
      return sunnyLogo;
      break;
  }
}

// ====== WIFI AP ======
String styles = "<style>html{background-color:#ffcc33;color:#000;font-family:sans-serif;}body{text-align:center;margin:auto;position:relative;}div{position:absolute;top:0;bottom:0;left:0;right:0;margin:auto;width:100%;height:250px}form{line-height:40px;display:inline-block;text-align:left;}label,input,select{display:inline-block;width:150px;}input,select{border:1px solid black;background-color:#ffcc33;border-radius:5px;padding-left:8px;}#submit{width:50%;margin:auto;display:block;margin-top:30px;padding:0;height:17px;}</style>";

void apHandleRoot() {
  String qname;
  getName(qname);
  
  String str = "<html>";
  str += "<meta name='viewport' content='width=device-width, user-scalable=no'>";
  str += styles;
  str += "<div>";
  str += "<h1>Please select your access point</h1>";
  str += "<form method='get' action='config'>";
  str += "<label for='name'>Name : </label>";
  str += "<input name='name' value='" + qname +  "' id='name' length=16><br>";
  str += "<label for='ssid'>SSID : </label>";
  str += "<select name='ssid' id='ssid' length=32>";
  for(int i = 0; i<numSsid; i++) {
    str += "<option>" + WiFi.SSID(i);
  }
  str += "</select><br>";
  str += "<label for='pass'>Password : </label>";
  str += "<input name='pass' type='password'  id='pass' length=64><br>";
  str += "<input type='submit' id='submit'>";
  str += "</form>";
  str += "</div>";
  str += "</html>";
  server.send(200, "text/html", str);
  delay(100);
}

void apHandleConfig() {
  String qname = server.arg("name");
  String qssid = server.arg("ssid");
  String qpass = server.arg("pass");
  String str = "<html>";
  str += styles;
  str += "<div>";
  str += "<h1>Thanks " + qname +" ,<br>your box will restart<br>automatically now</h1>";
  str += "</div>";
  str += "</html>";
  server.send(200, "text/html", str);
  delay(100);
  Serial.println("Storing " + qssid + " " + qpass);
  storeName(qname);
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

int requestContent(const char *request, char* b, int bMax) {
  int bCurr = 0;
  // Load content here.
  WiFiClient client;
  if (!client.connect("www.carbon14.arichard.fr", 3333)) {
    Serial.println("connection failed");
    return -1;
  }
  // Forge a request with the request type and the mac address which is our unique identifier
  if(!strcmp(request,"name")) {
    unsigned char qname[16];
    cname.getBytes(qname,16);
    client.printf("{\"request\":\"%s\",\"name\":\"%s\",\"id\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}",request,qname,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  } else if(!strcmp(request,"wheather")) {
    client.printf("{\"request\":\"%s\",\"id\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}",request,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  } else if(!strcmp(request,"ackMsg")) {
    client.printf("{\"request\":\"%s\",\"id\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}",request,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  } else if(!strcmp(request,"postBrief")) {
    client.printf("{\"request\":\"%s\",\"id\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"idBrief\":%d}",request,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],b[0]);
  } else {
    client.printf("{\"request\":\"%s\",\"id\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"szMax\":120,\"spaceFont\":8}",request,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  }
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

void loadName(void) {
  int contentSz = requestContent("name", content, contentMaxSz);
  Serial.println(contentSz);
}

void loadWheather(void) {
  int contentSz = requestContent("wheather", content, contentMaxSz);
  memcpy((char*)wthData,content,8);
  Serial.println(contentSz);
}

void loadQuote(void) {
  int contentSz = requestContent("quote",content,contentMaxSz);
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

void loadMessage(void) {
  int contentSz = requestContent("message",content,contentMaxSz);
  if((contentSz > 0) && (contentSz<MSG_SZ)) {
    msgStatus = int(content[0]-48);
    strncpy(message,content+1,contentSz-1);
    message[contentSz] = '\0';
    nMsgFrames = 0;
    for(int i =0; i< strlen(message); i++) {
      // The quote screens are separated by semicolons
      if(message[i] == ';')
        nMsgFrames++;
    }
    Serial.println(message);
    Serial.println(nMsgFrames);
  } else {
    Serial.println("Error getting message");
  } 
}

void loadBriefs(void) {
  int contentSz = requestContent("briefs",content,contentMaxSz);
  if((contentSz > 0) && (contentSz<BRIEFS_SZ)) {
    strncpy(briefs,content,contentSz);
    briefs[contentSz] = '\0';
    nBriefs = 0;
    for(int i = 0; i< strlen(briefs); i++) {
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

void postBrief(int nBrief) {
  content[0] = nBrief;
  int contentSz = requestContent("postBrief",content,contentMaxSz);
  Serial.println(contentSz);
}

void ackMessage(void) {
  int contentSz = requestContent("ackMsg",content,contentMaxSz);
  Serial.println(contentSz);
}

void loadAll(void) {
  // Could do a signe request..
  loadName();
  loadWheather();
  loadMessage();
  loadQuote();
  loadBriefs();
}

// ====== Screens =======


// ====== CENTER TEXT =====

void drawCenterTextFrame(int x, int y, char *b,int commaToFind) {
  int segmBegin = 0;
  int lastBegin = 0;
  int yLines = 0;
   // Find the right segment
  for(int i = 0; i < strlen(b); i++)  {
    // Each time we get a : this is a new line
    if(b[i] == ':') {
      yLines++;
    }
    // Each time we get a ; this is a new segment starting
    if(b[i] == ';') {
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

  int yTextSize = yLines * 10;
  int currBSize = 0;
  int lineIndex = 0;
  for(int i = lastBegin; i < strlen(b); i++) {
    // We encounter a ";"
    if(b[i] != ';') {
      if(b[i] == ':') {
        char subB[15];
        strncpy(subB,b+lastBegin,currBSize);
        subB[currBSize] = 0;
        //Serial.println(subQuote);
        int lx = (xSz)/2 - ((currBSize*8)/2);
        int ly = ySz/2  - yTextSize/2 + lineIndex*10;
        display.drawString(lx + x, ly + y,subB);
        lastBegin += currBSize + 1;
        currBSize = 0;
        lineIndex++;
      } else {
        currBSize++;
      }
    } else {
      break;
    }
  }
}

// ====== WHEATER =======
typedef void (*drawScreen)(int x, int y, int a);

void drawWeatherMenuFrame(int x,int y, int a) {
  int id = wData->idLogo;
  bool night = wData->night == 1;
  const char *pLogo = idToIcon(id,night);
  display.drawMyFmt(0+x,y,pLogo);
  char buff[10];
  // date : x Taille 8
  sprintf(buff,"%02d/%02d\0",wData->datehour[0],wData->datehour[1]);
  display.drawString(xSz/2 + x + 8, ySz/2 - 24 + y, buff);
  // hour : x Taille 5
  sprintf(buff,"%02d:%02d\0",wData->datehour[3],wData->datehour[4]);
  display.drawString(xSz/2 + x + 8, ySz/2 - 16 + y, buff);
  // Temp
  display.setFontScale2x2(true);
  sprintf(buff,"%2dC\0",wData->temp);
  //sprintf(buff,"%2dC\0",-2);
  display.drawString(xSz/2 + x, ySz/2 +y,buff);
  display.setFontScale2x2(false);
  //display.drawString(xSz/2 - 8 -  ((7*8) /2) + x, ySz/2 - 4 + y, "Wheater");
  //display.drawMyFmt(x + 16 , y + 16,bite);
}

// ===== WELCOME =====

void drawWelcomeFrame(int x, int y, int a) {
  display.setFontScale2x2(true);
  display.drawString((xSz/2) - (3*16) + x + 1 , (ySz/2) - 8 + y,"ZAGETT"); // plus 1 is because the font is not center in his spacing
  display.setFontScale2x2(false);
}

void drawGreetingsFrame(int x, int y, int a) {
  drawCenterTextFrame(x,y,welcome,a+1);
}

void drawConfigurationFrame(int x, int y, int a) {
    // Show Wifi Configuration screen
    display.drawMyFmt(32+x,0,setupLogo);
    //display.drawMyFmt(32+x,y,wifiLogo);
}

// ====== QUOTE ======

void drawQuoteMenuFrame(int x, int y, int a) {
  display.drawMyFmt(32+x,y,bubbleLogo);
  //display.drawString(xSz/2 - 8 -  ((5*8) /2) + x, ySz/2 - 4 + y, "Quote");
  //display.drawMyFmt(x + 16 , y + 16,bite);
}

void drawQuoteFrame(int x, int y, int a) {
  drawCenterTextFrame(x,y,quote,a+1);
}

// ===== MESSAGES ======

void drawMessageMenuFrame(int x, int y, int a) {
  if(msgStatus == 0)
    display.drawString(xSz/2 - 8 -  ((10*8) /2) + x, ySz/2 - 4 + y, "No message");
  else
    display.drawMyFmt(32+x,y,letterLogo);
    //display.drawString(xSz/2 - 8 -  ((9*8) /2) + x, ySz/2 - 4 + y, "1 Message");
  //display.drawMyFmt(x + 16 , y + 16,bite);
}

void drawMessageFrame(int x, int y, int a) {
  drawCenterTextFrame(x,y,message,a+1);
}

// ======= BRIEFS ======

void drawBriefsMenuFrame(int x, int y, int a) {
  //display.drawString(xSz/2 - 8 -  ((8*8) /2) + x, ySz/2 - 4 + y, "Call me?");
  display.drawMyFmt(32+x,y,penLogo);
}

void drawBriefsFrame(int x, int y, int a) {
  drawCenterTextFrame(x,y,briefs,a+1);
}

// ===== BRIEF VALIDATION =====

bool validate = true;

void drawValidateFrame(int x, int y, int a) {
  // Pas de deplacement y dans cette frame
  display.drawString(xSz/2 - ((4*8)/2) + x, ySz/2 - 4,"Send");
  display.drawString(xSz/2 - ((6*8)/2) + x, ySz/2 + 4 ,"Brief?");
  display.drawString(xSz - 25 + x, ySz/3 - 4, "OUI");
  display.drawString(xSz - 25 + x, ySz-(ySz/3) - 4, "NON");
  
  int yRef;
  if(a == 0)  // NON
    yRef = ySz-(ySz/3) - 1;
  else
    yRef = ySz/3 - 1;

  // Horizontal (x)
  /*for(int i = 0; i<=((3*8)+2); i++){
    display.setPixel(xSz - 25 - 1 + x + i,yRef - 4 - 1 + y);
    display.setPixel(xSz - 25 - 1 + x + i,yRef + 4 + 1 + y);
  }*/
  // Vertical (y)
  for(int i = 0; i<=((1*8)+2); i++){
    // display.setPixel(xSz - 25 - 1 + x,yRef - 4 - 1 + i + y);
    display.setPixel(xSz - 25 - 1 + 3*8 + 1 + x,yRef - 4 - 1 + i + y);
  }
  

}

drawScreen frames[4] = {&drawWeatherMenuFrame,&drawQuoteMenuFrame,drawMessageMenuFrame,&drawBriefsMenuFrame};
int numbFrames = 4;
int dispFrameIndex = 0;

void drawMarkers() {

  int nFrames = 0;
  int cFrame = 0;
  if(state == WELCOME) {
    if(stateWelcome == GREETINGS) {
      nFrames = nWelcomeFrames;
      cFrame = currWelcome;
    }
  } else if(state == MENU) {
    nFrames = numbFrames;
    cFrame = dispFrameIndex;
  } else if(state == QUOTE) {  
    nFrames = nQuoteFrames;
    cFrame = currQuote;
  } else if(state == BRIEFS) {
    nFrames = nBriefs;
    cFrame = currBrief;
  } else if(state == MESSAGE) {
    nFrames = nMsgFrames;
    cFrame = currMsg;
  } else {
    return;
  }
  int startPoint = ySz/2 - int((float(nFrames) / 2.0) * 9);

  for(int i = 0; i<nFrames; i++) {
    for(int j = 0; j<8; j++) {
      //display.setPixel(0,startPoint+i*9+j);
      display.setPixel(xSz-1,startPoint+i*9+j);
    }
    if(cFrame == i) {
      for(int j = 0; j<8; j++) {
        display.setPixel(xSz-2,startPoint+i*9+j);
        display.setPixel(xSz-3,startPoint+i*9+j);
        //display.setPixel(1,startPoint+i*9+j);
        //display.setPixel(2,startPoint+i*9+j);
      }
    }
  }


  /*
  for(int i = 0; i<numbFrames; i ++) {
    display.drawMyFmt(xSz-8,startPoint+i*(9),marker);
  }
  display.setPixel(xSz-5,startPoint+dispFrameIndex*(9)+3);
  display.setPixel(xSz-4,startPoint+dispFrameIndex*(9)+3);
  display.setPixel(xSz-5,startPoint+dispFrameIndex*(9)+4);
  display.setPixel(xSz-4,startPoint+dispFrameIndex*(9)+4);
  */
}

typedef enum {
  DOWN,
  UP,
  LEFT,
  RIGHT,
} scrollWay;

void redrawScreen() {
  if(state == MENU) {
    display.clear();
    frames[dispFrameIndex](0,0,0); // screen shown,
    drawMarkers();
    display.display();
  }
}

void switchScreen(drawScreen s1, int a1, drawScreen s2, int a2,scrollWay way) {
  if(way == DOWN) {
    if(state == WELCOME) {
      if(stateWelcome == GREETINGS) {
        currWelcome--;
      }
    } else if(state == MENU) {
      dispFrameIndex--;
    } else if(state == QUOTE) {
      currQuote--;
    } else if(state == MESSAGE) {
      currMsg--;
    } else if(state == BRIEFS) {
      currBrief--;
    }
    for(int i = 0; i <= 64; i+=8) {
      display.clear();
      s1(0,i,a1); // screen shown,
      s2(0,i-64,a2); // screen appearing;
      drawMarkers();
      display.display();
    }
  } else if(way == UP) {
    if(state == WELCOME) {
      if(stateWelcome == GREETINGS) {
        currWelcome++;
      }
    } else if(state == MENU) {
      dispFrameIndex++;
    } else if(state == QUOTE) {
      currQuote++;
    } else if(state == MESSAGE) {
      currMsg++;
    } else if(state == BRIEFS) {
      currBrief++;
    }
    for(int i = 0; i <= 64; i+=8) {
      display.clear();
      s1(0,-i,a1); // screen shown,
      s2(0,-i+64,a2); // screen appearing;
      drawMarkers();
      display.display();
    }
  } else if(way == LEFT) {
    for(int i = 0; i <= 128; i+=8) {
      display.clear();
      s1(i,0,a1); // screen shown,
      s2(i-128,0,a2); // screen appearing;
      drawMarkers();
      display.display();
    } 
  } else if(way == RIGHT) {
    for(int i = 0; i <= 128; i+=8) {
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

int count = 0;
void timerIsr() {
    switch(msgStatus) {
    default:
    case 0:
    case 1:
      analogWrite(13, 0);
      analogWrite(16, 0);
      break;
    case 2:
      float something = millis()/1000.0;
      int value = 60.0 + 50 * sin( something * PI  );
      analogWrite(13, value/2);   // GREEN
      analogWrite(16, value); // RED
      break;
  }
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
    getName(cname);
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
    state = WELCOME;
    stateWelcome = LOGO;
    nWelcomeFrames = 1;
    sprintf(welcome,"CHER CLIENT,:TOUTE L EQUIPE:ZAGETT  VOUS:PRESENTE SES:MEILLEURS VOEUX:;");

    // Draw Logo
    drawWelcomeFrame(0,0,0);
    display.display();

    // Set AP ssid
    WiFi.softAP("zazap", "zagettpw");
    delay(100);

    // Start dns capture
    dnsServer.start(DNS_PORT, "*", apIP);
    numSsid = WiFi.scanNetworks();
    
    server.on("/", apHandleRoot);
    server.on("/config", apHandleConfig);
    server.begin();
  } else {
    // Run App
    state = MENU;
    int retries = 0;
    Serial.println("Connect to " + ssid + " with " + pass);
    WiFi.begin(ssid.c_str(), pass.c_str());
    while ((WiFi.status() != WL_CONNECTED) and (retries < CONNECTION_RETRIES) ) {
      delay(250);
      display.clear();
      display.setFontScale2x2(true);
      display.drawString((xSz/2) - (6*16)/2 + 1, (ySz/2) - 16,"ZAGETT");
      display.setFontScale2x2(false);
      display.drawString((xSz/2) - (10*8)/2, (ySz/2) , "connecting");
      switch((retries%3)+1) {
        case 1:
          display.drawString((xSz/2) - (3*8)/2 , (ySz/2) + 8, ".");
          break;
        case 2:
          display.drawString((xSz/2) - (3*8)/2 , (ySz/2) + 8, "..");
          break;
        case 3:
          display.drawString((xSz/2) - (3*8)/2 , (ySz/2) + 8, "...");
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
      int gpio0State = digitalRead(0);
      if(gpio0State == 0) {
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
  // Run the timer
  timer.attach(0.1, timerIsr);
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

#define LOOP_DELAY 250

int wLogoIndex = 0;
int welcomeCounter = 0;
int refreshWheatherCounter = 0;
int refreshMessageCounter = 0;
#define REFRESH_WHEATHER_VALUE 20
#define REFRESH_MESSAGE_VALUE 40
void loop() {
  // This 
  switch(state) {
  case WELCOME:
    delay(LOOP_DELAY);
    switch(stateWelcome) {
      case LOGO:
        welcomeCounter++;
        if(welcomeCounter > 100)
          Serial.printf("Go to Greetings");
          switchScreen(drawWelcomeFrame,0,drawGreetingsFrame,0,RIGHT);
          stateWelcome = GREETINGS;
        break;
      case GREETINGS:
        Serial.printf("GREETINGS\n");
        if(bEvent == UP_BUTTON) {  // We clicked UP screens goes DOWN
          if(currWelcome != 0) {
            switchScreen(drawGreetingsFrame,currWelcome,drawGreetingsFrame,currWelcome+1,DOWN);
          }
        }
        if(bEvent == DOWN_BUTTON) {
          if(currWelcome != nWelcomeFrames-1) {
            switchScreen(drawGreetingsFrame,currWelcome,drawGreetingsFrame,currWelcome-1,UP);
          }
        }
        if(bEvent == ENTER_BUTTON) {
          stateWelcome = CONF;
          switchScreen(drawGreetingsFrame,currWelcome,drawConfigurationFrame,0,RIGHT);
        }
        bEvent = NONE;
        break;
      default:
      case CONF:
        dnsServer.processNextRequest();
        server.handleClient();
        break;
    }
    break;
  default:
  case MENU:
    delay(LOOP_DELAY);
    // Refresh contents
    if(refreshWheatherCounter > REFRESH_WHEATHER_VALUE) {
      Serial.printf("Request Wheather");
      refreshWheatherCounter = 0;
      loadWheather();
      redrawScreen();
    } else {
      refreshWheatherCounter++;
    }
    // Refresh Messages
    if(refreshMessageCounter > REFRESH_MESSAGE_VALUE) {
      Serial.printf("Request Messages");
      refreshMessageCounter = 0;
      loadMessage();
      redrawScreen();
    } else {
      refreshMessageCounter++;
    }
    // UI
    Serial.println("Menu LOOP");
    if(bEvent == UP_BUTTON) {  // We clicked UP screens goes DOWN
      refreshWheatherCounter = 0; // This avoid to make request when using UI
      refreshMessageCounter = 0;
      if(dispFrameIndex != 0) {
        switchScreen(frames[dispFrameIndex],0,frames[dispFrameIndex-1],0,DOWN);
      }
    }
    if(bEvent == DOWN_BUTTON) { // We clicled DOWN screens goes UP
      refreshWheatherCounter = 0; // This avoid to make request when using UI
      refreshMessageCounter = 0;
      if(dispFrameIndex != (numbFrames-1)) {
        switchScreen(frames[dispFrameIndex],0,frames[dispFrameIndex+1],0,UP);
      }
    }
    if(bEvent == ENTER_BUTTON) {
      /* if(dispFrameIndex == 0) {
        switchScreen(frames[dispFrameIndex],wLogoIndex,frames[dispFrameIndex],wLogoIndex+1,RIGHT);
        wLogoIndex++;
      } else */ if(dispFrameIndex == 1) {
        currQuote = 0;
        state = QUOTE;
        switchScreen(frames[dispFrameIndex],0,drawQuoteFrame,currQuote,RIGHT);
      } else if(dispFrameIndex == 2) {
        if(msgStatus != 0) {
          currMsg = 0;
          state = MESSAGE;
          switchScreen(frames[dispFrameIndex],0,drawMessageFrame,currMsg,RIGHT);
        }
      } else if(dispFrameIndex == 3) {
        currBrief = 0;
        state = BRIEFS;
        switchScreen(frames[dispFrameIndex],0,drawBriefsFrame,currBrief,RIGHT);
      }
    }
    bEvent = NONE;
    break;
  case MESSAGE:
    delay(LOOP_DELAY);
    Serial.println("Message LOOP");
    if(bEvent == UP_BUTTON) {  // We clicked UP screens goes DOWN
      if(currMsg != 0) {
        switchScreen(drawMessageFrame,currMsg,drawMessageFrame,currMsg-1,DOWN);
      }
    }
    if(bEvent == DOWN_BUTTON) { // We clicled DOWN screens goes UP
      if(currMsg != (nMsgFrames-1)) {
        switchScreen(drawMessageFrame,currMsg,drawMessageFrame,currMsg+1,UP);
      }
    }
    if(bEvent == ENTER_BUTTON) {
        state = MENU;
        switchScreen(drawMessageFrame,currMsg,frames[dispFrameIndex],0,LEFT);
        msgStatus = 1;
        ackMessage();
    }
    bEvent = NONE;
    break;
  case QUOTE:
    delay(LOOP_DELAY);
    Serial.println("Quote LOOP");  
    if(bEvent == UP_BUTTON) {  // We clicked UP screens goes DOWN
      if(currQuote != 0) {
        switchScreen(drawQuoteFrame,currQuote,drawQuoteFrame,currQuote-1,DOWN);
      }
    }
    if(bEvent == DOWN_BUTTON) { // We clicled DOWN screens goes UP
      if(currQuote != (nQuoteFrames-1)) {
        switchScreen(drawQuoteFrame,currQuote,drawQuoteFrame,currQuote+1,UP);
      }
    }
    if(bEvent == ENTER_BUTTON) {
        state = MENU;
        switchScreen(drawQuoteFrame,currQuote,frames[dispFrameIndex],0,LEFT);
    }
    bEvent = NONE;
    break;
  case BRIEFS:
    delay(LOOP_DELAY);
    Serial.println("Briefs LOOP");
    if(bEvent == UP_BUTTON) {  // We clicked UP screens goes DOWN
      if(currBrief != 0) {
        switchScreen(drawBriefsFrame,currBrief,drawBriefsFrame,currBrief-1,DOWN);
      }
    }
    if(bEvent == DOWN_BUTTON) { // We clicled DOWN screens goes UP
      if(currBrief != (nBriefs-1)) {
        switchScreen(drawBriefsFrame,currBrief,drawBriefsFrame,currBrief+1,UP);
      }
    }
    if(bEvent == ENTER_BUTTON) {
        state = VALIDATE;
        switchScreen(drawBriefsFrame,currBrief,drawValidateFrame,0,RIGHT);
        validate = false;
    }
    bEvent = NONE;
    break;
  case VALIDATE:
    delay(LOOP_DELAY);
    Serial.println("Validate LOOP");
    if(bEvent == UP_BUTTON) {  // We clicked UP screens goes DOWN
      if(!validate) {
        switchScreen(drawValidateFrame,0,drawValidateFrame,1,DOWN);
        validate = true;
      }
    }
    if(bEvent == DOWN_BUTTON) { // We clicled DOWN screens goes UP
      if(validate) {
        switchScreen(drawValidateFrame,1,drawValidateFrame,0,UP);
        validate = false;
      }
    }
    if(bEvent == ENTER_BUTTON) {
        int currVal = (validate)?1:0;
        state = MENU;
        if(currVal == 1)
          postBrief(currBrief);
        // Maybe some screen that shows message is gone
        switchScreen(drawValidateFrame,currVal,frames[dispFrameIndex],0,LEFT);
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
