#include <Wire.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>
#include <Streaming.h>
#define USE_DBG_SERIAL
#define HOUR_START_SEQUENCE  14

#ifdef USE_DBG_SERIAL
#include <SoftwareSerial.h>
#define DBG DBGSerial.write
#define DBG2 DBGSerial.write
#define DBG3 DBGSerial.write
#define INIT DBGSerial.write
SoftwareSerial DBGSerial(2, 3); // RX, TX
#else
#define DBG //
#define DBG2 DBG
#define DBG3 DBG
#define INIT //
#endif

#define SEQUENCE_STEP_RATE_1 1
#define SEQUENCE_STEP_RATE_4 2
#define SEQUENCE_STEP_RATE_5 3
#define SEQUENCE_STEP_RANGE  4
#define SEQUENCE_FIRST_STEP SEQUENCE_STEP_RATE_1
#define SEQUENCE_LAST_STEP SEQUENCE_STEP_RANGE

#define SEQUENCE_OFFSET_MS_DEFAULT 5000

#define BUFFER_SIZE 256
#define DBG //
#define DBG2 DBG
#define DBG3 DBG
#define INIT //

RTC_PCF8523 rtc;

char filename []={"03111508.csv"};


const int chipSelect = 10;
char serialbuffer[BUFFER_SIZE];
unsigned int bufferposition,oldbufferpos;
unsigned long timesent, millis_last_check;
bool  startfound , crfound, do_send_sequence;  
unsigned short sequence_step, sequence_offset_ms, sequence_done_step;
DateTime logtimestamp,timenow,timebefore; 
File dataFile;


void setup() 
{
  bufferposition = 0;
 
  // put your setup code here, to run once:
  pinMode (5, OUTPUT);
#ifdef USE_DBG_SERIAL
  DBGSerial.begin(4800);
#endif
  Serial.begin(9600);

   if (! rtc.begin()) {
    INIT (F("Couldn't find RTC\n"));
    while (1);
  }

  // intialize at compiler time
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    if (! rtc.initialized()) {
    INIT (F("RTC is NOT running!"));
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  CreateFilename();
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
  
}

void loop() 
{
   
  CheckSerial();
 
  if (do_send_sequence)
    DoUplinkSequence();

  CheckHour();
  
  return;
           
   // RTCTestAndLog();
   // delay(1000);

}

void CheckHour()
{
 if ((millis() - millis_last_check) <= 5000)
 return;
  
  timenow = rtc.now();
  if (timenow.hour() != timebefore.hour())
    if (timenow.hour() == HOUR_START_SEQUENCE)
      do_send_sequence = true;

  timebefore = timenow;
  millis_last_check = millis();
  
}

void DoUplinkSequence()
{       
   if ((millis() - timesent >= sequence_offset_ms)&&(sequence_step != sequence_done_step))
   {
    switch (sequence_step)
    {
      case SEQUENCE_STEP_RATE_1 : Serial.println(F("$CCCYC,0,1,0,1,0,1")); timesent = millis();  break;
      case SEQUENCE_STEP_RATE_4 : Serial.println(F("$CCCYC,0,1,0,4,0,1")); timesent = millis(); sequence_offset_ms = 10000;  break;
      case SEQUENCE_STEP_RATE_5 : Serial.println(F("$CCCYC,0,1,0,5,0,1")); timesent = millis(); break;
      case SEQUENCE_STEP_RANGE : Serial.println(F("$CCCYC,0,1,0,4,0,1")); timesent = millis(); sequence_offset_ms = SEQUENCE_OFFSET_MS_DEFAULT; break;
      
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
                      intobuffer(F("\n"));
                      LogBuffer(logtimestamp, true);
                      ResetBuffer();
                      startfound = false;
                     }
                      
                     startfound = true;
                     logtimestamp = rtc.now();
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
  const static char s1[] PROGMEM = "CCMUC";
  const static char c1[] PROGMEM = "$CCMUC,0,1,1111";
  const static char c2[] PROGMEM = "$CCMUC,0,1,1010";
  const static char c3[] PROGMEM = "$CCMUC,0,1,2222";
  const static char c4[] PROGMEM = "$CCMUC,0,1,3333";
  const static char c5[] PROGMEM = "$CCMUC,0,1,4444";
 if (strstr(serialbuffer, s1) && (bufferposition < 25))
    {
      if (strstr(serialbuffer,c1))
      {
        do_send_sequence = true; 
        sequence_offset_ms = SEQUENCE_OFFSET_MS_DEFAULT; 
        sequence_step = SEQUENCE_FIRST_STEP; 
      }
      else if (strstr(serialbuffer,c2))
        do_send_sequence = false;
      else if (strstr(serialbuffer,c3))
        SetBandwidth(1);
      else if (strstr(serialbuffer,c4))
        SetBandwidth(2);
      else if (strstr(serialbuffer,c5))
        SetBandwidth(3);
            
      /*
      sscanf (serialbuffer, "$CCMUC,0,1,%d*%d\n", &command,&chks);
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

void SetBandwidth(unsigned short _bw)
{
  switch (_bw)
  {
    case 1 :    Serial.println(F("$CCCFG,XXXX")); break;
    case 2 :    Serial.println(F("$CCCFG,YYYY")); break;
    case 3 :    Serial.println(F("$CCCFG,ZZZZ")); break;
  }
}

void CheckSerialIf()
{

  char inchar;
  inchar = Serial.read();
  if (((inchar >= 32) && (inchar <= 126)) || (inchar == 10) || (inchar == 13)) 
      
  {
    if (inchar == '$')

      {  
          /*           if (startfound)
                     {
                      //incomplete previous message already in buffer
                      intobuffer("\n");
                      LogBuffer(logtimestamp, true);
                      ResetBuffer();
                      startfound = false;
                     }
             */         
                     startfound = true;
                     logtimestamp = rtc.now();
                //     intobuffer(inchar);
      }
  /*    else if (inchar == '\r')
      {
                  if (startfound)
                    crfound = true;
        //            intobuffer(inchar);
      }*/
      else if (inchar =='\n')
      {
                    if (startfound && crfound)
                    //{
          //           intobuffer(inchar);
           //           LogBuffer(logtimestamp,true);
          //            ResetBuffer();
                    //}
                      startfound = false;
                      crfound = false;
                      
       }
     
  }
}


void intobuffer( char _inchar)
{
        if (bufferposition >= BUFFER_SIZE)
        {
         if (serialbuffer[0] == '$')
            LogBuffer(logtimestamp, true);
         LogBuffer(logtimestamp, false);
         
         
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
  
  dataFile = SD.open(filename, FILE_WRITE);
  if (!dataFile)
  {
    DBG (F("error opening file for writing"));
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
  dataFile.close();

  serialbuffer[i] = 0x00;

  DBG3 (F("Logged one entry\n"));
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
}



/*
void RTCTestAndLog()
{
  DateTime now = rtc.now();
    
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();


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
}
*/

