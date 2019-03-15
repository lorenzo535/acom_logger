#include <Wire.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>
#include <Streaming.h>
#define USE_DBG_SERIAL

//========================================
#define HOUR_START_SEQUENCE  15
//========================================

#define BATTERY_SCALE 0.025715
#define BATTERY_OFFSET -0.466921

#ifdef USE_DBG_SERIAL
#include <SoftwareSerial.h>
#define DBG DBGSerial.println
#define DISPLAY_INFO DBGSerial.print
#define DBG2 DBGSerial.println
#define DBG3 DBGSerial.println
#define INIT DBGSerial.println
SoftwareSerial DBGSerial(2, 3); // RX, TX
#else
#define DBG //
#define DBG2 DBG
#define DBG3 DBG
#define INIT //
#define DISPLAY_INFO //
#endif

#define SEQUENCE_STEP_RATE_1 1
#define SEQUENCE_STEP_RATE_4 2
#define SEQUENCE_STEP_RATE_5 3
#define SEQUENCE_STEP_PING  4
#define SEQUENCE_STEP_PAUSE  5
#define SEQUENCE_FIRST_STEP SEQUENCE_STEP_RATE_1
#define SEQUENCE_LAST_STEP SEQUENCE_STEP_PAUSE

#define SEQUENCE_OFFSET_MS_DEFAULT 8000
#define SEQUENCE_OFFSET_MS_PAUSE 40000

#define BUFFER_SIZE 256

#define DBG //
#define DBG2 //
#define DBG3 //
//#define INIT //


RTC_PCF8523 rtc;

char filename []={"03111508.csv"};
bool file_is_open;

const int chipSelect = 10;
char serialbuffer[BUFFER_SIZE];
unsigned int bufferposition,oldbufferpos;
unsigned long timesent, millis_last_check;
bool  startfound , crfound, do_send_sequence;  
unsigned short sequence_step, sequence_offset_ms, sequence_done_step;
DateTime logtimestamp,timenow,timebefore; 
File dataFile;
bool display_info;

void setup() 
{
  bufferposition = 0;
 
  // put your setup code here, to run once:
  pinMode (5, OUTPUT);
  pinMode (A6, INPUT);
#ifdef USE_DBG_SERIAL
  DBGSerial.begin(4800);
#endif
  Serial.begin(9600);

   if (! rtc.begin()) {
    INIT ( F("Couldn't find RTC\n") );
    while (1);
  }

  
 // This line sets the RTC with an explicit date & time
   // rtc.adjust(DateTime(2019, 3, 13, 14, 59, 35));

    if (! rtc.initialized()) {
    INIT (F("RTC is NOT running!"));
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  else
  {
    INIT (F("RTC fine and dandy ...."));
    timenow = rtc.now();
    DisplayInfo();
  }

 
 
  
  INIT (F("Initializing SD card..."));

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    INIT (F("Card failed, or not present"));
    // don't do anything more:
    return;
  }
  INIT (F("card initialized."));

  timesent = millis();
  millis_last_check = timesent;
  do_send_sequence = false;
  sequence_step = SEQUENCE_FIRST_STEP;
  sequence_done_step = SEQUENCE_LAST_STEP;
  sequence_offset_ms = SEQUENCE_OFFSET_MS_DEFAULT;

  Serial.println(F("$CCCFQ,ALL"));
  timebefore = rtc.now();
  display_info = false;
  file_is_open = false;

   CreateFilename();
   
}

void loop() 
{
   //RTCTestAndLog();
   //delay(1000);
   //return;
   
  CheckSerial();
   
  if (do_send_sequence)
    DoUplinkSequence();

  CheckHourAndDisplay();


  ReadKeyboardCmds();
  return;
           
   

}

void CheckHourAndDisplay()
{

 
 if ((millis() - millis_last_check) <= 5000)
 return;

// battery = 12.12;//analogRead(A6) * BATTERY_SCALE + BATTERY_OFFSET;
/*
if (dataFile)
{
  dataFile.close();
  file_is_open = false;
}
*/

  timenow = rtc.now();
  if (display_info)
  {
     DisplayInfo();
  }
   
  if (timenow.hour() > timebefore.hour())
    if (timenow.hour() == HOUR_START_SEQUENCE)
    {
      do_send_sequence = true;
      DBG3 (F("hour check has started sequence"));
    }

  timebefore = timenow;
  millis_last_check = millis();
  
}

void DoUplinkSequence()
{       
   
   if ((millis() - timesent >= sequence_offset_ms)&&(sequence_step != sequence_done_step))
   {
    switch (sequence_step)
    {
      case SEQUENCE_STEP_RATE_1 : Serial.println(F("$CCCYC,0,0,1,1,0,1")); timesent = millis(); DBG (F(">>>>>> Sequence rate 1"));  break;
      case SEQUENCE_STEP_RATE_4 : Serial.println(F("$CCCYC,0,0,1,4,0,1")); timesent = millis(); DBG (F(">>>>>> Sequence rate 4"));sequence_offset_ms = 10000;  break;
      case SEQUENCE_STEP_RATE_5 : Serial.println(F("$CCCYC,0,0,1,5,0,1")); timesent = millis(); DBG (F(">>>>>> Sequence rate 5"));sequence_offset_ms = SEQUENCE_OFFSET_MS_DEFAULT; break;
      case SEQUENCE_STEP_PING : Serial.println(F("$CCMPC,0,1")); timesent = millis();  DBG (F(">>>>> Sequence PING"));sequence_offset_ms = SEQUENCE_OFFSET_MS_PAUSE; break;
      case SEQUENCE_STEP_PAUSE :  timesent = millis();  DBG (F(">>>>>> Sequence PAUSE")); sequence_offset_ms = SEQUENCE_OFFSET_MS_DEFAULT; break;
      
    }
    sequence_done_step = sequence_step;
    sequence_step++;
    if (sequence_step > SEQUENCE_LAST_STEP)
      sequence_step = SEQUENCE_FIRST_STEP;
   }
  
}



void PrintBuffer()
{
   DBG2 (serialbuffer);
   ResetBuffer();
   
}


String zeropad(int input)
{
  String output;
  if (input < 10)
    output = "0" + String(input);
    else
    output = input;
    
 return output;
}


void CheckSerial()
{

  char inchar;
  inchar = Serial.read();
  
  if (((inchar >= 32) && (inchar <= 126)) || (inchar == 10) || (inchar == 13)) 
         
  {
      
      switch(inchar)
         {
         case '$' :  
                     if (startfound)
                     {
                      //incomplete previous message already in buffer
                      intobuffer(('\n'));
                      LogBuffer(logtimestamp, true);
                      ResetBuffer();
                      startfound = false;
                     }
                      
                     startfound = true;
                     logtimestamp = timenow; //rtc.now();
                     intobuffer(inchar);
                     break;
         
         
         case '\r':
                  if (startfound)
                    crfound = true;
                    intobuffer(inchar);
                    break;
           
        case '\n':
                    if (startfound && crfound)
                    {
                      intobuffer(inchar);
                      CheckCommand();
                      LogBuffer(logtimestamp, true);
                      ResetBuffer();
                    }
                      startfound = false;
                      crfound = false;
                      
                      break; 
         default:
                    intobuffer(inchar);
                  
         }
     
  }
}




void CheckCommand()
{
  unsigned short command,chks;
 
  const static char s1[] PROGMEM = "CAMUA";
  const static char start_seq[] PROGMEM = "$CAMUA,1,0,1fff";
  const static char stop_seq[]  PROGMEM = "$CAMUA,1,0,0000";
  const static char bw5000[]    PROGMEM = "$CAMUA,1,0,0500";
  const static char bw2500[]    PROGMEM = "$CAMUA,1,0,0250";
  const static char bw1250[]    PROGMEM = "$CAMUA,1,0,0125";
  const static char bw2000[]    PROGMEM = "$CAMUA,1,0,0200";
  const static char freq10000[] PROGMEM = "$CAMUA,1,0,1100";
  const static char freq9000[]   PROGMEM = "$CAMUA,1,0,1900";
  const static char battreq[]   PROGMEM = "$CAMUA,1,0,0bbb";

  
 if (strstr_P(serialbuffer, s1) && (bufferposition < 25))
    {
      DBG3 (F("Received a MUC command"));
      
      if (strstr_P(serialbuffer,start_seq))
      {
        
        do_send_sequence = true; 
        sequence_offset_ms = SEQUENCE_OFFSET_MS_DEFAULT; 
        sequence_step = SEQUENCE_FIRST_STEP;
        sequence_done_step = SEQUENCE_LAST_STEP;
        timesent = millis();
        DBG3 (F("command start sequence")); 
      }
      else if (strstr_P(serialbuffer,stop_seq))
      {
        do_send_sequence = false;
           DBG3 (F("command stop sequence")); 
      }
      else if (strstr_P(serialbuffer,bw5000))
        SetBandwidthOrFrequency(5000);
      else if (strstr_P(serialbuffer,bw2500))
        SetBandwidthOrFrequency(2500);
      else if (strstr_P(serialbuffer,bw1250))
        SetBandwidthOrFrequency(1250);
      else if (strstr_P(serialbuffer,bw2000))
        SetBandwidthOrFrequency(2000);
      else if (strstr_P(serialbuffer,freq10000))
        SetBandwidthOrFrequency(10000);
       else if (strstr_P(serialbuffer,freq9000))
        SetBandwidthOrFrequency(9000);
       else if (strstr_P(serialbuffer,battreq))
        SendBatteryValue();

                   
      /*
      sscanf (serialbuffer, "$CAMUA,0,1,%d*%d\n", &command,&chks);
      switch (command)
      {
        case 1111: do_send_sequence = true; sequence_offset_ms = SEQUENCE_OFFSET_MS_DEFAULT; sequence_step = SEQUENCE_FIRST_STEP; break;
        case 0: do_send_sequence = false; break;
        case 2222: SetBandwidth(1); break;
        case 3333: SetBandwidth(2); break;
        case 4444: SetBandwidth(3); break;
        
      }*/
      }
    
}

void SendBatteryValue()
{
  unsigned short battADC = analogRead(A6);
  if (battADC < 1000)
    Serial.print (F("$CCMUC,0,1,0"));
    else
  Serial.print (F("$CCMUC,0,1,"));  
  Serial.println(battADC);
}

void SetBandwidthOrFrequency(unsigned short _bw)
{
     DBG3 (F("====")); 
     DBG3 (F("Setting bandwith to parameter"));
     DBG3 (_bw);
     DBG3 (F("====")); 
  
  switch (_bw)
  {
    case 5000 :    Serial.println(F("$CCCFG,BW0,5000")); break;
    case 2500 :    Serial.println(F("$CCCFG,BW0,2500")); break;
    case 1250 :    Serial.println(F("$CCCFG,BW0,1250")); break;
    case 2000 :    Serial.println(F("$CCCFG,BW0,2000")); break;
    case 10000 :   Serial.println(F("$CCCFG,FC0,10000")); break;
    case 9000 :    Serial.println(F("$CCCFG,FC0,9000")); break;
  }
}



void intobuffer( char _inchar)
{
        if (bufferposition >= BUFFER_SIZE)
        {
         if (serialbuffer[0] == '$')
            LogBuffer(logtimestamp, true);
         LogBuffer(logtimestamp, false);
         //serialbuffer[bufferposition] = 0x00;
         
        //overflow
         bufferposition = 0;
          }
        
        serialbuffer[bufferposition] =  _inchar;
        bufferposition++;

      
        
}

void Resetbuffer()
{
  bufferposition = 0;
  startfound = false;
  crfound = false;  
}

void LogBuffer(DateTime _timestamp, bool withstamp)
{
  
  int i;

  if (!dataFile)
  {
    dataFile = SD.open(filename, FILE_WRITE);
    DBG (F(" #################   error opening file for writing"));    
    return;
  }

  if (withstamp)
  {
    String timestamp = "";
  
    timestamp += _timestamp.year(); 
    timestamp += zeropad(_timestamp.month()) ;
    timestamp += zeropad(_timestamp.day());
    timestamp += "-";
    timestamp += zeropad(_timestamp.hour());
    timestamp += ":";
    timestamp += zeropad(_timestamp.minute());
    timestamp += ":";
    timestamp += zeropad(_timestamp.second());
    timestamp += ",";
    timestamp += _timestamp.unixtime();
    timestamp += ",";
    dataFile.print(timestamp);
  }  

 

  for (i = 0; i < bufferposition; i++)
  {
    dataFile.print(serialbuffer[i]);
   
  }
  

  serialbuffer[i] = 0x00;

  DBG (F("Logged one entry\n"));
  DBG (F("Serial buffer:  "));
  DBG2 (serialbuffer);

}


void ResetBuffer()
{

  serialbuffer[0] = 0x00;
  bufferposition = 0;

}


void CreateFilename()
{
  logtimestamp= rtc.now();

  String timestamp = "";  
  timestamp += zeropad(logtimestamp.month()) ;
  timestamp += zeropad(logtimestamp.day());
  
  timestamp += zeropad(logtimestamp.hour());
  
  timestamp += zeropad(logtimestamp.minute());

  timestamp += ".csv";
  sprintf (filename,"%s", timestamp.c_str());  

  INIT  (F("new filename :"));
  INIT  (filename);
  INIT  (F("\n"));


  //Open file by default:
   dataFile = SD.open(filename, FILE_WRITE);
}




void RTCTestAndLog()
{
  DateTime now = rtc.now();
    
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
   // Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
/*

  String dataString = "";

  dataString += now.year(); 
  dataString += zeropad(now.month()) ;
  dataString += zeropad(now.day());
  dataString += "--";
  dataString += zeropad(now.hour());
  dataString += ":";
  dataString += zeropad(now.minute());
  dataString += ":";
  dataString += zeropad(now.second());
  dataString += ",";
  dataString += now.unixtime();
  dataString += ",";


  // read three sensors and append to the string:
  for (int analogPin = 0; analogPin < 3; analogPin++) {
    int sensor = analogRead(analogPin);
    dataString += String(sensor);
    if (analogPin < 2) {
      dataString += ",";
    }
  }

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  dataFile = SD.open("data.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) 
  {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    DBG (dataString.c_str());
    DBG ("\n");
  }
  // if the file isn't open, pop up an error:
  else {
    DBG ("error opening datalog.txt");
  }
  */
}



void ReadKeyboardCmds()
{
  
    //Check for manual commands
   if (DBGSerial.available() > 0) 
   {  
    DBG (F("keyboard in:"));
      
      char rx_byte = DBGSerial.read();       // get the character
          
      
          
      switch (rx_byte) 
      {
       case 'I': display_info = !display_info;  break;
       
       case 'X': INIT (F("Received file close command")); dataFile.close();  break;
       case 'O':  INIT (F("Received file open command"));  dataFile = SD.open(filename, FILE_WRITE);  break;
       case 'Q': break;
            
      }
        
     }
}

void DisplayInfo()
{
   DISPLAY_INFO (F(" >>>> BATT: "));
     DISPLAY_INFO ( analogRead(A6));
     DISPLAY_INFO (F("\n"));
     DISPLAY_INFO (F("==== TIME: "));
     DISPLAY_INFO ( timenow.hour());
     DISPLAY_INFO (F(":"));
     DISPLAY_INFO ( timenow.minute());
     DISPLAY_INFO (F("-"));
     DISPLAY_INFO ( timenow.second());
     DISPLAY_INFO (F("\n"));

}

