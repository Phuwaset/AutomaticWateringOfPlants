
#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <TimeLib.h>
#include <WidgetRTC.h>

DHT dht;
BlynkTimer    timer;
WidgetRTC     server_rtc;

// ################# What you need to modify #########################
// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "Put_Your_Blynk_Token_Here";

// Your WiFi credentials.
char ssid[] = "Put_Your_WiFi_Name_Here";
char pass[] = "Put_Your_WiFi_Password_Here";

// Your Time Zone
const char TimeZone[] = "Put_Your_Time_Zone_Here";   

// ###################################################################

long rtc_sec_server;
unsigned char weekday_server;

#define VALVE_1_OUT             D1
#define VALVE_2_OUT             D2
#define MOISURE_READ            A0
#define DHT_READ_PIN            D3

#define VALVE_ON                0
#define VALVE_OFF               1

#define BLYNK_TEMP              V0
#define BLYNK_RH                V1
#define BLYNK_SOIL_MOISTURE     V2
#define BLYNK_TIMER_1           V3
#define BLYNK_TIMER_2           V4
#define BLYNK_VALVE_1           V5
#define BLYNK_VALVE_2           V6

int soil_moisture;

unsigned char start_time_hour_1;
unsigned char start_time_min_1;
unsigned char stop_time_hour_1;
unsigned char stop_time_min_1;
unsigned char day_timer_1;
bool flag_timer1_en;
bool flag_timer_on_valve_1;

unsigned char start_time_hour_2;
unsigned char start_time_min_2;
unsigned char stop_time_hour_2;
unsigned char stop_time_min_2;
unsigned char day_timer_2;
bool flag_timer2_en;
bool flag_timer_on_valve_2;

bool flag_valve_1_set;
bool flag_valve_2_set;

bool flag_valve_1_status;
bool flag_valve_2_status;

bool flag_blynk_valve_1_update;
bool flag_blynk_valve_2_update;

// ######################################################################
BLYNK_CONNECTED()
{
  Blynk.syncAll();
  server_rtc.begin();
}

// ######################################################################
BLYNK_WRITE (BLYNK_VALVE_1)
{
  int val = param.asInt();  // assigning incomming value from pin to a var

  if ( flag_timer_on_valve_1 == 0 )
    flag_valve_1_set = val;
  else
    flag_blynk_valve_1_update = 1;
  
  Serial.print("Valve 1 Set: ");
  Serial.println(val);
}

// ######################################################################
BLYNK_WRITE (BLYNK_VALVE_2)
{
  int val = param.asInt();  // assigning incomming value from pin to a var

  if ( flag_timer_on_valve_2 == 0)
    flag_valve_2_set = val;
  else
    flag_blynk_valve_2_update = 1;
  
  Serial.print("Valve 2 Set: ");
  Serial.println(val);
}

// ######################################################################
BLYNK_WRITE(BLYNK_TIMER_1)
{
  unsigned char week_day;
  
  TimeInputParam  t(param);
  
  if (t.hasStartTime() && t.hasStopTime() && t.getStartSecond()==0 && t.getStopSecond()==0 )
  {
     start_time_hour_1 = t.getStartHour();
     start_time_min_1 = t.getStartMinute();
     Serial.println(String("Time1 Start: ") +
                     start_time_hour_1 + ":" +
                     start_time_min_1);
    
     stop_time_hour_1 = t.getStopHour();
     stop_time_min_1 = t.getStopMinute();
     Serial.println(String("Time1 Stop: ") +
                     stop_time_hour_1 + ":" +
                     stop_time_min_1);
    
     for (int i = 1; i <= 7; i++)
     {
       if (t.isWeekdaySelected(i))  // will be "TRUE" if nothing selected as well
       {
         day_timer_1 |= (0x01 << (i-1));
       }
       else
         day_timer_1 &= (~(0x01 << (i-1)));
     }
    
     Serial.print("Time1 Selected Days: ");
     Serial.println(day_timer_1, HEX);
     flag_timer1_en = 1;
  }
  else
  {
    flag_timer1_en = 0;
    Serial.println("Disabled Timer 1");
  }
}

// ######################################################################
BLYNK_WRITE(BLYNK_TIMER_2)
{
  unsigned char week_day;
  
  TimeInputParam  t(param);
  
  if (t.hasStartTime() && t.hasStopTime() && t.getStartSecond()==0 && t.getStopSecond()==0 )
  {
     start_time_hour_2 = t.getStartHour();
     start_time_min_2 = t.getStartMinute();
     Serial.println(String("Time2 Start: ") +
                     start_time_hour_2 + ":" +
                     start_time_min_2);
    
     stop_time_hour_2 = t.getStopHour();
     stop_time_min_2 = t.getStopMinute();
     Serial.println(String("Time2 Stop: ") +
                     stop_time_hour_2 + ":" +
                     stop_time_min_2);
    
     for (int i = 1; i <= 7; i++)
     {
       if (t.isWeekdaySelected(i))  // will be "TRUE" if nothing selected as well
       {
         day_timer_2 |= (0x01 << (i-1));
       }
       else
         day_timer_2 &= (~(0x01 << (i-1)));
     }
    
     Serial.print("Time1 Selected Days: ");
     Serial.println(day_timer_2, HEX);
     flag_timer2_en = 1;
  }
  else
  {
    flag_timer2_en = 0;
    Serial.println("Disabled Timer 1");
  }
}

// ######################################################################
void fn_valve_mng (void)
{
  bool time_set_overflow;
  long start_timer_sec;
  long stop_timer_sec;
  bool flag_timer_on_1_buf = flag_timer_on_valve_1;
  bool flag_timer_on_2_buf = flag_timer_on_valve_2;

  // VALVE 1
  time_set_overflow = 0;
  start_timer_sec = start_time_hour_1*3600 + start_time_min_1*60;
  stop_timer_sec = stop_time_hour_1*3600 + stop_time_min_1*60;

  if ( stop_timer_sec < start_timer_sec ) time_set_overflow = 1;
  
  if ( flag_timer1_en && (((time_set_overflow == 0 && (rtc_sec_server >= start_timer_sec) && (rtc_sec_server < stop_timer_sec)) ||
        (time_set_overflow  && ((rtc_sec_server >= start_timer_sec) || (rtc_sec_server < stop_timer_sec)))) && 
        (day_timer_1 == 0x00 || (day_timer_1 & (0x01 << (weekday_server - 1) )))) )
  {
    flag_timer_on_valve_1 = 1;
  }
  else
    flag_timer_on_valve_1 = 0;

  // VALVE 2
  time_set_overflow = 0;
  start_timer_sec = start_time_hour_2*3600 + start_time_min_2*60;
  stop_timer_sec = stop_time_hour_2*3600 + stop_time_min_2*60;

  if ( stop_timer_sec < start_timer_sec ) time_set_overflow = 1;
  
  if ( flag_timer2_en && (((time_set_overflow == 0 && (rtc_sec_server >= start_timer_sec) && (rtc_sec_server < stop_timer_sec)) ||
        (time_set_overflow  && ((rtc_sec_server >= start_timer_sec) || (rtc_sec_server < stop_timer_sec)))) && 
        (day_timer_2 == 0x00 || (day_timer_2 & (0x01 << (weekday_server - 1) )))) )
  {
    flag_timer_on_valve_2 = 1;
  }
  else
    flag_timer_on_valve_2 = 0;


  // VALVE 1
  if ( flag_timer_on_valve_1 )
  {
    flag_valve_1_status = 1;
    flag_valve_1_set = 0;
  }
  else
  {
    flag_valve_1_status = flag_valve_1_set;
  }

  // VALVE 2
  if ( flag_timer_on_valve_2 )
  {
    flag_valve_2_status = 1;
    flag_valve_2_set = 0;
  }
  else
  {
    flag_valve_2_status = flag_valve_2_set;
  }

  if ( flag_timer_on_1_buf != flag_timer_on_valve_1 )
    flag_blynk_valve_1_update = 1;

  if ( flag_timer_on_2_buf != flag_timer_on_valve_2 )
    flag_blynk_valve_2_update = 1;

  // HARDWARE CONTROL
  digitalWrite(VALVE_1_OUT, !flag_valve_1_status);  // Relay active LOW
  digitalWrite(VALVE_2_OUT, !flag_valve_2_status);  // Relay active LOW
}

// ######################################################################
void RTC_mng(void)
{
  int year_server;
  String currentTime = String(hour()) + ":" + minute() + ":" + second();
  String currentDate = String(day()) + " " + month() + " " + year();
  
  weekday_server = weekday();
  
  if ( weekday_server == 1 )
    weekday_server = 7;
  else
    weekday_server -= 1;

  year_server = year();
  
  rtc_sec_server = (hour()*60*60) + (minute()*60) + second();

#if 0
  Serial.print("Current server time: ");
  Serial.print(currentTime);
  Serial.print(" ");
  Serial.print(currentDate);
  Serial.println();
  Serial.print("RTC Server in Seconds : ");
  Serial.println(rtc_sec_server);
  Serial.print("Weekday: ");
  Serial.println(weekday_server);

  Serial.print("Year Server: ");
  Serial.println(year_server);
#endif
}

// ######################################################################
void update_blynk_data(void)
{
  delay(dht.getMinimumSamplingPeriod());
  float humidity = dht.getHumidity(); 
  float temperature = dht.getTemperature(); 

  soil_moisture = analogRead(MOISURE_READ);
  soil_moisture = map(soil_moisture, 1023, 0, 0, 1023);
  
  Blynk.virtualWrite(BLYNK_TEMP, temperature);
  Blynk.virtualWrite(BLYNK_RH, humidity);
  Blynk.virtualWrite(BLYNK_SOIL_MOISTURE, soil_moisture);

  if ( flag_blynk_valve_1_update )
  {
    flag_blynk_valve_1_update = 0;
    Blynk.virtualWrite(BLYNK_VALVE_1, flag_valve_1_status);
  }

  if ( flag_blynk_valve_2_update )
  {
    flag_blynk_valve_2_update = 0;
    Blynk.virtualWrite(BLYNK_VALVE_2, flag_valve_2_status);
  }
}

// ######################################################################
void setup()
{
  // Debug console
  Serial.begin(9600);

  Blynk.begin(auth, ssid, pass);
  // You can also specify server:
  //Blynk.begin(auth, ssid, pass, "blynk-cloud.com", 80);
  //Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,100), 8080);
  
   // DHT initialization
  dht.setup(DHT_READ_PIN); 
    
  pinMode(VALVE_1_OUT, OUTPUT);
  pinMode(VALVE_2_OUT, OUTPUT);
  pinMode(MOISURE_READ, INPUT);

  digitalWrite(VALVE_1_OUT, VALVE_OFF);  
  digitalWrite(VALVE_2_OUT, VALVE_OFF);  
}

// ######################################################################
void loop()
{
  Blynk.run();
  delay(100);
  RTC_mng();
  fn_valve_mng();
  update_blynk_data();
}
