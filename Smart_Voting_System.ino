#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <SPI.h>
#include <MFRC522.h>

  // set the LCD address to 0x3F for a 16 chars and 2 line display

// wifi vars
const char* ssid = "IIC_WIFI";
const char* password = "R@isecom22";
String rfid_id = "";
uint8_t fingerprint_status = 0;
uint8_t fingerprint_attempt = 0;
String user_id = "";
String user_name = "";

# define RFID_MATCH_URL "http://192.168.40.107:8000/match/rfid" 
# define CAST_VOTE_URL "http://192.168.40.107:8000/cast-vote" 

unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

#define SS_PIN 5
#define RST_PIN 27
#define buzzerPin 25

// push button pins
#define btn1 32 
#define btn2 33
#define btn3 26
#define btn4 12
   
#define voter_name ""
#define voter_id -1
#define mySerial Serial2

LiquidCrystal_I2C lcd(0x3F,16,2);
MFRC522 mfrc522(SS_PIN, RST_PIN);
HTTPClient http;

// initialize fingerprint sensor
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup() {
  pinMode(buzzerPin, OUTPUT);
  Serial.begin(115200); 
  ConnectToWifi();
  initiateRfid();
  buzzer("success");

  // push button init
  pinMode(btn1, INPUT_PULLUP);
  pinMode(btn2, INPUT_PULLUP);
  pinMode(btn3, INPUT_PULLUP);
  pinMode(btn4, INPUT_PULLUP);

  // fingerpritn setup 
  // set the data rate for the sensor serial port
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }
}

void loop() {
  delay(500);
  lcdClear();
  lcdPrint(0,0, "Scan your card..");

  // scan card
  while(rfid_id == ""){
    rfid_id = readRfid();
  }
  int id = verifyRfid(rfid_id);
  Serial.println(rfid_id);
  if(id == -1){
    // Rfid not found
    lcdClear();
    lcdPrint(0,0, "RFID not found..");
    buzzer("error");
    rfid_id = "";
    delay(2000);
  }else if (id == 0){
    // Network / Other errors
    lcdClear();
    lcdPrint(0,0, "Network error..");
    buzzer("error");
    rfid_id = "";
    delay(2000);
  }else if (id == -2){
    // if already voted
    lcdClear();
    lcdPrint(0, 0, "Already Voted.");
    rfid_id = "";
    buzzer("already_voted");
    delay(2000);
  }
  else if(id > 0){
    // Rfid Found
    lcdClear();
    lcdPrint(0,0, "RFID found..");
    lcdPrint(1,0,user_name);
    buzzer("succes");
    delay(2000);
    rfid_id = ""; // temp

    //fingerprint Verification
    lcdClear();
    lcdPrint(0, 0, "Finger scan.");
    getFingerprintID();
    while(fingerprint_status != 1 && fingerprint_attempt < 2){
      delay(100);
      getFingerprintID();
    }
    fingerprint_attempt= 0;
    Serial.println("fing Status: ");
    Serial.println(fingerprint_status);
    if(fingerprint_status == 0 || fingerprint_status == 3){
      lcdClear();
      lcdPrint(0, 0, "Fingerprint");
      lcdPrint(1, 4, "Not Found");
      buzzer("error");
      delay(2000);
    }else{
    lcdClear();
    lcdPrint(0, 0, "Fingerprint");
    lcdPrint(1, 4, "Found.");
    buzzer("succes");
    delay(1000);
    // cast vote after rfid and fingerpint are ok 
    castVote();
    fingerprint_status = 0;
    }
  
  }else{
    rfid_id = "";
  }
}

void castVote(){
  lcdClear();
  lcdPrint(0,0, "Cast your vote");
  lcdPrint(1,2, "Using Buttons");
  int button = getButtonInput();
  lcdClear();
  delay(500);
  lcdPrint(0,0, "Confirm vote");
  lcdPrint(1,0, "Press Same button");
  int button2 = getButtonInput();
  if(button == button2){
    lcdClear();
    lcdPrint(0, 0, "Thank you");
    lcdPrint(1, 2, "for voting.");
    uploadVote(user_id, button);
    buzzer("succes");
    delay(1500);
    //  TODO: Upload Vote
  }else{
    lcdClear();
    lcdPrint(0, 0, "Conformation faild");
    lcdPrint(1, 2, "");
    buzzer("error");
  }
}
int uploadVote(String id, int vote){
  Serial.println("Test1");
  if(WiFi.status() == WL_CONNECTED){
    Serial.print("Making POST request to ");
    Serial.println(CAST_VOTE_URL);
    // http request
    http.begin(CAST_VOTE_URL);
    http.addHeader("id", id);
    http.addHeader("vote", String(vote));
    Serial.println("Test2");
    int responseCode = http.POST("{}");
    Serial.println("Test3");
    Serial.println(responseCode);
    return 1;
  }else{
    return 0;
  }
}
void ConnectToWifi(){
  //lcd setup
  lcdSetup();
  // Connect to wifi and print out ip address
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  lcdPrint(0,0, "Connecting to");
  lcdPrint(1,0, ssid);

  while(WiFi.status() != WL_CONNECTED) {
    // loop and wait for connection
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  lcdClear();
  lcdPrint(1,0, "IP: "+WiFi.localIP().toString());
  delay(2000);
  lcdClear();
  Serial.println(WiFi.localIP()); // print current ip;
}

// lcd functions
void lcdSetup(){
  lcd.init();
  lcd.clear();         
  lcd.backlight();      // Make sure backlight is on
}
void lcdPrint(uint8_t row, uint8_t position, String message ){
  lcd.setCursor(position,row);
  lcd.print(message);
}
void lcdClear(){
  lcd.clear();
}

// rfid functions
// initiate rfid scanner
void initiateRfid(){
  SPI.begin();			// Init SPI bus
	mfrc522.PCD_Init();		// Init MFRC522
	delay(200);				// Optional delay. Some board do need more time after init to be ready, see Readme
}

String readRfid() 
{
  
  if ( ! mfrc522.PICC_IsNewCardPresent()) 
  {
    return "";
  }
  
  if ( ! mfrc522.PICC_ReadCardSerial()) 
  {
    return "";
  }
 
  Serial.print("UID tag :");
  String content= "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++) 
  {
     Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
     Serial.print(mfrc522.uid.uidByte[i], HEX);
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }

  content.toUpperCase();
  return content.substring(1);
}

int verifyRfid(String rfid_tag){
  lcdClear();
  lcdPrint(0, 0, "Verifying RFID.");

  if(WiFi.status() == WL_CONNECTED){
  Serial.print("Making POST request to ");
  Serial.println(RFID_MATCH_URL);

    // http request
    http.begin(RFID_MATCH_URL);
    http.addHeader("rfid", rfid_tag);

    int responseCode = http.POST("{}");
    while(responseCode == -2){
      responseCode = http.POST("{}");
    }
    Serial.print("Request Status: ");
    Serial.println(responseCode);

    if(responseCode == 200){
      String data = http.getString();
      int i1 = data.indexOf(",");
      user_id = data.substring(0,i1);
      user_name = data.substring(i1+1, -1);
      http.end();
      return 1;
    }else if(responseCode == 444){
      http.end();
      return -1;
    }else if (responseCode == 445){
      return -2;
    }else{
      http.end();
      return 0;
    }
    http.end();
  }else{
    return 0;
  }
}

void buzzer(String type){
  if(type == "error"){
    digitalWrite(buzzerPin, HIGH);
    delay(300);
    digitalWrite(buzzerPin, LOW);
    delay(200);
    digitalWrite(buzzerPin, HIGH);
    delay(300);
    digitalWrite(buzzerPin, LOW);
  }else if(type == "succes"){
    digitalWrite(buzzerPin, HIGH);
    delay(500);
    digitalWrite(buzzerPin, LOW);
  }else if (type == "already_voted"){
    digitalWrite(buzzerPin, HIGH);
    delay(300);
    digitalWrite(buzzerPin, LOW);
    delay(200);
    digitalWrite(buzzerPin, HIGH);
    delay(300);
    digitalWrite(buzzerPin, LOW);
    delay(200);
    digitalWrite(buzzerPin, HIGH);
    delay(300);
    digitalWrite(buzzerPin, LOW);
  }
}


int getButtonInput(){
  int buttonPressed = 0;
  while(buttonPressed == 0){
    if(digitalRead(btn1) == 0){
      buttonPressed = 1;
      return 1;
    }
    if(digitalRead(btn2) == 0){
      buttonPressed = 2;
      return 2;
    }
    if(digitalRead(btn3) == 0){
      buttonPressed = 3;
      return 3;
    }
    if(digitalRead(btn4) == 0){
      buttonPressed = 4;
      return 4;
    }
  }
  return 0;
}


uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      fingerprint_attempt= fingerprint_attempt+1;
      break;
    case FINGERPRINT_NOFINGER:
      // no finger print found
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    fingerprint_status = 1;
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    fingerprint_status = 0;
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }
  
  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  Serial.println("Finger Comp");
  Serial.println(finger.fingerID);
  Serial.println(user_id.toInt());
  if(finger.fingerID != user_id.toInt()){
    fingerprint_status = 3;
  }
  return finger.fingerID;
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}