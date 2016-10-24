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