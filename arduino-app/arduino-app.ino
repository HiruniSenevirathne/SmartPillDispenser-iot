/**
    SMART PILL DISPENSER
*/

#include <Servo.h>
#include <FirebaseArduino.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
/*
  SIGNAL WIRES
  white   =>  Main disk servo
  gray    =>  IR sensor
  purple  =>  Door servo
*/

#define pinDiskServo D1
#define pinDoorServo D2
#define pinIRButton  D3
#define BUZZER D5
#define switchDelay 1000

//UTC+ 5H 30M to get Sri lanka time
#define UTC_TIMEZONE_OFFSET  19800

//WIFI settings
#define WIFI_SSID "SLT-4G_B290A"
#define WIFI_PASSWORD "prolink12345"

#define LED_POWER A0
#define LED_WIFI D8

//Firebase project settings
#define FIREBASE_URL "smartpilldispenser-8714f-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_DB_SECRET "7J2hPt4W2478d2msFGWClQgmAqtl0IMJEBPBqlXD"

//Device User Firebase UID
String DEVICE_ID = "vEICUqTCuddiKoGfFx4C6by6YeF3";

/**
   Door angles
*/
#define doorOpenAng 90
#define doorCloseAng 182

/**
   Disk properties
*/
#define diskResetAng 0
#define diskPos1 0
#define diskPos2 48
#define diskPos3 90
#define diskPos4 135
#define diskPos5 180

//***
#define diskPosDelta 0

Servo diskServo;
Servo doorServo;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_TIMEZONE_OFFSET, 60000);



String scheduleReadPath = "";
String lastData = "";
String newData = "";

//Seperated Schedule string data
int foundScheduleIdx = 0;
String scheduleStrList[10] = {};

bool scheduleAlarm10m[10] = {false, false, false, false, false, false, false, false, false, false};
bool scheduleAlarm5m[10] = {false, false, false, false, false, false, false, false, false, false};
bool scheduleAlarm0m[10] = {false, false, false, false, false, false, false, false, false, false};

int swtichStateVal = 0;
long switchStateOnTime = 0;

void setup() {
  diskServo.attach(pinDiskServo);
  doorServo.attach(pinDoorServo);
  pinMode(pinIRButton, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_POWER, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);

  digitalWrite(LED_POWER, HIGH);
  digitalWrite(LED_WIFI, LOW);

  Serial.begin(115200);
  initScript();
}

void initScript() {
  //  delay(2000);
  //  doorOpen();
  //  delay(2000);
  doorClose();
  //  diskMoveResetAng();
  initLibClients();
  settingInitialScheduleData();

}

int t2 = 0;
int state = 1;
int functionShift = 0;
void loop() {

  if (functionShift > 5) {
    functionShift = 0;
    //read stream for schedules
    firebaseSteramListener();

    //use to run buz alarm
    checkTimeSchedule("");
  } else {
    //getting ir button value for press button logic
    irSwitchRead();
    functionShift++;
  }

  delay(50);
  int t1 = millis();

  if (t1 - t2 > 5000) {
    if (WiFi.status() == WL_CONNECTED) {
      digitalWrite(LED_WIFI, HIGH);
    } else {
      digitalWrite(LED_WIFI, LOW);
    }
    t2 = millis();
  }
}


//listener for firebase data changes
void firebaseSteramListener() {

  //only need when long term device on
  //  updateScheduleRef();

  //check if streaming failed and try to restart
  if (Firebase.failed()) {
    Serial.println("streaming error");
    Serial.println(Firebase.error());
    delay(1000);
    Firebase.stream(scheduleReadPath);
    return;
  }

  if (Firebase.available()) {
    //get the event
    FirebaseObject event = Firebase.readEvent();
    newData = event.getString("data");

    if (newData != lastData && newData.length() > 2) {

      Serial.print("day_schedule_stream_1: ");
      Serial.println(newData);

      //set streaming again
      Firebase.stream(scheduleReadPath);
      lastData = "" + newData;
      saveNewSchedule(newData);
      sepSchedules();
    }
  }
}


//when device start get current date and set shedule stored path for firebase
//this path use to listern getting shedules
void updateScheduleRef() {
  scheduleReadPath = "/devices/";
  scheduleReadPath +=  getDateString();
  scheduleReadPath += "/";
  scheduleReadPath += DEVICE_ID;

  Serial.print("New Stream path:");
  Serial.println(scheduleReadPath);
}


//getting current date string fromated as YYYY-MM-DD
String getDateString() {
  timeClient.update();
  time_t epochTime = timeClient.getEpochTime();

  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime);
  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon + 1;
  int currentYear = ptm->tm_year + 1900;

  String cm = String(currentMonth);
  String cd = String(monthDay);
  if (cm.length() < 2) {
    cm = "0" + cm;
  }
  if (cd.length() < 2) {
    cd = "0" + cd;
  }
  String currentDate = String(currentYear) + "-" + cm + "-" + cd;
  return currentDate;
}

//When firebase listener recive new data it save in eeprom
void saveNewSchedule(String newData) {
  uint addr = 0;
  struct {
    uint val = 0;
    char str[255] = "";
  } data;
  EEPROM.get(addr, data);
  Serial.println("Old values are: " + String(data.val) + "," + String(data.str));

  int n = newData.length();
  char char_array[n + 1];
  strcpy(char_array, newData.c_str());

  data.val = n;
  strncpy(data.str, char_array , 255);

  //   replace values in byte-array cache with modified data
  //   no changes made to flash, all in local byte-array cache
  EEPROM.put(addr, data);
  EEPROM.commit();
  data.val = 0;
  strncpy(data.str, "", 255);
  EEPROM.get(addr, data);
}


//getting shedule items from stored comma separated string
void sepSchedules() {

  //Schedule string example data:  "1647320719841-11-30-1,1647321922188-11-50-2,1647343479811-17-30-3,"
  String nStr = "";
  foundScheduleIdx = 0;
  //resetting old schedule strings
  for (int i = 0; i < 10; i++) {
    scheduleStrList[i] = "";
  }

  for (int i = 0; i < lastData.length(); i++) {
    char nChar = lastData.charAt(i);
    if (nChar == ',') {
      if (nStr.length() > 2 && foundScheduleIdx < 10) {
        scheduleStrList[foundScheduleIdx] = nStr;
        scheduleAlarm10m[foundScheduleIdx] = false;
        scheduleAlarm5m[foundScheduleIdx] = false;
        scheduleAlarm0m[foundScheduleIdx] = false;
        foundScheduleIdx++;
      }
      nStr = "";
    } else {
      nStr += String(nChar);
    }
  }
}

//check current time can run alarm for any schedule item    => when argument tag=""
//validate button press time actualy have a scheduled item  => when argument tag="update_status"
void checkTimeSchedule(String tag) {

  //lastData
  //timeClient.update();
  //time_t epochTime = timeClient.getEpochTime();

  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  //  Serial.print("time h:");
  //  Serial.print(currentHour);
  //  Serial.print(",m:");
  //  Serial.println(currentMinute);
  String foundDispensableScheduleId = "";
  for (int k = 0; k < foundScheduleIdx; k++) {

    String currentScheduleId = "";
    int hh = -1;
    int mm = -1;
    String slotTag = "";
    int todayMints = currentHour * 60 + currentMinute;

    int fieldIndex = 0;
    String nStr = "";


    for (int i = 0; i < scheduleStrList[k].length(); i++ ) {

      char nChar = scheduleStrList[k].charAt(i);
      if (nChar == '-') {
        if (fieldIndex == 0) {
          currentScheduleId = "" + nStr;
        } else if (fieldIndex == 1) {
          hh =  nStr.toInt();
        } else if (fieldIndex == 2) {
          mm =  nStr.toInt();
        }

        fieldIndex++;
        nStr = "";
      } else {
        nStr += String(nChar);
        if (i == scheduleStrList[k].length() - 1) {
          slotTag = "" + nStr;
        }
      }
    }

    if (hh >= 0 && mm >= 0 && currentScheduleId.length() > 2) {

      //      Serial.print("sheduleId:");
      //      Serial.print(currentScheduleId);
      //      Serial.print("hh:");
      //      Serial.print(hh);
      //      Serial.print("mm:");
      //      Serial.println(mm);
      //      Serial.print("slot tag:");
      //      Serial.println(slotTag);
      int schedMints = hh * 60 + mm;

      int diff =  schedMints - todayMints ;

      if (tag == "update_status" ) {
        Serial.println("888");
        Serial.println(foundDispensableScheduleId);
        Serial.println(foundDispensableScheduleId.length());
        if (abs(diff) < 6 && foundDispensableScheduleId.length() < 2) {
          Serial.println("999");


          beep(100);
          beep(50);
          beep(100);

          foundDispensableScheduleId = "" + currentScheduleId;

          if ( foundDispensableScheduleId.length() > 2) {
            //send firebase command to update schedule item is taken
            setScheduleItemTaken(foundDispensableScheduleId, slotTag);
          }
        }
      } else {
        if (diff > 0 && diff <= 10) {
          if ( diff > 5) {

            if ( scheduleAlarm10m[k] == false) {
              //10m alarm
              beep(1000);
              scheduleAlarm10m[k] = true;
            }
          } else if (scheduleAlarm5m[k] == false) {
            //5m alarm
            beep(500);
            beep(1000);
            scheduleAlarm5m[k] = true;
          } else if (diff < 2 && scheduleAlarm0m[k] == false) {

            //0m alarm
            beep(100);
            beep(200);

            scheduleAlarm0m[k] = true;
          }
        }
      }
    }
  }

  if (tag == "update_status" && foundDispensableScheduleId.length() < 2) {
    Serial.println("aaa");
    beep(30);
    beep(80);
    beep(80);

  }
}

// save schedule item  "taken" status = 1
void setScheduleItemTaken(String scheduleId, String slotTag) {

  moveToSlot(slotTag);

  
    Serial.println("setting schedule item taken:" + scheduleId);
    String dbRef = "/patients/" + DEVICE_ID + "/schedule/";
    dbRef +=  scheduleId + "/status";
    Serial.println(dbRef);
    Firebase.setString(dbRef, "1");

}

// IR button value reading
void irSwitchRead() {

  int v = digitalRead(pinIRButton);

  if (v == 0) {
    if (swtichStateVal == 0) {
      switchStateOnTime = millis();
      swtichStateVal = 1;
    } else if (swtichStateVal == 1 && switchStateOnTime > 0 && switchStateOnTime + switchDelay < millis()) {

      swtichStateVal = -1;
      switchStateOnTime = 0;
      beep(600);
      checkTimeSchedule("update_status");
    }

  } else {
    swtichStateVal = 0;
    switchStateOnTime = 0;

  }
}
/**
   Disk move functions

*/

void moveToSlot(String slot) {
  Serial.println("Move to slot: " + slot);
  if (slot == "1") {
    moveDisk_(diskPos1);
    delay(2000);
    doorOpen();
    delay(2000);
    doorClose();
  } else if (slot == "2") {
    moveDisk_(diskPos2);
    delay(2000);
    doorOpen();
    delay(2000);
    doorClose();
  } else if (slot == "3") {
    moveDisk_(diskPos3);
    delay(2000);
    doorOpen();
    delay(2000);
    doorClose();
  } else if (slot == "4") {
    moveDisk_(diskPos4);
    delay(2000);
    doorOpen();
    delay(2000);
    doorClose();
  }


}
void diskMoveResetAng() {
  moveDisk_(diskResetAng);
}

void moveDisk_(int degAng) {
  diskServo.write(degAng + diskPosDelta);

}
/**
   Door move functions

*/
void doorOpen() {
  moveDoor_(doorOpenAng);
}
void doorClose() {
  moveDoor_(doorCloseAng);
}
void moveDoor_(int degAng) {
  doorServo.write(degAng);
}
void initLibClients() {
  // connect to wifi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("connecting");
  int x = 0;
  while (WiFi.status() != WL_CONNECTED && x < 30) {
    Serial.print(".");
    x++;
    delay(500);
  }
  delay(2000);
  // Initialize a NTPClient to get time
  timeClient.begin();

  delay(2000);
  //update ref according to the current date  and device id
  updateScheduleRef();

  //begin Firebase
  Firebase.begin(FIREBASE_URL, FIREBASE_DB_SECRET);

}
void settingInitialScheduleData() {
  /*
    EEPROM DATA
  */
  uint addr = 0;

  // fake data
  struct {
    uint val = 0;
    char str[255] = "";
  } data;

  EEPROM.begin(512);
  EEPROM.get(addr, data);
  Serial.println("eeprom values are: " + String(data.val) + "," + String(data.str));


  //get the last updated value for the device
  newData = Firebase.getString(scheduleReadPath);

  if (newData.length() < 2) {
    newData = String(data.str);
    Serial.print("init schedules from eeprom:");
  } else {
    Serial.print("init schedules from firebase:");
  }
  Serial.println(newData);
  lastData = "" + newData;
  sepSchedules();
  //start streaming the data for the updating value
  Firebase.stream(scheduleReadPath);
}


//beep Buzzer
void beep(int d) {
  digitalWrite(BUZZER, HIGH);
  delay(d);
  digitalWrite(BUZZER, LOW);
  delay(d);
}
