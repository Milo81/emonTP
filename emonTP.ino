/*
  emonTP barometric pressure & temp sensor
 
  Licence: GNU GPL V3
 
  Author: Miloslav Fritz
  Builds upon JeeLabs RF12 library and Arduino

  THIS SKETCH REQUIRES:

  Libraries in the standard arduino libraries folder:
	- JeeLib		https://github.com/jcw/jeelib
  - SFE_BMP180        https://github.com/sparkfun/BMP180_Breakout
*/

#define freq RF12_868MHZ                                                // Frequency of RF12B module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.
const int nodeID = 20;                                                  // emonTx RFM12B node ID - should be unique on network, see recomended node ID range below
const int networkGroup = 210;                                           // emonTx RFM12B wireless network group - needs to be same as emonBase and emonGLCD

/*Recommended node ID allocation
------------------------------------------------------------------------------------------------------------
-ID-	-Node Type- 
0	- Special allocation in JeeLib RFM12 driver - reserved for OOK use
1-4     - Control nodes 
5-10	- Energy monitoring nodes
11-14	--Un-assigned --
15-16	- Base Station & logging nodes
17-30	- Environmental sensing nodes (temperature humidity etc.)
31	- Special allocation in JeeLib RFM12 driver - Node31 can communicate with nodes on any network group
-------------------------------------------------------------------------------------------------------------
*/

// altitude - needed to convert pressure to sea level pressure
#define ALTITUDE 165

// delay between readings in minutes
#define TIME_BETWEEN_READINGS 5
                                           
#include <JeeLib.h>                                                     // Download JeeLib: http://github.com/jcw/jeelib
#include <avr/sleep.h>

#include <SFE_BMP180.h>
#include <Wire.h>

ISR(WDT_vect) { Sleepy::watchdogEvent(); }                              // Attached JeeLib sleep function to Atmega328 watchdog -enables MCU to be put into sleep mode inbetween readings to reduce power consumption 


typedef struct {
  	  int temp;
      long pressure;
} Payload;
Payload emontx;

SFE_BMP180 pressure;

void setup() {
  Serial.begin(9600);
  Serial.println("emonTX Low-power humidity example"); 
  Serial.println("OpenEnergyMonitor.org");
  Serial.print("Node: "); 
  Serial.print(nodeID); 
  Serial.print(" Freq: "); 
  if (freq == RF12_433MHZ) Serial.print("433Mhz");
  if (freq == RF12_868MHZ) Serial.print("868Mhz");
  if (freq == RF12_915MHZ) Serial.print("915Mhz"); 
  Serial.print(" Network: "); 
  Serial.println(networkGroup);
 
  delay(2000);                                                          // wait 2s
 
  rf12_initialize(nodeID, freq, networkGroup);                          // initialize RFM12B
  rf12_control(0xC040);                                                 // set low-battery level to 2.2V i.s.o. 3.1V
  delay(10);
  rf12_sleep(RF12_SLEEP);

  if (pressure.begin())
    Serial.println("BMP180 init success");
  else
  {
    // Oops, something went wrong, this is usually a connection problem,
    Serial.println("BMP180 init fail\n\n");
    while(1); // Pause forever.
  }

}

void loop()
{ 
  
  char status;
  double T,P,p0;

  status = pressure.startTemperature();
  if (status != 0)
  {
    // Wait for the measurement to complete:
    delay(status);

    // Retrieve the completed temperature measurement:
    // Note that the measurement is stored in the variable T.
    // Function returns 1 if successful, 0 if failure.

    status = pressure.getTemperature(T);
    if (status != 0)
    {
      // Print out the measurement:
      Serial.print("temperature: ");
      Serial.print(T,2);
      Serial.println(" deg C");
      emontx.temp = T * 100.0;

      status = pressure.startPressure(3);

      if (status != 0)
      {
        // Wait for the measurement to complete:
        delay(status);

        status = pressure.getPressure(P,T);
        if (status != 0)
        {
          // Print out the measurement:
          Serial.print("absolute pressure: ");
          Serial.print(P,2);
          Serial.println(" mb, ");

          // recalculate to sea level pressure
          p0 = pressure.sealevel(P,ALTITUDE);

          Serial.print("relative (sea-level) pressure: ");
          Serial.print(p0,2);
          Serial.println(" mb, ");
          emontx.pressure = p0 * 100;
        }
      }
    }
  }

  // check if returns are valid, if they are NaN (not a number) then something went wrong!

  Serial.print("About to send: ");
  Serial.print(emontx.temp);
  Serial.print(" : ");
  Serial.println(emontx.pressure);
  delay(500);
  rf12_sleep(RF12_WAKEUP);
  // if ready to send + exit loop if it gets stuck as it seems too
  int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}
  rf12_sendStart(0, &emontx, sizeof emontx);
  // set the sync mode to 2 if the fuses are still the Arduino default
  // mode 3 (full powerdown) can only be used with 258 CK startup fuses
  rf12_sendWait(2);
  rf12_sleep(RF12_SLEEP); 
  delay(50);
  
  for(int i=0; i<TIME_BETWEEN_READINGS; i++){
      Sleepy::loseSomeTime(60000);
  }
}