/*
 * uAlarm.ino
 *
 * Created: 7/18/2014 1:33:23 PM
 * Author: Ryan Hamilton
 */ 

#include "pitches.h"
#include "Timezone.h"
#include "TinyGPS++.h"
#include "Time.h"
#include "Streaming.h"
#include "GPSTimeZoneLookup.h" 
#include "Flash.h"

//for the Adafruit 7-segment display I2C backpack
#include <Wire.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GFX.h>

//#define DEBUG
//#define DEBUG_FAKE_GPS
//#define DEBUG_PRINT_BRIGHTNESS
//#define DEBUG_PRINT_LOOP_SPEED
//#define DEBUG_TEST_TONE

#define GPS_TIME_UPDATE_INTERVAL_SECS 10
#define SNOOZE_DURATION_MINS 5

// define pins
//buttons
const byte alarmAMPMPin = 49;
const byte snoozePin = 43;
const byte alarmOnPin = 33;
//LCD control
const byte LCDminPin = A9;
const byte LCDmaxPin = A10;
const byte photoCellPin = A8;
const byte LCDSDApPin = 20;
const byte LCDSCLPin = 21;
//speaker
const byte speakerPin = 11;
//set alarm hour rotary switch
const byte setAlarmHour1Pin = 34;
const byte setAlarmHour2Pin = 23;
const byte setAlarmHour3Pin = 32;
const byte setAlarmHour4Pin = 22;
const byte setAlarmHour5Pin = 30;
const byte setAlarmHour6Pin = 24;
const byte setAlarmHour7Pin = 28;
const byte setAlarmHour8Pin = 26;
const byte setAlarmHour9Pin = 38;
const byte setAlarmHour10Pin = 53;
const byte setAlarmHour11Pin = 36;
const byte setAlarmHour12Pin = 40;
//set alarm minute rotary switch
const byte setAlarmMin00Pin = 48;
const byte setAlarmMin10Pin = 52;
const byte setAlarmMin20Pin = 46;
const byte setAlarmMin30Pin = 50;
const byte setAlarmMin40Pin = 44;
const byte setAlarmMin50Pin = 42;

//GPS - GPS is connected to hardware serial1 (TX on D18, RX on D19). Just read Serial1.begin(9600)
TinyGPSPlus gps; //TinyGPS++ object

boolean locationFound = false;

time_t nextDisplay = 0; // when to next update the numeric display
#define DISPLAY_UPDATE_INTERVAL 1 // update the display every one second
#define DISPLAY_ALARM_TIME_DURATION 5 // when the alarm time is displayed, show it for this many seconds

boolean alarmOn;
byte alarmHour = 12; // 24 hour time
byte alarmMin = 0;
//boolean alarmAMPM = true; // false == AM, true == PM
boolean snoozePressed = false;
boolean currentlyAlarming = false;

//Timezone library setup
TimeChangeRule timeChangeRuleStart;
TimeChangeRule timeChangeRuleStop;

#ifdef DEBUG_FAKE_GPS
// A sample NMEA stream.
const char *gpsStream =
"$GPRMC,045103.000,A,3014.1984,N,09749.2872,W,0.67,161.46,030913,,,A*7C\r\n"
"$GPGGA,045104.000,3014.1985,N,09749.2873,W,1,09,1.2,211.6,M,-22.5,M,,0000*62\r\n"
"$GPRMC,045200.000,A,3014.3820,N,09748.9514,W,36.88,65.02,030913,,,A*77\r\n"
"$GPGGA,045201.000,3014.3864,N,09748.9411,W,1,10,1.2,200.8,M,-22.5,M,,0000*6C\r\n"
"$GPRMC,045251.000,A,3014.4275,N,09749.0626,W,0.51,217.94,030913,,,A*7D\r\n"
"$GPGGA,045252.000,3014.4273,N,09749.0628,W,1,09,1.3,206.9,M,-22.5,M,,0000*6F\r\n";
#endif

//7 segment LCD display setup
Adafruit_7segment sevenSegmentDisplay = Adafruit_7segment();

// display modes
typedef enum {
    LCDTime, LCDAlarm
} LCDMode;
LCDMode currentMode = LCDTime;

void setup() {
    //switches and buttons
    pinMode(alarmAMPMPin, INPUT_PULLUP);
    pinMode(snoozePin, INPUT_PULLUP);
    pinMode(alarmOnPin, INPUT_PULLUP);

    //set alarm hour rotary switch
    pinMode(setAlarmHour1Pin, INPUT_PULLUP);
    pinMode(setAlarmHour2Pin, INPUT_PULLUP);
    pinMode(setAlarmHour3Pin, INPUT_PULLUP);
    pinMode(setAlarmHour4Pin, INPUT_PULLUP);
    pinMode(setAlarmHour5Pin, INPUT_PULLUP);
    pinMode(setAlarmHour6Pin, INPUT_PULLUP);
    pinMode(setAlarmHour7Pin, INPUT_PULLUP);
    pinMode(setAlarmHour8Pin, INPUT_PULLUP);
    pinMode(setAlarmHour9Pin, INPUT_PULLUP);
    pinMode(setAlarmHour10Pin, INPUT_PULLUP);
    pinMode(setAlarmHour11Pin, INPUT_PULLUP);
    pinMode(setAlarmHour12Pin, INPUT_PULLUP);

    //set alarm minute rotary switch
    pinMode(setAlarmMin00Pin, INPUT_PULLUP);
    pinMode(setAlarmMin10Pin, INPUT_PULLUP);
    pinMode(setAlarmMin20Pin, INPUT_PULLUP);
    pinMode(setAlarmMin30Pin, INPUT_PULLUP);
    pinMode(setAlarmMin40Pin, INPUT_PULLUP);
    pinMode(setAlarmMin50Pin, INPUT_PULLUP);

    //speaker
    pinMode(speakerPin, OUTPUT);

    //LCD control
    pinMode(LCDminPin, INPUT);
    pinMode(LCDmaxPin, INPUT);
    pinMode(photoCellPin, INPUT);
    pinMode(LCDSDApPin, INPUT);
    pinMode(LCDSCLPin, INPUT);


    //turn on the display and set it to say "GPS"
    //TODO

    //communicate output to a PC
    Serial.begin(9600);

    #ifdef DEBUG_FAKE_GPS
    while (*gpsStream) if (gps.encode(*gpsStream++)) displayGPSInfo();
    #else
    Serial1.begin(9600);
    #endif

    //I2C communication with display at address 0x70;
    sevenSegmentDisplay.begin(0x70);
    sevenSegmentDisplay.print(1200);
    sevenSegmentDisplay.writeDisplay();
    sevenSegmentDisplay.blinkRate(3);
    sevenSegmentDisplay.setBrightness(0);
}
void loop() {
    static unsigned long loopCount = 0;
    loopCount++;

    readGPS();
    
    //the first time we identify our location, calculate the time zone offset
    if (!locationFound && gps.location.isValid()) {
        locationFound = true;
        Serial.println("loop: first location lock");
        //based on our location, determine our GMT offset, and whether our location uses DST
        setTimeZoneAndDST(gps.location.lat(), gps.location.lng());
        //stop blinking the display
        sevenSegmentDisplay.blinkRate(0);
        //set up sync provider for Time.h
        setSyncProvider(gpsTimeSync);
        setSyncInterval(GPS_TIME_UPDATE_INTERVAL_SECS);
        setTime(gpsTimeSync());
        delay(10);
    }

    readSwitchesUpdateAlarmTime();

    #ifdef DEBUG_TEST_TONE
        if(alarmOn) {
            startAlarm();
        } else {stopAlarm();}
    #endif
    
    //if the alarm is turned on
    if(alarmOn) {
        //check to see if it is currently time to trigger the alarm
        if(isTimeToAlarm()) startAlarm();
        if(currentlyAlarming){
            playAlarmTone();
            //if it is currently alarming see if someone hit snooze and if so act on it
            if(snoozePressed) hitSnooze();
        }
    }
    else { //alarm switch is off
        if(currentlyAlarming) stopAlarm();
    }

    //show the right stuff on the LCD
    switch (currentMode) {
        case LCDTime:
            //display the current clock time based on GPS GMT, time zone and DST status
            if(timeStatus() != timeNotSet)    
            {
                if( now() >= nextDisplay ) {//update the display only if the time has changed by one second
                    nextDisplay = now() + DISPLAY_UPDATE_INTERVAL;
                    digitalClockSerialDisplay();
                    time_t  lt = calcLocalTime(now());
                    writeTimeDisplay(hour(lt),minute(lt),alarmOn);
                }
            }        
        break;
        case LCDAlarm: // display selected alarm time for a few seconds
            nextDisplay = now() + DISPLAY_ALARM_TIME_DURATION;
            currentMode = LCDTime;
            digitalClockSerialDisplay();
            writeTimeDisplay(alarmHour,alarmMin,true);
    }

#ifdef DEBUG_PRINT_LOOP_SPEED
    if((loopCount % 10000)==0){
        digitalClockSerialDisplay();
        static unsigned long lastMillis = 0;
        Serial.print("loops per sec: ");
        Serial.println(10000000/(millis()-lastMillis));
        lastMillis = millis();
    }  
#endif 

}
void setTimeZoneAndDST(float myLat, float myLong) {
    Serial.print("setTimeZoneAndDST: coordinates: ");
    Serial.print(myLat);
    Serial.print(", ");
    Serial.println(myLong);

    GPSTimeZoneLookup tz(myLat, myLong);

    int myGMTOffset = tz.GMTOffset;
    boolean myLocationUsesDST = tz.implementsDST;

    Serial.print("    identified time zone: ");
    switch(myGMTOffset){
        case -10: //Pacific/Honolulu
        Serial.println("US Hawaii?Aleutian Time Zone (Honolulu)");
        timeChangeRuleStart.week    = Second;
        timeChangeRuleStart.dow     = Sun;
        timeChangeRuleStart.month   = Mar;
        timeChangeRuleStart.hour    = 2;
        timeChangeRuleStart.offset  = -600;

        timeChangeRuleStop.week    = First;
        timeChangeRuleStop.dow     = Sun;
        timeChangeRuleStop.month   = Nov;
        timeChangeRuleStop.hour    = 2;
        timeChangeRuleStop.offset  = -600;
        break;
        case -9: //America/Anchorage
        Serial.println("US Alaska Time Zone (Anchorage)");
        timeChangeRuleStart.week    = Second;
        timeChangeRuleStart.dow     = Sun;
        timeChangeRuleStart.month   = Mar;
        timeChangeRuleStart.hour    = 2;
        timeChangeRuleStart.offset  = -480;

        timeChangeRuleStop.week    = First;
        timeChangeRuleStop.dow     = Sun;
        timeChangeRuleStop.month   = Nov;
        timeChangeRuleStop.hour    = 2;
        timeChangeRuleStop.offset  = -540;
        break;
        case -8: //America/Los_Angeles
        Serial.println("US Pacific Time Zone (Las Vegas, Los Angeles)");
        timeChangeRuleStart.week    = Second;
        timeChangeRuleStart.dow     = Sun;
        timeChangeRuleStart.month   = Mar;
        timeChangeRuleStart.hour    = 2;
        timeChangeRuleStart.offset  = -420;

        timeChangeRuleStop.week    = First;
        timeChangeRuleStop.dow     = Sun;
        timeChangeRuleStop.month   = Nov;
        timeChangeRuleStop.hour    = 2;
        timeChangeRuleStop.offset  = -480;
        break;
        case -7: //America/Phoenix or America/Denver
        if (myLocationUsesDST) {
            Serial.println("US Mountain Time Zone (Denver, Salt Lake City)");
            timeChangeRuleStart.week    = Second;
            timeChangeRuleStart.dow     = Sun;
            timeChangeRuleStart.month   = Mar;
            timeChangeRuleStart.hour    = 2;
            timeChangeRuleStart.offset  = -360;

            timeChangeRuleStop.week    = First;
            timeChangeRuleStop.dow     = Sun;
            timeChangeRuleStop.month   = Nov;
            timeChangeRuleStop.hour    = 2;
            timeChangeRuleStop.offset  = -420;
        }
        else {
            Serial.println("Arizona is US Mountain Time Zone but does not use DST");
            timeChangeRuleStart.week    = Second;
            timeChangeRuleStart.dow     = Sun;
            timeChangeRuleStart.month   = Mar;
            timeChangeRuleStart.hour    = 2;
            timeChangeRuleStart.offset  = -420;

            timeChangeRuleStop.week    = First;
            timeChangeRuleStop.dow     = Sun;
            timeChangeRuleStop.month   = Nov;
            timeChangeRuleStop.hour    = 2;
            timeChangeRuleStop.offset  = -420;
        }
        break;
        case -6: //America/Chicago - US Central Time Zone (Chicago, Houston)
        Serial.println("US Central Time Zone (Chicago, Houston)");
        timeChangeRuleStart.week    = Second;
        timeChangeRuleStart.dow     = Sun;
        timeChangeRuleStart.month   = Mar;
        timeChangeRuleStart.hour    = 2;
        timeChangeRuleStart.offset  = -300;

        timeChangeRuleStop.week    = First;
        timeChangeRuleStop.dow     = Sun;
        timeChangeRuleStop.month   = Nov;
        timeChangeRuleStop.hour    = 2;
        timeChangeRuleStop.offset  = -360;
        break;
        case -5: //America/New_York
        Serial.println("US Eastern Time Zone (New York, Detroit)");
        timeChangeRuleStart.week    = Second;
        timeChangeRuleStart.dow     = Sun;
        timeChangeRuleStart.month   = Mar;
        timeChangeRuleStart.hour    = 2;
        timeChangeRuleStart.offset  = -240;

        timeChangeRuleStop.week    = First;
        timeChangeRuleStop.dow     = Sun;
        timeChangeRuleStop.month   = Nov;
        timeChangeRuleStop.hour    = 2;
        timeChangeRuleStop.offset  = -300;
        break;
        default:
        Serial.println("Not found! Offset from GMT outside range {-10 to -5}");
    }

    /*
    //set up local Timezone object instance
    Timezone tz(timeChangeRuleStart,timeChangeRuleStop);
    localTimeZone = &tz;
    
    Serial.print("    before TZ conversion: ");
    digitalClockDisplay();
    setTime(localTimeZone->toLocal(now()));
    Serial.print("    after TZ conversion: ");
    digitalClockDisplay();
    Serial.println();
    */
}
void writeTimeDisplay(byte hours, byte minutes, bool alarmIsSet) {
    // write to the LCD display
    //bit map for TRUE for turning on dots
    static const uint8_t lowerLeftDot = 0x04;
    static const uint8_t upperLeftDot = 0x08;
    static const uint8_t decimalPoint = 0x10;
    static const uint8_t colon = 0x02;
    uint8_t displayMask = colon;
        
    // hour is 1-24, convert to 12 hours with PM
    boolean PM = false;
    if (hours >= 12) PM = true;
    if (hours>12) hours -= 12;
    if (hours == 0) hours = 12;
    
    sevenSegmentDisplay.print(hours*100 + minutes);
    if(PM)          displayMask = displayMask | upperLeftDot;
    if(alarmIsSet)  displayMask = displayMask | lowerLeftDot;
    sevenSegmentDisplay.writeDigitRaw(2,displayMask);
    sevenSegmentDisplay.setBrightness(calcLEDBrightness());
    sevenSegmentDisplay.writeDisplay();
}
byte AlarmHourSwitchValue() {
    byte switchValue;
    static byte prevSwitchValue = 0;
    //set alarm hour rotary switch. Switch label values are 1,2,3,4,5,6,7,8,9,10,11,12. The number 12 switch position returns 0. If there is no position selected (switch is mid-twist) return the previous value.

    //switches have pullup resistors, so LOW == connected
    if (digitalRead(setAlarmHour1Pin) == LOW) {
        switchValue = 1;
    }
    else if (digitalRead(setAlarmHour2Pin) == LOW) {
        switchValue = 2;
    }
    else if (digitalRead(setAlarmHour3Pin) == LOW) {
        switchValue = 3;
    }
    else if (digitalRead(setAlarmHour4Pin) == LOW) {
        switchValue = 4;
    }
    else if (digitalRead(setAlarmHour5Pin) == LOW) {
        switchValue = 5;
    }
    else if (digitalRead(setAlarmHour6Pin) == LOW) {
        switchValue = 6;
    }
    else if (digitalRead(setAlarmHour7Pin) == LOW) {
        switchValue = 7;
    }
    else if (digitalRead(setAlarmHour8Pin) == LOW) {
        switchValue = 8;
    }
    else if (digitalRead(setAlarmHour9Pin) == LOW) {
        switchValue = 9;
    }
    else if (digitalRead(setAlarmHour10Pin) == LOW) {
        switchValue = 10;
    }
    else if (digitalRead(setAlarmHour11Pin) == LOW) {
        switchValue = 11;
    }
    else if (digitalRead(setAlarmHour12Pin) == LOW) {
        switchValue = 0;
    }
    else {
        switchValue = prevSwitchValue;
    }

    prevSwitchValue = switchValue;
    return switchValue;
}
byte AlarmMinSwitchValue() {
    byte switchValue;
    static byte prevSwitchValue = 0;
    //set alarm hour rotary switch. Switch label values are 00, 10, 20, 30, 40, 50. If there is no position selected (switch is mid-twist) return the previous value.

    //switches have pullup resistors, so LOW == connected
    if (digitalRead(setAlarmMin00Pin) == LOW) {
        switchValue = 0;
    }
    else if (digitalRead(setAlarmMin10Pin) == LOW) {
        switchValue = 1;
    }
    else if (digitalRead(setAlarmMin20Pin) == LOW) {
        switchValue = 2;
    }
    else if (digitalRead(setAlarmMin30Pin) == LOW) {
        switchValue = 3;
    }
    else if (digitalRead(setAlarmMin40Pin) == LOW) {
        switchValue = 4;
    }
    else if (digitalRead(setAlarmMin50Pin) == LOW) {
        switchValue = 5;
    }
    else {
        switchValue = prevSwitchValue;
    }


    prevSwitchValue = switchValue;
    return switchValue * 10;
}
void digitalClockSerialDisplay(){
    // digital clock display of the time
    Serial.print("digitalClockDisplay: UTC: ");
    displayTime(now());
    Serial.print(" local: ");
    displayTime(calcLocalTime(now()));
    Serial.print(" alarmOn: ");
    Serial.print(alarmOn);
    Serial.print(" alarmHour: ");
    Serial.print(alarmHour);
    Serial.print(" alarmMin: ");
    Serial.print(alarmMin);
    Serial.print(" currentlyAlarming: ");
    Serial.print(currentlyAlarming);
    Serial.println();
}
void printDigits(int digits) {
    // utility function for digital clock display: prints preceding colon and leading 0
    Serial.print(":");
    if(digits < 10)
    Serial.print('0');
    Serial.print(digits);
}
void displayGPSInfo() {
    // displays GPS coordinates and UTC
    Serial.print(F("Location: "));
    if (gps.location.isValid())
    {
        Serial.print(gps.location.lat(), 6);
        Serial.print(F(","));
        Serial.print(gps.location.lng(), 6);
    }
    else
    {
        Serial.print(F("INVALID"));
    }

    Serial.print(F("  UTC: "));
    if (gps.date.isValid())
    {
        Serial.print(gps.date.month());
        Serial.print(F("/"));
        Serial.print(gps.date.day());
        Serial.print(F("/"));
        Serial.print(gps.date.year());
    }
    else
    {
        Serial.print(F("INVALID"));
    }

    Serial.print(F(" "));
    if (gps.time.isValid())
    {
        if (gps.time.hour() < 10) Serial.print(F("0"));
        Serial.print(gps.time.hour());
        Serial.print(F(":"));
        if (gps.time.minute() < 10) Serial.print(F("0"));
        Serial.print(gps.time.minute());
        Serial.print(F(":"));
        if (gps.time.second() < 10) Serial.print(F("0"));
        Serial.print(gps.time.second());
        Serial.print(F("."));
        if (gps.time.centisecond() < 10) Serial.print(F("0"));
        Serial.print(gps.time.centisecond());
    }
    else
    {
        Serial.print(F("INVALID"));
    }

    Serial.println();
}
time_t gpsTimeSync() {
    //  returns time_t if avail from gps, else returns 0
    Serial.println("gpsTimeSync: time sync");
    
    //return gpsTimeToArduinoTime(); // return time only if updated recently by gps
    if (gps.date.isUpdated() && gps.time.isUpdated()) {
        Serial.println("    time data updated, update Time");
        return gpsTimeToTime_t(); // return time only if updated recently by gps
    }
    else {
        Serial.println("    time data not updated");
        #ifdef DEBUG
        return gpsTimeToTime_t();
        #else
        return 0;
        #endif
        
    }
    
}
time_t gpsTimeToTime_t() {
    // returns time_t from gps date and time in UTC
    tmElements_t tm;

    tm.Year = gps.date.year() - 1970;
    tm.Month = gps.date.month();
    tm.Day = gps.date.day();
    tm.Hour = gps.time.hour();
    tm.Minute = gps.time.minute();
    tm.Second = gps.time.second();

    #ifdef DEBUG //for debugging lets make it always 2 minutes before 7AM
    tm.Year = gps.date.year() - 1970;
    tm.Month = gps.date.month();
    tm.Day = gps.date.day();
    tm.Hour = 11;
    tm.Minute = 59;
    tm.Second = 40;
    #endif
    
    return makeTime(tm); //UTC
}
void startAlarm() {
    //turn on the alarm
    currentlyAlarming = true;
}
void stopAlarm() {
    //turn off the alarm
    currentlyAlarming = false;
    noTone(speakerPin);
}
void hitSnooze() {
    snoozePressed = false;
    //localAlarmTime = now() + SNOOZE_DURATION_SECS;
    stopAlarm();
    alarmMin = alarmMin + SNOOZE_DURATION_MINS;
    if (alarmMin == 60) {
        alarmMin = 0;
        alarmHour++;
    }
    if (alarmHour == 25) alarmHour = 0;
    Serial.print("hitSnooze: ");
    Serial.print(alarmHour);
    Serial.print(":");
    Serial.println(alarmMin);
}
void displayTime(time_t t) {
    // digital clock display of the time
    Serial.print(hour(t));
    printDigits(minute(t));
    printDigits(second(t));
    Serial.print(" ");
    Serial.print(month(t));
    Serial.print("/");
    Serial.print(day(t));
    Serial.print("/");
    Serial.print(year(t));
}
boolean isTimeToAlarm() {
    //compare local time hour and minutes to alarm settings. returns true if it is past time to alarm
    time_t   localTime = calcLocalTime(now());

    return (alarmHour == hour(localTime) && alarmMin == minute(localTime));
}
time_t calcLocalTime(time_t t) {
    //convert time_t from UTC to local time
    if (locationFound)
    {
        static Timezone localTimeZone(timeChangeRuleStart,timeChangeRuleStop);
        return localTimeZone.toLocal(t);
    }
    else
    {
        return t;
    }
}
void readSwitchesUpdateAlarmTime() {
    //if currently snoozing and someone changes a knob, stop snoozing and just use the knob alarm time. Otherwise if snoozing do not update alarm time (snooze set a new alarm)
    //any other time, just use the knob time
    
    // HMI state
    byte	hourSwitchVal;
    byte	minSwitchVal;
    static byte	prevHourSwitchVal = 255;
    static byte	preMinSwitchVal = 255;
    static boolean prevAlarmAMPM = false;
    boolean alarmOnSwitchVal;
    boolean alarmAMPM;
    
    //read all of the switches positions
    hourSwitchVal	= AlarmHourSwitchValue(); // noon == 0
    minSwitchVal	= AlarmMinSwitchValue();
    alarmAMPM = digitalRead(alarmAMPMPin);
    //if the alarm switch is flipped off then turn off the alarm
    alarmOnSwitchVal = (digitalRead(alarmOnPin)==LOW);
    
     
    if ((hourSwitchVal != prevHourSwitchVal)|| 
        (minSwitchVal != preMinSwitchVal)   || 
        (alarmAMPM != prevAlarmAMPM)        || 
        (!alarmOn && alarmOnSwitchVal) ) // someone deliberately changed a knob position
    {
        Serial << alarmOn << alarmOnSwitchVal << ":" << prevHourSwitchVal << preMinSwitchVal << prevAlarmAMPM << "-->";
        Serial << hourSwitchVal << minSwitchVal << alarmAMPM << endl;
       alarmHour = hourSwitchVal + 12 * (byte)alarmAMPM;
        alarmMin = minSwitchVal;
        prevHourSwitchVal = hourSwitchVal;
        preMinSwitchVal = minSwitchVal;
        prevAlarmAMPM = alarmAMPM;
        alarmOn = alarmOnSwitchVal;
        currentMode = LCDAlarm;
        Serial.println("values changed");
    }
    
    // if the alarm switch is turned off, always set alarm off
    if (!alarmOnSwitchVal)
    {
        alarmOn = false;
    }
    

    //only register a snooze press if the alarm is currently active
    if(currentlyAlarming && digitalRead(snoozePin)==LOW) snoozePressed = true;
}
void readGPS() {
    //process values from the GPS
    while (Serial1.available() > 0) gps.encode(Serial1.read());
}
uint8_t calcLEDBrightness() {
    // brightness values for the Adafruit LED I2C backpack are 0-15
    int lightSensorValue = analogRead(photoCellPin);
    int LCDmax;
    int LCDmin;
    //calculate min and max brightness from the pots

    LCDmax = map(analogRead(LCDmaxPin),0,1023,0,16);
    LCDmin = map(analogRead(LCDminPin),0,1023,0,LCDmax);

    const int sensorMin = 100;
    const int sensorMax = 500;
    lightSensorValue = constrain(lightSensorValue,sensorMin,sensorMax);
    //respan the sensor output
    byte LEDBrightness = map(lightSensorValue,sensorMin,sensorMax,LCDmin,LCDmax);
        
#ifdef DEBUG_PRINT_BRIGHTNESS
    Serial.print("minpin: ");
    Serial.print(analogRead(LCDminPin));
    Serial.print(" sensor min-max: ");
    Serial.print(sensorMin);
    Serial.print("-");
    Serial.print(sensorMax);
    Serial.print(" value: ");
    Serial.print(lightSensorValue);
    Serial.print(" Setting min-max: ");
    Serial.print(LCDmin);
    Serial.print("-");
    Serial.print(LCDmax);    
    Serial.print(" brightness: ");
    Serial.println(LEDBrightness);
#endif
    
    return constrain(LEDBrightness,0,15);
	//return 15;
}

void playAlarmTone(){
    static int nextNoteTime = 0;
    const int noteDurationMillis = 300;
    const int noteGapMillis = 300;    
   
    if (millis()>=nextNoteTime){
        tone(speakerPin,NOTE_B4,noteDurationMillis);
        nextNoteTime+=noteDurationMillis+noteGapMillis;        
    }
}
