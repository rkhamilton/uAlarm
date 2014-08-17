/*
 * uAlarm.ino
 *
 * Created: 7/18/2014 1:33:23 PM
 * Author: Ryan Hamilton
 */ 

#include "pitches.h"
#include <Timezone.h>
#include <TinyGPS++.h>
#include <Time.h>

//for the Adafruit 7-segment display I2C backpack
#include <Wire.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GFX.h>

//#define DEBUG
//#define DEBUG_FAKE_GPS
//#define DEBUG_PRINT_BRIGHTNESS
//#define DEBUG_PRINT_LOOP_SPEED
#define DEBUG_TEST_TONE

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
boolean alarmAMPM = true; // false == AM, true == PM
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
    Serial.begin(115200);

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
        Serial.println("loop: first location lock");
        //based on our location, determine our GMT offset, and whether our location uses DST
        setTimeZoneAndDST(gps.location.lat(), gps.location.lng());
        locationFound = true;
        //stop blinking the display
        sevenSegmentDisplay.blinkRate(0);
        //set up sync provider for Time.h
        setSyncProvider(gpsTimeSync);
        setSyncInterval(GPS_TIME_UPDATE_INTERVAL_SECS);
        setTime(gpsTimeSync());
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
            Serial.println("showing alarm time");
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
    //define lat/long coordinates of all US citied above a population of 15000
    int _numCities = 313;
    int _minIndex = 0;
    float _minDistance = 3.0E+38; //very large distance, any point on earth will be closer
    float thisDistance;

    const float _cityCoords[313][2] = {
        {37.08339, -88.60005},
        {31.22323, -85.39049},
        {34.01426, -86.00664},
        {32.64541, -85.37828},
        {33.61427, -85.83496},
        {32.47098, -85.00077},
        {28.47688, -82.52546},
        {30.43826, -84.28073},
        {31.57851, -84.15574},
        {32.07239, -84.23269},
        {33.58011, -85.07661},
        {32.46098, -84.98771},
        {34.25704, -85.16467},
        {30.83658, -83.97878},
        {39.16532, -86.52639},
        {39.86671, -86.14165},
        {39.84338, -86.39777},
        {39.97837, -86.11804},
        {38.29674, -85.75996},
        {39.20144, -85.92138},
        {37.97476, -87.55585},
        {39.95559, -86.01387},
        {39.78504, -85.76942},
        {39.61366, -86.10665},
        {39.76838, -86.15804},
        {38.39144, -86.93111},
        {38.27757, -85.73718},
        {39.83865, -86.02526},
        {38.28562, -85.82413},
        {39.92894, -85.37025},
        {39.70421, -86.39944},
        {39.82894, -84.89024},
        {38.95922, -85.89025},
        {39.52144, -85.77692},
        {39.4667, -87.41391},
        {38.67727, -87.52863},
        {38.36446, -98.76481},
        {38.87918, -99.32677},
        {36.99032, -86.4436},
        {39.02756, -84.72411},
        {37.64563, -84.77217},
        {37.69395, -85.85913},
        {39.01673, -84.60078},
        {38.15979, -85.58774},
        {38.99895, -84.62661},
        {38.20091, -84.87328},
        {37.83615, -87.59001},
        {38.14285, -85.62413},
        {36.8656, -87.48862},
        {38.19424, -85.5644},
        {38.25424, -85.75941},
        {37.3281, -87.49889},
        {38.16007, -85.65968},
        {38.14118, -85.68774},
        {37.77422, -87.11333},
        {38.14535, -85.8583},
        {37.84035, -85.94913},
        {38.25285, -85.65579},
        {38.20007, -85.82274},
        {38.11118, -85.87024},
        {39.34589, -84.5605},
        {39.29034, -84.50411},
        {39.3995, -84.56134},
        {39.507, -84.74523},
        {39.21311, -84.59939},
        {35.04563, -85.30968},
        {36.16284, -85.50164},
        {34.99591, -85.15023},
        {35.06535, -85.24912},
        {35.01424, -85.2519},
        {36.38838, -86.44666},
        {36.20811, -86.2911},
        {36.50921, -86.885},
        {35.36202, -86.20943},
        {39.48061, -86.05499},
        {34.26759, -86.20887},
        {33.65983, -85.83163},
        {32.60986, -85.48078},
        {42.2942, -83.30993},
        {40.12448, -87.63002},
        {40.10532, -85.68025},
        {40.04115, -86.87445},
        {41.41698, -87.36531},
        {41.68199, -85.97667},
        {41.1306, -85.12886},
        {40.27948, -86.51084},
        {41.59337, -87.34643},
        {41.58227, -85.83444},
        {41.74813, -86.12569},
        {41.52837, -87.42365},
        {41.53226, -87.25504},
        {40.8831, -85.49748},
        {40.48643, -86.1336},
        {41.6106, -86.72252},
        {40.4167, -86.87529},
        {40.04837, -86.46917},
        {40.75448, -86.35667},
        {40.55837, -85.65914},
        {41.48281, -87.33281},
        {41.70754, -86.89503},
        {41.66199, -86.15862},
        {40.19338, -85.38636},
        {40.04559, -86.0086},
        {41.57587, -87.17615},
        {41.47892, -87.45476},
        {41.68338, -86.25001},
        {41.47309, -87.06114},
        {40.42587, -86.90807},
        {40.04282, -86.12749},
        {41.89755, -84.03717},
        {42.25754, -83.21104},
        {42.97225, -85.95365},
        {42.27756, -83.74088},
        {42.32115, -85.17971},
        {42.30865, -83.48216},
        {42.32226, -83.17631},
        {42.33698, -83.27326},
        {42.33143, -83.04575},
        {42.46059, -83.13465},
        {42.95947, -85.48975},
        {42.32559, -83.33104},
        {42.96336, -85.66809},
        {42.90975, -85.76309},
        {42.39282, -83.04964},
        {42.46254, -83.10409},
        {42.78752, -86.10893},
        {42.64059, -84.51525},
        {42.24587, -84.40135},
        {42.90725, -85.79198},
        {42.29171, -85.58723},
        {42.86947, -85.64475},
        {42.73254, -84.55553},
        {42.25059, -83.17854},
        {42.48587, -83.1052},
        {46.54354, -87.39542},
        {41.91643, -83.39771},
        {43.59781, -84.76751},
        {43.23418, -86.24839},
        {43.1689, -86.26395},
        {42.20115, -85.58},
        {42.22226, -83.3966},
        {42.67087, -83.03298},
        {42.21393, -83.19381},
        {42.58031, -83.0302},
        {42.24087, -83.26965},
        {42.13949, -83.17826},
        {43.00141, -85.76809},
        {42.47754, -83.0277},
        {42.7392, -84.62082},
        {42.28143, -83.38632},
        {42.3242, -83.40021},
        {42.21421, -83.14992},
        {42.91336, -85.70531},
        {42.24115, -83.61299},
        {46.91054, -98.70844},
        {40.92501, -98.34201},
        {40.58612, -98.38839},
        {40.69946, -99.08148},
        {41.37477, -83.65132},
        {41.28449, -84.35578},
        {41.35033, -83.12186},
        {40.74255, -84.10523},
        {41.64366, -83.48688},
        {41.557, -83.62716},
        {40.14477, -84.24244},
        {40.28422, -84.1555},
        {41.71894, -83.71299},
        {41.1145, -83.17797},
        {41.66394, -83.55521},
        {40.0395, -84.20328},
        {45.4647, -98.48648},
        {42.58474, -87.82119},
        {44.08861, -87.65758},
        {42.72613, -87.78285},
        {43.75083, -87.71453},
        {33.37032, -112.58378},
        {31.34455, -109.54534},
        {35.19807, -111.65127},
        {32.65783, -114.41189},
        {35.18944, -114.05301},
        {34.4839, -114.32245},
        {31.34038, -110.93425},
        {34.23087, -111.32514},
        {34.54002, -112.4685},
        {34.58941, -112.32525},
        {31.47148, -110.97648},
        {36.82523, -119.70292},
        {36.74773, -119.77237},
        {36.59634, -119.4504},
        {36.70801, -119.55597},
        {36.57078, -119.61208},
        {38.93324, -119.98435},
        {39.09193, -108.44898},
        {37.27528, -107.88007},
        {39.06387, -108.55065},
        {38.47832, -107.87617},
        {39.5186, -104.76136},
        {38.25445, -104.60914},
        {38.35, -104.72275},
        {37.7528, -100.01708},
        {37.97169, -100.87266},
        {37.04308, -100.921},
        {32.42067, -104.22884},
        {34.4048, -103.20523},
        {36.72806, -108.21869},
        {35.52808, -108.74258},
        {32.70261, -103.13604},
        {32.31232, -106.77834},
        {35.97859, -114.83249},
        {39.1638, -119.7674},
        {39.60797, -119.25183},
        {36.80553, -114.06719},
        {36.20829, -115.98391},
        {39.52963, -119.8138},
        {39.64908, -119.70741},
        {39.53491, -119.75269},
        {39.5963, -119.77602},
        {35.222, -101.8313},
        {32.2504, -101.47874},
        {31.75872, -106.48693},
        {34.81506, -102.3977},
        {31.69261, -106.20748},
        {33.57786, -101.85517},
        {31.99735, -102.07791},
        {31.84568, -102.36764},
        {35.53616, -100.95987},
        {34.18479, -101.70684},
        {31.65456, -106.30331},
        {31.63622, -106.29054},
        {31.84235, -102.49876},
        {37.67748, -113.06189},
        {37.10415, -113.58412},
        {37.13054, -113.50829},
        {58.30194, -134.41972},
        {40.41628, -120.65301},
        {43.6135, -116.20345},
        {43.66294, -116.68736},
        {47.67768, -116.78047},
        {43.69544, -116.35401},
        {43.46658, -112.03414},
        {43.49183, -116.42012},
        {46.41655, -117.01766},
        {46.38044, -116.97543},
        {43.61211, -116.39151},
        {46.73239, -117.00017},
        {43.54072, -116.56346},
        {42.8713, -112.44553},
        {47.71796, -116.95159},
        {43.82602, -111.78969},
        {42.56297, -114.46087},
        {45.67965, -111.03856},
        {46.00382, -112.53474},
        {47.50024, -111.30081},
        {46.59271, -112.03611},
        {48.19579, -114.31291},
        {46.87215, -113.994},
        {46.80833, -100.78374},
        {46.87918, -102.78962},
        {46.82666, -100.88958},
        {48.23251, -101.29627},
        {41.12389, -100.76542},
        {41.86663, -103.66717},
        {40.83242, -115.76312},
        {42.20681, -121.73722},
        {44.05817, -121.31531},
        {44.9279, -122.98371},
        {44.98595, -122.98287},
        {44.99012, -123.02621},
        {42.22487, -121.78167},
        {45.67208, -118.7886},
        {44.27262, -121.17392},
        {44.9429, -123.0351},
        {44.08054, -103.23101},
        {41.51021, -112.0155},
        {41.11078, -112.02605},
        {41.13967, -112.0505},
        {41.73549, -111.83439},
        {41.30716, -111.96022},
        {41.223, -111.97383},
        {40.0444, -111.73215},
        {41.16161, -112.02633},
        {41.19189, -111.97133},
        {40.11496, -111.65492},
        {40.16523, -111.61075},
        {41.08939, -112.06467},
        {40.53078, -112.29828},
        {47.64995, -117.23991},
        {46.73127, -117.17962},
        {47.65966, -117.42908},
        {47.67323, -117.23937},
        {46.06458, -118.34302},
        {41.58746, -109.2029},
        {20.89472, -156.47},
        {21.40222, -157.73944},
        {21.40929, -157.80092},
        {20.75548, -156.45446},
        {21.34694, -158.08583},
        {21.45, -158.00111},
        {21.39722, -157.97333},
        {21.50211, -158.02104},
        {20.89111, -156.50472},
        {21.38667, -158.00917},
        {21.34417, -158.03083},
        {19.72972, -155.09},
        {21.30694, -157.85833},
        {61.32139, -149.56778},
        {64.83778, -147.71639},
        {61.21806, -149.90028},
        {64.8, -147.53333},
        {45.90194, -112.65708},
        {21.35237, -158.08655},
        {21.4936, -158.06151},
        {39.82861, -86.38224}
    };
    const int GMToffsets[313] = {
        -6,
        -6,
        -6,
        -6,
        -6,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -6,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -6,
        -6,
        -6,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -6,
        -5,
        -6,
        -5,
        -5,
        -6,
        -5,
        -5,
        -6,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -6,
        -5,
        -5,
        -5,
        -6,
        -6,
        -6,
        -6,
        -5,
        -6,
        -6,
        -6,
        -5,
        -6,
        -5,
        -5,
        -6,
        -5,
        -5,
        -5,
        -6,
        -5,
        -5,
        -6,
        -6,
        -5,
        -5,
        -6,
        -5,
        -5,
        -5,
        -5,
        -6,
        -6,
        -5,
        -5,
        -5,
        -6,
        -6,
        -5,
        -6,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -6,
        -6,
        -6,
        -6,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -5,
        -6,
        -6,
        -6,
        -6,
        -6,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -8,
        -8,
        -8,
        -8,
        -8,
        -8,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -6,
        -6,
        -6,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -8,
        -8,
        -8,
        -8,
        -8,
        -8,
        -8,
        -8,
        -8,
        -6,
        -6,
        -7,
        -6,
        -7,
        -6,
        -6,
        -6,
        -6,
        -6,
        -7,
        -7,
        -6,
        -7,
        -7,
        -7,
        -9,
        -8,
        -7,
        -7,
        -8,
        -7,
        -7,
        -7,
        -8,
        -8,
        -7,
        -8,
        -7,
        -7,
        -8,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -6,
        -7,
        -6,
        -6,
        -6,
        -7,
        -8,
        -8,
        -8,
        -8,
        -8,
        -8,
        -8,
        -8,
        -8,
        -8,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -7,
        -8,
        -8,
        -8,
        -8,
        -8,
        -7,
        -10,
        -10,
        -10,
        -10,
        -10,
        -10,
        -10,
        -10,
        -10,
        -10,
        -10,
        -10,
        -10,
        -9,
        -9,
        -9,
        -9,
        -7,
        -10,
        -10,
        -5
    };
    const bool usesDST[313] = {
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
        true
    };

    Serial.print("setTimeZoneAndDST: coordinates: ");
    Serial.print(myLat);
    Serial.print(", ");
    Serial.println(myLong);

    for (int ii = 0; ii < _numCities; ii++) {
        //find the distance between my coordinates and the next city on the list
        thisDistance = TinyGPSPlus::distanceBetween(myLat,myLong,_cityCoords[ii][0],_cityCoords[ii][1]);
        if (thisDistance < _minDistance) {
            _minDistance = thisDistance;
            _minIndex = ii;
        }
    }
    Serial.print("    coordinates of closest city: ");
    Serial.print(_cityCoords[_minIndex][0]);
    Serial.print(", ");
    Serial.println(_cityCoords[_minIndex][1]);

    int myGMTOffset = GMToffsets[_minIndex];
    boolean myLocationUsesDST = usesDST[_minIndex];

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
    Serial.print(" alarmAMPM: ");
    Serial.print(alarmAMPM);
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
    boolean alarmAMPM;
    static boolean prevAlarmAMPM = false;
    boolean alarmOnSwitchVal;
    
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