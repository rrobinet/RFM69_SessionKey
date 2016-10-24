// Sample RFM69 receiver/gateway sketch, with Session Key library

#include <RFM69_SessionKey.h> // enable session key support extension for RFM69 base library
#include <RFM69.h>            //get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>

#define NODEID        1    //unique for each node on same network
#define NETWORKID     110  //the same on all nodes that talk to each other
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
//#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define SERIAL_BAUD   115200
#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGA have LED on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteino has LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
#endif
long int NOANS = 0;     // used to count the answer errors
RFM69_SessionKey radio;       // create radio instance
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network
bool SESSION_KEY = true;      // set usage of session mode (or not)
bool SESSION_3ACKS = false;   // set 3 acks at the end of a session transfer (or not)
unsigned long SESSION_WAIT_TIME = 40; // adjust wait time of data recption in session mode (default is 40ms)
byte ackCount=0;              // use to count 
uint32_t packetCount = 0;     // use to count the received packets

void setup()
{
  Serial.begin(SERIAL_BAUD);
  delay(10);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);            // set encryption
  radio.useSessionKey(SESSION_KEY);     // set session mode
  radio.promiscuous(promiscuousMode);   // set promiscuous mode
  radio.sessionWaitTime(40);            // adjust wait time of data recption in session mode (default is 40ms) 
  char buff[50];
  sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
}

void loop() {
  //process any serial input
  if (Serial.available() > 0)
  {
    char input = Serial.read();
    if (input == 't')
    {
      byte temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
      byte fTemp = 1.8 * temperature + 32; // 9/5=1.8
      Serial.print( "Radio Temp is ");
      Serial.print(temperature);
      Serial.print("C, ");
      Serial.print(fTemp); //converting to F loses some resolution, obvious when C is on edge between 2 values (ie 26C=78F, 27C=80F)
      Serial.println('F');
    }
  }

  if (radio.receiveDone())
  {
    Serial.print("#[");
    Serial.print(++packetCount);
    Serial.print(']');
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    if (promiscuousMode)
    {
      Serial.print("to [");Serial.print(radio.TARGETID, DEC);Serial.print("] ");
    }
    for (byte i = 0; i < radio.DATALEN; i++)
      Serial.print((char)radio.DATA[i]);
    Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");
    
    if (radio.ACKRequested())
    {
      byte theNodeID = radio.SENDERID;
      radio.sendACK();
      Serial.print(" - ACK sent.");

      // When a node requests an ACK, respond to the ACK
      // and also send a packet requesting an ACK (every 3rd one only)
      // This way both TX/RX NODE functions are tested on 1 end at the GATEWAY
      if (ackCount++%3==0)
      {
        Serial.print("\n\rPinging node ");
        Serial.print(theNodeID);
        Serial.print(" - ACK...");
        delay(3); //need this when sending right after reception .. ?
        if (radio.sendWithRetry(theNodeID, "ACK TEST", 8, 0))  // 0 = only 1 attempt, no retries
        {
          Serial.println (" SEND OK");
          if (SESSION_KEY)
          {
            Serial.print("Session key receive status: ");
            switch (RFM69_SessionKey::SESSION_KEY_RCV_STATUS)
            {
              case (0):
                Serial.println ("Receiver: The received session key DO match with the expected one");
                break;
              case (1):
                Serial.println ("Receiver: A session key is requested and send");
                break;
              case (2):
                Serial.println ("Sender: A session key is received and computed");
                break;
              case (3):
                Serial.println ("Receiver: The received session key DO NOT match with the expected one");
                break;
              case (4):
                Serial.println ("Receiver: No Data received (time-out) or Data without Session Key received");
                break;
              default:
                break;       
            }
          }
        }
        else 
        {
          Serial.print ("  !!! NO ANSWER COUNT: "); Serial.println (++NOANS);  
          if (SESSION_KEY)
          {
            Serial.print("Last Received status is: ");
            switch (RFM69_SessionKey::SESSION_KEY_RCV_STATUS)
            {
              case (0):
                      Serial.println ("Receiver: The received session key DO match with the expected one");
                     break;
              case (1):
                Serial.println ("Receiver: A session key is requested and send");
                break;
              case (2):
                      Serial.println ("Sender: A session key is received and computed");
                      break;
              case (3):
                      Serial.println ("Receiver: The received session key DO NOT match with the expected one");
                     break;
              case (4):
                      Serial.println ("Receiver: No Data received (time-out) or Data without Session Key received");
                      break;
              default:
                      break;       
            }
          }
        }
      }
    }
    Serial.println();
    Blink(LED,3);
  }
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
