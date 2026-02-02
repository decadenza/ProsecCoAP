/**
 * Example of advanced message building.
 */
#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
IPAddress deviceIp(192, 168, 0, 99);                   // Set your own.

// UDP and CoAP class
EthernetUDP Udp;
// Coap coap(Udp);

void setup()
{
  Serial.begin(9600);

  Ethernet.begin(mac, deviceIp);
  Serial.print("IP address: ");
  Serial.print(Ethernet.localIP());
  Serial.println();
}

void loop()
{
  IPAddress destinationIp = IPAddress(192, 168, 0, 100); // Set your CoAP server IP address here.

  // Build a GET request message to transmit.
  Coap::Message msg(Coap::MessageType::Con, Coap::MessageCode::Get); // Initialise a new CoAP confirmable message, as GET request.
  // Optionally, add a token of 4 bytes. Use getToken() to retrieve it later.
  msg.addToken(4);                                                   
  Serial.print("Added token of length: ");
  Serial.println(msg.getTokenLength());

  Coap::ErrorCode err = msg.addHost(destinationIp);
  if(err!= Coap::ErrorCode::None) {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
    }

  err = msg.addPort(COAP_DEFAULT_PORT);
  if(err!= Coap::ErrorCode::None) {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
    }
    
    
  // Add CoAP options using the constructor...
  Coap::Option newOption(Coap::OptionNumber::UriPath,(const uint8_t *)"abcdefg", 7);
  err = msg.addOption(newOption);
  if(err!= Coap::ErrorCode::None) {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
    }
  // ...or passing the values directly.
  err = msg.addOption(Coap::OptionNumber::UriPath, (const uint8_t *)"test", 4);
  if(err!= Coap::ErrorCode::None) {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
    }
  
  // SECTION Read the options from the message.
  Coap::OptionIterator optIterator = msg.getOptionIterator();
  Coap::Option opt;
  while((err = optIterator.next(opt)) == Coap::ErrorCode::None) {
      Serial.print("Found option: ");
      Serial.print((int8_t)opt.number);
      Serial.print(", length: ");
      Serial.println((int8_t)opt.length);
    }
  if(err!= Coap::ErrorCode::NotFound) {
    Serial.print("Error while reading options: ");
    Serial.println((int8_t)err);
    }
   // !SECTION

  delay(1000);
}
