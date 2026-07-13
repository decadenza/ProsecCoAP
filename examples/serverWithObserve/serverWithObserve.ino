/**
 * @file serverWithObserve.ino
 * @brief A CoAP server with Observe functionality.
 *
 * Author: Pasquale Lafiosca (2026)
 *
 * # Testing with libcoap
 * To test this example with the coap-client tool from libcoap:
 * ```
 * coap-client-notls -p 5683 -v 9 -s 60 coap://192.168.0.1/time
 * ```
 *
 * You will see binary data incoming. Adjust IP address and port as needed.
 *
 * Note that the option `-s 60` will set the CoAP observe register option and keep the
 * command running to receive notifications for the given amount of seconds.
 *
 * Notes:
 * - `-p 5683` sets a fixed port.
 * - `-T AAAA` optionally sets a fixed starting token (ASCII, libcoap will use the next one).
 *
 * Multiple subscriptions with equal IP, port, path and token will result in
 * the same observer.
 */
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ProsecCoAP.h>

// --- DEBUG MACROS ---
// Set to 1 to enable Serial output for debugging. Set to 0 to disable all Serial calls (no-ops).
#define ENABLE_SERIAL_DEBUG 1

#if ENABLE_SERIAL_DEBUG
#define SERIAL_BEGIN(baud) Serial.begin(baud)
#define SERIAL_WHILE_WAIT \
    while (!Serial)       \
        ;
#define SERIAL_PRINT(...) Serial.print(__VA_ARGS__)
#define SERIAL_PRINTLN(...) Serial.println(__VA_ARGS__)
#define SERIAL_WRITE(...) Serial.write(__VA_ARGS__)
#else
#define SERIAL_BEGIN(baud)
#define SERIAL_WHILE_WAIT
#define SERIAL_PRINT(...)
#define SERIAL_PRINTLN(...)
#define SERIAL_WRITE(...)
#endif

// UDP and CoAP instances.
EthernetUDP Udp;
Coap::Node coapNode(Udp);

// Define an observer registry with maximum capacity for 5 observers.
// This registry is binded to a specific resource!
Coap::ObserverRegistry<5> myObservers;

// Using a sequential identifier. IP will be based on this.
#define DEVICE_ID 1

byte mac[] = {0xBE, 0xEF, 0xBE, 0xEF, 0x00, DEVICE_ID}; // Define the MAC address, this must be unique.
IPAddress ip(192, 168, 0, DEVICE_ID);                   // This device IP.
IPAddress dns(0, 0, 0, 0);                              // Optional. Set your own.
IPAddress gateway(0, 0, 0, 0);                          // Optional. Set your own.
IPAddress subnet(255, 255, 255, 0);                     // Optional. Set your own.

/**
 * @brief Declaration of our callback to liked to the "time" resource.
 *
 * For the purpose of this example, our resource is simply `millis()`.
 */
void timeCallback(Coap::Message &message, IPAddress ip, uint16_t port);

void setup()
{
    // Initialize serial and wait for port to open.
    SERIAL_BEGIN(115200);
    SERIAL_PRINTLN("Booting...");
    SERIAL_PRINT("Configuring Ethernet...");
    // NOTE: You may use DHCP instead of static IP. In that case, call `Ethernet.begin(mac)` instead.
    // Also Ethernet.begin(mac, ip) will work, but it will use the default DNS, gateway and subnet.
    // Please refer to Arduino documentation.
    Ethernet.begin(mac, ip, dns, gateway, subnet);
    // Check for hardware issues
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
        SERIAL_PRINTLN("Ethernet shield/hardware not found. Check connections!");
        return;
    }
    SERIAL_PRINTLN("OK");

    coapNode.serve("time", timeCallback); // Serve the "observe" path using the timeCallback.

    // Start coap server.
    coapNode.start();

    SERIAL_PRINT("Server listening on ");
    SERIAL_PRINT(Ethernet.localIP());
    SERIAL_PRINT(":");
    SERIAL_PRINTLN(coapNode.getPort());

    SERIAL_PRINTLN("Initialisation completed!");
}

void loop()
{
    coapNode.loop(); // Process coap requests.

    sendNotification(); // Simulate a recurring notification.

    delay(1000);
}

/// Helper to build the time message.
Coap::Message getCurrentTimeMessage()
{
    const size_t payloadLength = 4; // We will encode the time as a 4-byte unsigned integer (uint32_t).
    uint8_t payload[payloadLength];
    Coap::Utils::toNetworkByteOrder(millis(), payload); // Convert to big-endian byte order, as required by CoAP specifications.
    Coap::Message msg(Coap::MessageType::NON, Coap::MessageCode::CONTENT);
    msg.addPayload((const uint8_t *)payload, payloadLength, Coap::ContentFormat::APPLICATION_OCTET_STREAM);
    return msg;
}

// Handle observe registration and deregistration requests.
void timeCallback(Coap::Message &message, IPAddress ip, uint16_t port)
{

    if (message.getCode() != Coap::MessageCode::GET)
    {
        // Only GET method is allowed for this resource.
        return;
    }

    // Build the response with resource payload.
    Coap::Message response = getCurrentTimeMessage();

    if (message.isObserveRegister())
    {
        // This is a subscription request. Add a new observer in the registry.
        Coap::Observer new_observer(ip, port, message.getToken(), message.getTokenLength());
        Coap::ErrorCode err = myObservers.add(new_observer);

        if (err == Coap::ErrorCode::OK)
        {
            // Send ACK response.
            // As per https://datatracker.ietf.org/doc/html/rfc7641#section-4.1
            // the response must also be a notification i.e. including the observe option.
            response.intoNotification(new_observer);  // Build a notification.
            response.setType(Coap::MessageType::ACK); // Set the response type to ACK.
            SERIAL_PRINT("Subscribed with token: ");
            SERIAL_WRITE(message.getToken(), message.getTokenLength());
            SERIAL_PRINTLN();
        }
        else
        {
            // Tell the client that the subscription failed.
            response.intoResponse(message, Coap::MessageCode::SERVICE_UNAVAILABLE);
            SERIAL_PRINTLN("Observer could not be added!");
        }
    }
    else if (message.isObserveDeregister())
    {
        Coap::ErrorCode err = myObservers.remove(ip, port, message.getToken(), message.getTokenLength());
        if (err == Coap::ErrorCode::OK)
        {
            response.intoResponse(message, Coap::MessageCode::VALID);
            SERIAL_PRINTLN("Unsubscribed!");
        }
        else
        {
            // Tell the client that the un-subscription failed.
            response.intoResponse(message, Coap::MessageCode::SERVICE_UNAVAILABLE);
            SERIAL_PRINTLN("Observer could not be removed!");
        }
    }
    else
    {
        // This is a normal GET request without observe option. Just send the response.
        response.intoResponse(message, Coap::MessageCode::CONTENT);
        SERIAL_PRINTLN("Received non-observe GET request!");
    }

    // Send the response.
    if (coapNode.sendMessage(response, ip, port) != Coap::ErrorCode::OK)
    {
        SERIAL_PRINTLN("Failed to send response!");
    }
}

void sendNotification()
{
    if (myObservers.countActive() == 0)
    {
        // No active observers, no need to send notifications.
        return;
    }

    Coap::Message time_message = getCurrentTimeMessage();

    for (size_t i = 0; i < myObservers.length(); i++)
    {
        if (myObservers[i].isActive())
        {
            // Build a notification message for this observer, using the original time message as template.
            Coap::Message notification = time_message;
            Coap::ErrorCode err = notification.intoNotification(myObservers[i]);
            if (err == Coap::ErrorCode::OK)
            {
                Serial.print("Sending notification to ");
                Serial.print(myObservers[i].getIp());
                Serial.print(":");
                Serial.println(myObservers[i].getPort());
                err = coapNode.sendMessage(notification, myObservers[i].getIp(), myObservers[i].getPort());
                if (err == Coap::ErrorCode::OK)
                {
                    SERIAL_PRINT("Notification sent to observer #");
                    SERIAL_PRINTLN(i);
                }
                else
                {
                    SERIAL_PRINT("Failed send to observer #");
                    SERIAL_PRINTLN(i);
                }
            }
            else
            {
                SERIAL_PRINT("Failed to build notification for observer #");
                SERIAL_PRINT(i);
                SERIAL_PRINT(" error ");
                SERIAL_PRINTLN((int8_t)err);
            }
        }
    }
}
