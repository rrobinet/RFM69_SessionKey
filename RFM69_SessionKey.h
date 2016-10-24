// Date: 11/10/2016
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
//			SESSION_KEY_RCV_STATUS = 0  Receiver: The received session key do match with the expected one
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
#ifndef RFM69_SessionKey_h
#define RFM69_SessionKey_h
#include <RFM69.h>

#define SESSION_KEY_LENGTH	4										  		// !RVDB define to the session key Length
#define RF69_HEADER_LENGTH  4 												// !RVDB define to the RFM standard Header Length
#define SESSION_HEADER_LENGTH	RF69_HEADER_LENGTH + SESSION_KEY_LENGTH	    // !RVDB define to the session Header Length (including RF69_HEADER_LENGTH)
#define SESSION_MAX_DATA_LEN 	RF69_MAX_DATA_LEN - SESSION_KEY_LENGTH		// !RVDB Define the Session maximum Data Length

class RFM69_SessionKey: public RFM69 {
  // !RVDB make all these variables private
public:
static volatile uint8_t SESSION_KEY_INCLUDED; 		// flag in CTL byte indicating this packet includes a session key
static volatile uint8_t SESSION_KEY_REQUESTED; 		// flag in CTL byte indicating this packet is a request for a session key
static volatile uint8_t SESSION_KEY_RCV_STATUS;		// !RVDB add a variable to indicate the session key status after receive was done 
private:
static volatile unsigned long SESSION_KEY; 			// !RVDB set to the session key value for a particular transmission
static volatile uint8_t SESSION_KEY1; 				// !RVDB set to the session key High value for a particular transmission
static volatile uint8_t SESSION_KEY2; 				// !RVDB set to the session key Medium value for a particular transmission
static volatile uint8_t SESSION_KEY3; 				// !RVDB set to the session key Medium value for a particular transmission
static volatile uint8_t SESSION_KEY4; 				// !RVDB set to the session key Low value for a particular transmission
static volatile unsigned long INCOMING_SESSION_KEY; // !RVDB set on an incoming packet and used to decide if receiveDone should be true and data should be processed
static volatile uint8_t INCOMING_SESSION_KEY1; 		// !RVDB set to the session key High value for a particular transmission
static volatile uint8_t INCOMING_SESSION_KEY2; 		// !RVDB set to the session key Medium value for a particular transmission
static volatile uint8_t INCOMING_SESSION_KEY3; 		// !RVDB set to the session key Medium value for a particular transmission
static volatile uint8_t INCOMING_SESSION_KEY4; 		// !RVDB set to the session key Low value for a particular transmission
static volatile uint16_t _waitTime; 					// !RVDB used to store the retryWaitTime for multiple ACK Send loop
static volatile uint16_t _respDelayTime; 		    // !RVDB used to store the Session KEY challenge response for slow remote nodes
 public:	
    RFM69_SessionKey(uint8_t slaveSelectPin=RF69_SPI_CS, uint8_t interruptPin=RF69_IRQ_PIN, bool isRFM69HW=false, uint8_t interruptNum=RF69_IRQ_NUM) :
      RFM69(slaveSelectPin, interruptPin, isRFM69HW, interruptNum) {
    }
    
    bool initialize(uint8_t freqBand, uint8_t ID, uint8_t networkID=1); // need to call initialise because _sessionKeyEnabled (new variable) needs to be set as false when first loaded
    void send(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK=false); // some additions needed
    void sendACK(const void* buffer = "", uint8_t bufferSize=0); // some additions needed
    bool receiveDone(); // some additions needed
    
    // new public functions for session handling
    void useSessionKey(bool enabled);
    bool sessionKeyEnabled();
    void useSession3Acks(bool enabled); 				// !RVDB new function to Enable 3 final ACKs
    bool session3AcksEnabled ();						// !RVDB new function to check if 3 final ACKs are enabled
    void sessionWaitTime(uint16_t waitTime);  			// !RVDB new function allowing to change of the watchdog time between Session request an Session included
    void sessionRespDelayTime(uint16_t delayTime);  	// !RVDB new function allowing to change the SESSION KEY delay response time for slow nodes

  protected:
   
    void interruptHook(uint8_t CTLbyte);
    void sendFrame(byte toAddress, const void* buffer, byte size, bool requestACK=false, bool sendACK=false);  // Need this one to match the RFM69 library
    void sendFrame(uint8_t toAddress, const void* buffer, uint8_t size, bool requestACK, bool sendACK, bool sessionRequested, bool sessionIncluded); // two parameters added for session key support
    void sendWithSession(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK=false, uint16_t retryWaitTime=40); // new function to transparently handle session without sketch needing to change
    void receiveBegin(); // some additions needed
    bool _sessionKeyEnabled; // protected variable to indicate if session key support is enabled
    bool _session3AcksEnabled; // !RVDB protected variable to indicate if 3 final Acks support is enabled
};

#endif