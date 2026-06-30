/**
 * @file messageBuilder.ino
 * @brief Examples of advanced message building.
 *
 * @note Message building does not require to initialise the CoAP node.
 * See the client example to see how to send the message after building it.
 */
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
IPAddress deviceIp(192, 168, 0, 99);                   // Set your own.
IPAddress destinationIp = IPAddress(192, 168, 0, 100); // Set your CoAP server IP address here.

// UDP and CoAP class
EthernetUDP Udp;

void setup()
{
  Serial.begin(115200);

  Ethernet.begin(mac, deviceIp);
  Serial.print("IP address: ");
  Serial.print(Ethernet.localIP());
  Serial.println();
}

void loop()
{
  Serial.println("[Building new message]");
  // Build a GET request message to transmit.
  Coap::Message msg(Coap::MessageType::CON, Coap::MessageCode::GET); // Initialise a new CoAP confirmable message, as GET request.

  // Optionally, add a token of 4 bytes. Use getToken() to retrieve it later.
  msg.addRandomToken(4);
  Serial.print("Added token of length: ");
  Serial.println(msg.getTokenLength());

  // You can add a payload.
  Coap::ErrorCode err = msg.addPayload((const uint8_t *)(&"My text payload"), 15, Coap::ContentFormat::TEXT_PLAIN);
  if (err != Coap::ErrorCode::OK)
  {
    Serial.print("Error while adding payload: ");
    Serial.println((int8_t)err);
  }

  err = msg.addHost(destinationIp); // Uri-Host is optional (often not necessary, if destination host is the same IP).
  if (err != Coap::ErrorCode::OK)
  {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
  }

  err = msg.addPort(COAP_DEFAULT_PORT); // Optional. If not present will default to the default port anyway.
  if (err != Coap::ErrorCode::OK)
  {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
  }

  // Add Uri-Path options using the helper method.
  err = msg.addPath("sensors/temp");
  if (err != Coap::ErrorCode::OK)
  {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
  }

  // You may add advanced CoAP options using the constructor...
  char value[] = "this is 15 long";
  Coap::Option newOption(Coap::OptionNumber::LOCATION_PATH, (const uint8_t *)value, strlen(value));
  err = msg.addOption(newOption);
  if (err != Coap::ErrorCode::OK)
  {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
  }
  // ...or passing the values directly.
  char value2[] = "this is much much much much more";
  err = msg.addOption(Coap::OptionNumber::LOCATION_PATH, (const uint8_t *)value2, strlen(value2));
  if (err != Coap::ErrorCode::OK)
  {
    Serial.print("Error while adding option: ");
    Serial.println((int8_t)err);
  }

  /**
   * PRO-TIP: The library supports building a message in any order.
   * However, given the structure of the CoAP message, some memmove calls
   * will be necessary.
   *
   * It is slightly more efficient to perform operations in this order:
   * 1. Instantiate the message with type and code.
   * 2. Add a token.
   * 3. Add options from the option with lower OptionNumber to the higher one.
   * 4. Lastly, add a payload.
   */

  // Reading the message content.

  // Read the current options from the message using an iterator.
  Coap::OptionIterator optIterator = msg.getOptionIterator();
  Coap::Option opt;
  while ((err = optIterator.next(opt)) == Coap::ErrorCode::OK)
  {
    Serial.print("Found option: ");
    Serial.print((unsigned)opt.number);
    Serial.print(", length: ");
    Serial.print(opt.length);
    Serial.print(", value: 0x"); // Value is printed both in HEX and as characters.
    for (size_t i = 0; i < opt.length; i++)
    {
      Serial.print(opt.value[i] >> 4, HEX);
      Serial.print(opt.value[i] & 0x0F, HEX);
    }
    Serial.print(" (");
    Serial.write(opt.value, opt.length);
    Serial.println(")");
  }
  if (err != Coap::ErrorCode::NOT_FOUND)
  {
    Serial.print("Error while reading options: ");
    Serial.println((int8_t)err);
  }
  // Else, a NOT_FOUND error means that we correctly reached the end of the options.

  // Read the payload from the message.
  const uint8_t *payload;
  size_t payloadLength;
  err = msg.getPayload(payload, payloadLength);
  if (err != Coap::ErrorCode::OK)
  {
    Serial.print("Error while reading payload: ");
    Serial.println((int8_t)err);
  }
  else
  {
    Serial.print("Payload found: ");
    Serial.write(payload, payloadLength); // Write as raw bytes. You may need to interpret it in other ways.
    Serial.println();
  }

  delay(2000);
}
