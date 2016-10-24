// Date: 18/10/2016
// This Session Key library is based one the original one of:
// https://github.com/dewoodruff/RFM69_SessionKey (February 2015)
// Modifications by rrobinet (see !RVDB):
// 1.	One Byte Random Session Key is replaced by a 4 Bytes system time
// 2.	Some new variables are defined for standardisation
//			SESSION_KEY_LENGTH	4										  		
//			RF69_HEADER_LENGTH  4 												
//			SESSION_HEADER_LENGTH	RF69_HEADER_LENGTH + SESSION_KEY_LENGTH	   
// 			SESSION_MAX_DATA_LEN 	RF69_MAX_DATA_LEN - SESSION_KEY_LENGTH		
//	3.	A test is done on Broadcast node destination before sending a session
//	4.	A test is done at the session receiver to avoid receiving session data when the node is in promiscuous mode
//	5.	New function (useSession3Acks) allowing 3 ACKs to be sent by the receiver at the end of the session instead of one
//  6.  New function (session3AcksEnabled) allowing to check the Session3Acks value
//	7.  New function (sessionWaitTime) allowing to change the SESSION KEY request response time watchdog
//	8.	New function (sessionRespDelay) allowing to change SESSION KEY response delay for slow remote node
//  9.  Add SESSION_KEY_RCV_STATUS to be able to evaluate a Session negotiation failure
//			SESSION_KEY_RCV_STATUS = 1	Receiver: A session key is requested and sent
//			SESSION_KEY_RCV_STATUS = 2  Sender: A session key is received an computed
//			SESSION_KEY_RCV_STATUS = 3  Receiver: The received session key doesn't match with the expected one
//			SESSION_KEY_RCV_STATUS = 4  Receiver: No Data received (time-out) or Data without Session Key received
//			SESSION_KEY_RCV_STATUS = 0  Receiver: The received session key does match with the expected one
//	10. Modify _respDelayTime from uint8_t to uint16_t
//  11. Add AVR check (ifdef SREG) while saving SREG values, to maintain compatibility with ESP8266 
//  12. Correct typo in RFM69_SessionKey::initialise instead of RFM69_SessionKey::initialize
//  13. Improve messages of 9.
//	14. Correct restore Interrupt in receiveDone() function for ESP8266 compatibilities 
// **********************************************************************************
// Session key class derived from RFM69 library. Session key prevents replay of wireless transmissions.
// **********************************************************************************
// Copyright Dan Woodruff
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// You should have received a copy of the GNU General    
// Public License along with this program.
// If not, see <http://www.gnu.org/licenses/>.
//                                                        
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************
// Notes
// **1 Indicate modification for Interrupt Control 
#include <RFM69_SessionKey.h>
#include <RFM69.h>   // include the RFM69 library files as well
#include <RFM69registers.h>
#include <SPI.h>

volatile uint8_t RFM69_SessionKey::SESSION_KEY_INCLUDED; // flag in CTL byte indicating this packet includes a session key
volatile uint8_t RFM69_SessionKey::SESSION_KEY_REQUESTED; // flag in CTL byte indicating this packet is a request for a session key
volatile uint8_t RFM69_SessionKey::SESSION_KEY_RCV_STATUS;		    //***** !RVDB add a variable to indicate the type the session key status after receive was done

volatile unsigned long RFM69_SessionKey::SESSION_KEY; // !RVDB set to the session key value for a particular transmission
volatile uint8_t RFM69_SessionKey::SESSION_KEY1; // !RVDB set to the session key High value for a particular transmission
volatile uint8_t RFM69_SessionKey::SESSION_KEY2; // !RVDB set to the session key Medium value for a particular transmission
volatile uint8_t RFM69_SessionKey::SESSION_KEY3; // !RVDB set to the session key Medium value for a particular transmission
volatile uint8_t RFM69_SessionKey::SESSION_KEY4; // !RVDB set to the session key Low value for a particular transmission
volatile unsigned long RFM69_SessionKey::INCOMING_SESSION_KEY; // !RVDB set on an incoming packet and used to decide if receiveDone should be true and data should be processed
volatile uint8_t RFM69_SessionKey::INCOMING_SESSION_KEY1; // !RVDB set to the session key High value for a particular transmission
volatile uint8_t RFM69_SessionKey::INCOMING_SESSION_KEY2; // !RVDB set to the session key Medium value for a particular transmission
volatile uint8_t RFM69_SessionKey::INCOMING_SESSION_KEY3; // !RVDB set to the session key Medium value for a particular transmission
volatile uint8_t RFM69_SessionKey::INCOMING_SESSION_KEY4; // !RVDB set to the session key Low value for a particular transmission
volatile uint16_t RFM69_SessionKey::_waitTime; 	  // !RVDB used to store the retryWaitTime (ms) for multiple ACK Send loop
volatile uint16_t RFM69_SessionKey::_respDelayTime; 	  // !RVDB used to store the Session KEY challenge response for slow remote nodes
//=============================================================================
// initialize() - Some extra initialisation before calling base class
//=============================================================================
bool RFM69_SessionKey::initialize(uint8_t freqBand, uint8_t nodeID, uint8_t networkID)
{
  _sessionKeyEnabled = false; 							// default to disabled
  _session3AcksEnabled = false; 						// !RVDB initialise the 3 final Acks Options
  SESSION_KEY_INCLUDED = 0;
  SESSION_KEY_REQUESTED = 0;
  _waitTime = 40;										// !RVDB initialise the default watchdog time between Session Request and Session Included
  _respDelayTime = 0;									// !RVDB initialise the Session KEY response delay 
  return RFM69::initialize(freqBand, nodeID, networkID);// use base class to initialise everything else
}

//=============================================================================
// send() - Modified to either send the raw frame or send with session key 
//          if session key is enabled
//=============================================================================
void RFM69_SessionKey::send(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK)
{ 
  // !RVDB Do not Send Session Data to the Broadcast node ID)
  if (toAddress == RF69_BROADCAST_ADDR) return; 
  writeReg(REG_PACKETCONFIG2, (readReg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART); // avoid RX deadlocks
  uint32_t now = millis();
  while (!canSend() && millis() - now < RF69_CSMA_LIMIT_MS) receiveDone();
  if (sessionKeyEnabled())
  {
    sendWithSession(toAddress, buffer, bufferSize, requestACK, _waitTime); // !RVDB add the _waitTime parameter

  }
  else
  {
    sendFrame(toAddress, buffer, bufferSize, requestACK, false);
  }  
}

//=============================================================================
// sendWithSession() - Function to do the heavy lifting of session handling so it is transparent to sketch
//=============================================================================
void RFM69_SessionKey::sendWithSession(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK, uint16_t retryWaitTime) {
//    Serial.print("\n\r Send with Session; Request ACK is: "), Serial.print(requestACK), Serial.print(" Wait Time is: "), Serial.println (retryWaitTime);
  // reset session key to blank value to start
  SESSION_KEY = 0;
  // start the session by requesting a key. don't request an ACK - ACKs are handled at the whole session level
  //Serial.println("sendWithSession: Requesting session key.");
  sendFrame(toAddress, null, 0, false, false, true, false);
  receiveBegin();
  // loop until session key received, or timeout
  uint32_t sentTime = millis();
  while ((millis() - sentTime) < retryWaitTime && SESSION_KEY == 0);
  if (SESSION_KEY == 0) 
  {
    SESSION_KEY_RCV_STATUS = 4;  // !RVDB Receiver: No Data received or Data without Session Key received
  // Serial.println("sendWithSession: SESSION_KEY = 0");
    return;
  }
//  Serial.print("Session Data after Time: "); Serial.println (millis()-sentTime); Serial.print("sendWithSession: Received key: ");
//  Serial.print("Request ACK: ");Serial.println(requestACK); Serial.println(SESSION_KEY);
  // finally send the data! request the ACK if needed
  sendFrame(toAddress, buffer, bufferSize, requestACK, false, false, true);
}

//=============================================================================
// sendAck() - Updated to call new sendFrame with additional parameters.
//             Should be called immediately after reception in case sender wants ACK
//=============================================================================
void RFM69_SessionKey::sendACK(const void* buffer, uint8_t bufferSize) {
  uint8_t sender = SENDERID;
  uint8_t receiver = TARGETID;										// !RVDB Save the recipient node address
  int16_t _RSSI = RSSI; // save payload received RSSI value
  writeReg(REG_PACKETCONFIG2, (readReg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART); // avoid RX deadlocks
  uint32_t now = millis();
  while (!canSend() && millis() - now < RF69_CSMA_LIMIT_MS) receiveDone();
  // if session keying is enabled, call sendFrame to include the session key
  // otherwise send as the built in library would
  // !RVDB Send 3 consecutive ACK to ensure the message both sender and recipient synchronisation (case of one ACK answer was lost)
  if (sessionKeyEnabled())
  {
  	int acks = 3;
  	if (!session3AcksEnabled()) acks = 1; 								// !RVDB adapt the final Acks according the the 3Acks enable value 
     for (int i = 0; i <acks; i++) 
     {
       SENDERID = sender;          										// !RVDB Restore the sender ID (cleared after each sendAck message)
       TARGETID = receiver;             								// !RVDB Restore the target ID (cleared after each sendAck message
       sendFrame(sender, buffer, bufferSize, false, true, false, true);  
       delay (_waitTime/4);   											// !RVDB Ensure that total transmit time is less than the receiver ACK window time         									         
     }
  }
  else
    sendFrame(sender, buffer, bufferSize, false, true);
  RSSI = _RSSI; // restore payload RSSI
}
    
//=============================================================================
// sendFrame() - The basic version is used to match the RFM69 library so it can be extended
//               This sendFrame is generally called by the internal RFM69 functions
//               Simply transfer to the modified version with additional parameters
//=============================================================================
void RFM69_SessionKey::sendFrame(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK, bool sendACK)
{
//  Serial.print("\n\r Send without Session; Request ACK is: "), Serial.println(requestACK); 
  sendFrame(toAddress, buffer, bufferSize, requestACK, sendACK, false, false);
}

//=============================================================================
// sendFrame() - New with additional parameters. Handles the CTLbyte bits needed for session key
//=============================================================================
void RFM69_SessionKey::sendFrame(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK, bool sendACK, bool sessionRequested, bool sessionIncluded)
 {
//  Serial.print("\n\r Send Frame; Request ACK is: "), Serial.print(requestACK);Serial.print (" - Send ACK is: "); Serial.print (sendACK); Serial.print (" - Session RQST is: "); Serial.print (sessionRequested);Serial.print (" - Session INC: "); Serial.println (sessionIncluded);
  setMode(RF69_MODE_STANDBY); // turn off receiver to prevent reception while filling fifo
  while ((readReg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00); // wait for ModeReady
  writeReg(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_00); // DIO0 is "Packet Sent"
  // !RVDB Correct the buffer size length according to the session Header
  if (bufferSize > SESSION_MAX_DATA_LEN) bufferSize = SESSION_MAX_DATA_LEN;
  
  // start with blank control byte
  uint8_t CTLbyte = 0x00;
  // layer on the bits to the CTLbyte as needed
  if (sendACK)
  {
    CTLbyte = CTLbyte | RFM69_CTL_SENDACK;
   //Serial.println("Sendframe: Send ACK");
  }
  if (requestACK)
  {
    CTLbyte = CTLbyte | RFM69_CTL_REQACK;
   //Serial.println("Sendframe: Request ACK");
  }
  if (sessionRequested)
  {
    CTLbyte = CTLbyte | RFM69_CTL_EXT1;
   //Serial.println("Sendframe: Session requested");
  }
  if (sessionIncluded)
  {
    CTLbyte = CTLbyte | RFM69_CTL_EXT2;
   //Serial.println("Sendframe: Session included");
  } 
    // write to FIFO
  select();
  SPI.transfer(REG_FIFO | 0x80);
  if (sessionIncluded)
    SPI.transfer(bufferSize + SESSION_HEADER_LENGTH-1); // !RVDB use Session Header definition
  else
  SPI.transfer(bufferSize + RF69_HEADER_LENGTH-1); 	    // !RVDB use RF69 header definition
  SPI.transfer(toAddress);
  SPI.transfer(_address);
  SPI.transfer(CTLbyte);
  if (sessionIncluded)
  {
    delayMicroseconds (_respDelayTime);			   		// !RVDB used to delayed transmission after a session request for slow remote node
    SPI.transfer(SESSION_KEY1);							// !RVDB Session Key Higher Byte
    SPI.transfer(SESSION_KEY2);							// !RVDB Session Key Medium Byte
    SPI.transfer(SESSION_KEY3);							// !RVDB Session Key Medium Byte
    SPI.transfer(SESSION_KEY4);							// !RVDB Session Key Lower Byte
  } 
   for (uint8_t i = 0; i < bufferSize; i++)
  { 
    SPI.transfer(((uint8_t*) buffer)[i]);
  } 
  unselect();
  // no need to wait for transmit mode to be ready since its handled by the radio
  setMode(RF69_MODE_TX);
  uint32_t txStart = millis();
  while (digitalRead(_interruptPin) == 0 && millis() - txStart < RF69_TX_LIMIT_MS); // wait for DIO0 to turn HIGH signalling transmission finish
  //while (readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PACKETSENT == 0x00); // wait for ModeReady
  setMode(RF69_MODE_STANDBY);
 }

//=============================================================================
// interruptHook() - Gets called by the base class interruptHandler right after the header is fetched
//=============================================================================
void RFM69_SessionKey::interruptHook(uint8_t CTLbyte) {
  SESSION_KEY_REQUESTED = CTLbyte & RFM69_CTL_EXT1; // extract session key request flag
  SESSION_KEY_INCLUDED = CTLbyte & RFM69_CTL_EXT2; //extract session key included flag
 
  // if a new session key was requested, send it right here in the interrupt to avoid having to handle it in sketch manually, and for greater speed
  if (sessionKeyEnabled() && SESSION_KEY_REQUESTED && !SESSION_KEY_INCLUDED) {
    unselect();
//    Serial.println("SESSION_KEY_REQUESTED && NO SESSION_KEY_INCLUDED");
    setMode(RF69_MODE_STANDBY);
    // !RVDB use system up time to generate a key
    SESSION_KEY = millis();
    SESSION_KEY1 = SESSION_KEY>>24;		// !RVDB Save Session Key Higher Bytes 
    SESSION_KEY2 = SESSION_KEY>>16;		// !RVDB Save Session Key Medium Bytes 
    SESSION_KEY3 = SESSION_KEY>>8;		// !RVDB Save Session Key Medium Bytes 
    SESSION_KEY4 = SESSION_KEY;			// !RVDB Save Session Key Lower Bytes 
    // send it!
    sendFrame(SENDERID, null, 0, false, false, true, true);
    // don't process any data
    SESSION_KEY_RCV_STATUS = 1;			// !RVDB Session Key is requested and send
	DATALEN = 0;
    return;
  }
  // if both session key bits are set, the incoming packet has a new session key
  // set the session key and do not process data
  if (sessionKeyEnabled() && SESSION_KEY_REQUESTED && SESSION_KEY_INCLUDED) {
    // !RVDB Get the Session Bytes
    SESSION_KEY1 = SPI.transfer(0);
    SESSION_KEY2 = SPI.transfer(0);
    SESSION_KEY3 = SPI.transfer(0);
    SESSION_KEY4 = SPI.transfer(0);
    // !RVDB Concatenate the Session Bytes 
    SESSION_KEY = (unsigned long)(SESSION_KEY1)<<24 | (unsigned long) (SESSION_KEY2)<<16 | (unsigned long) (SESSION_KEY3) <<8 |(unsigned long) SESSION_KEY4; 
    // don't process any data
	DATALEN = 0;
	SESSION_KEY_RCV_STATUS = 2;			// !RVDB Session key is received and computed
//	Serial.println("SESSION_KEY_REQUESTED && SESSION_KEY_INCLUDED");
    return;
  }
  // if a session key is included, make sure it is the key we expect
  // if the key does not match, do not set DATA and return false
  if (sessionKeyEnabled() && SESSION_KEY_INCLUDED && !SESSION_KEY_REQUESTED) {
    //   Serial.println("SESSION_KEY_INCLUDED && NO SESSION_KEY_REQUESTED");
    //   Serial.print("CONTROL Byte; "), Serial.println (CTLbyte,HEX);
    // !RVDB Get the Session Incoming Bytes
    INCOMING_SESSION_KEY1 = SPI.transfer(0);
    INCOMING_SESSION_KEY2 = SPI.transfer(0);
    INCOMING_SESSION_KEY3 = SPI.transfer(0);
    INCOMING_SESSION_KEY4 = SPI.transfer(0);
    // !RVDB Concatenate the Incoming Session Bytes
    INCOMING_SESSION_KEY = (unsigned long)(INCOMING_SESSION_KEY1)<<24 | (unsigned long) (INCOMING_SESSION_KEY2)<<16 | (unsigned long) (INCOMING_SESSION_KEY3) <<8 |(unsigned long) INCOMING_SESSION_KEY4;
    if (INCOMING_SESSION_KEY != SESSION_KEY){
       //Serial.print ("Received frame: "); Serial.println("Session Key received DO NOT match the Session Key send");
       SESSION_KEY_RCV_STATUS = 3; 		// !RVDB The Session key received doesn't match with the expected one
      // don't process any data
	  DATALEN = 0;
      return;
    }
    // !RVDB if keys do match, actual data is payload minus the Session header Length -1
    DATALEN = PAYLOADLEN - (SESSION_HEADER_LENGTH-1);  // !RVDB use the Session Key length definition
       //Serial.print ("Received frame: "); Serial.println("Session Key received DO match the Session Key send");
       SESSION_KEY_RCV_STATUS = 0;		// !RVDB The received session key match the expected one
    return;
  }
}

//=============================================================================
//  receiveBegin() - Need to clear out session flags before calling base class function
//=============================================================================
void RFM69_SessionKey::receiveBegin() {	
  SESSION_KEY_INCLUDED = 0;
  SESSION_KEY_REQUESTED = 0;
  RFM69::receiveBegin();
}

//=============================================================================
//  receiveDone() - Added check so if session key is enabled, incoming key is checked 
//                  against the stored key for this session. Returns false if not matched
//=============================================================================
 bool RFM69_SessionKey::receiveDone() {
//ATOMIC_BLOCK(ATOMIC_FORCEON)
//{
  if (sessionKeyEnabled() && _promiscuousMode)
  {
    return false; // !RVDB Avoid to received data when node is in promiscuous mode
  }
#ifdef SREG  		// !RVDB check for AVR environment
_SREG = SREG; 		//  Save Interrupt Control
#endif
  noInterrupts(); // re-enabled in unselect() via setMode() or via receiveBegin()
  if (_mode == RF69_MODE_RX && PAYLOADLEN > 0)
  {
    // if session key on and keys don't match
    // return false, as if nothing was even received
    if (sessionKeyEnabled() && INCOMING_SESSION_KEY != SESSION_KEY) {
      interrupts(); // explicitly re-enable interrupts
      receiveBegin();
      return false;
    }
    setMode(RF69_MODE_STANDBY); // enables interrupts
#ifdef SREG     	// !RVDB check for AVR environment
    SREG = _SREG; 	// Interrupt Control - Restore interrupts
#endif   
    return true;
  }
  else if (_mode == RF69_MODE_RX) // already in RX no payload yet
  {
#ifdef SREG    		// !RVDB check for AVR environment
    SREG = _SREG; 	// Interrupt Control - Restore interrupts
#endif
    interrupts(); // Interrupt Control - Restore interrupts
    return false;
  }
  receiveBegin();
#ifdef SREG    		// !RVDB check for AVR environmen
    SREG = _SREG; 	// Interrupt Control - Restore interrupts
#endif
  return false;
}

//=============================================================================
//  useSessionKey() - Enables session key support for transmissions
//=============================================================================
void RFM69_SessionKey::useSessionKey(bool onOff) {
  _sessionKeyEnabled = onOff;
}
//=============================================================================
//  sessionKeyEnabled() - Check if session key support is enabled
//=============================================================================
bool RFM69_SessionKey::sessionKeyEnabled() {
  return _sessionKeyEnabled;
}
//=============================================================================
//  ! RVDB New function
//  useSession3Acks() - Enables 3 ACKS in stead of 1 for final acknowledgement
//=============================================================================
void RFM69_SessionKey::useSession3Acks(bool onOff) {
  _session3AcksEnabled = onOff;
}
//=============================================================================
//  ! RVDB New function
//   session3AcksEnabled() - Check if 3Acks option key is enabled
//=============================================================================
bool RFM69_SessionKey::session3AcksEnabled() {
  return _session3AcksEnabled;
}
//=============================================================================
//  ! RVDB New function
//   sessionWaitTime() - Set the SESSION KEY request response time watchdog
//=============================================================================
void RFM69_SessionKey::sessionWaitTime(uint16_t waitTime) {
  if (waitTime == 0) _waitTime = 40;				// if the value is 0 use the default one of 40ms
  else _waitTime = waitTime;
}
//=============================================================================
//  ! RVDB New function
//   sessionRespDelay() - Set the SESSION KEY response delay for slow remote node
//=============================================================================
void RFM69_SessionKey::sessionRespDelayTime(uint16_t respDelayTime) {
  if (respDelayTime > 500) _respDelayTime = 500;				// if the value is 0 use the maximum one of 500us
  else _respDelayTime = respDelayTime;
}