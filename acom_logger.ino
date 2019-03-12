#include <Wire.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>
//#include <Streaming.h>
#include <SoftwareSerial.h>


#define BUFFER_SIZE 256
#define DBG DBGSerial.write
#define DBG2 DBGSerial.write
#define DBG3 DBGSerial.write
#define INIT DBGSerial.write
#define DBG //
#define DBG2 DBG
#define INIT //

RTC_PCF8523 rtc;

char filename []={"03111508.csv"};
SoftwareSerial DBGSerial(2, 3); // RX, TX

const int chipSelect = 10;
char serialbuffer[BUFFER_SIZE];
unsigned int bufferposition,oldbufferpos;
bool  startfound , crfound;  
DateTime logtimestamp; 
File dataFile;


void setup() {
  bufferposition = 0;
 
  // put your setup code here, to run once:
  pinMode (5, OUTPUT);

  DBGSerial.begin(4800);

  Serial.begin(9600);

   if (! rtc.begin()) {
    DBG ("Couldn't find RTC\n");
    while (1);
  }

  // intialize at compiler time
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    if (! rtc.initialized()) {
    DBG ("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  CreateFilename();
  INIT ("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    DBG ("Card failed, or not present");
    // don't do anything more:
    return;
  }
  INIT ("card initialized.");
 
  
}

void loop() {
 
  
  CheckSerial();
  return;

  
  //Test();
    
  if (bufferposition > BUFFER_SIZE -1)
          PrintBuffer();
          
   // RTCTestAndLog();
   // delay(1000);

}

/*
void Test()
{
 char  inchar = Serial.read();
  if (((inchar >= 32) && (inchar <= 126)) || (inchar == 10) || (inchar == 13)) 
    {
    intobuffer(inchar);
   
    }
  
}
*/
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
     
    //keep reading and printing from serial untill there are bytes in the serial buffer
 //    while (Serial.available()>0)
    {
//    inchar = Serial.read();
   //DBG (inchar);
       switch(inchar)
         {
         case '$' :  
                     if (startfound)
                     {
                      //incomplete previous message already in buffer
                      intobuffer("\n");
                      LogBuffer(logtimestamp);
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
                      LogBuffer(logtimestamp);
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

void intobuffer( char _inchar)
{
        if (bufferposition >= BUFFER_SIZE)
        {
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

void LogBuffer(DateTime _timestamp)
{
  
  int i;
  
  dataFile = SD.open(filename, FILE_WRITE);
  if (!dataFile)
  {
    DBG ("error opening file for writing");
    return;
  }

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
  

  for (i = 0; i < bufferposition; i++)
  {
    dataFile.print(serialbuffer[i]);
  }
  dataFile.close();

  serialbuffer[i] = 0x00;

  DBG3 ("Logged one entry\n");
  DBG ("Serial buffer:  ");
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

  INIT  ("new filename :");
  INIT  (filename);
  INIT  ("\n");
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

