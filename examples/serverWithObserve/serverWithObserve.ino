/*
 * A CoAP server with Observe functionality.
 *
 * Author: Pasquale Lafiosca (2026)
 *
 * # Testing with libcoap
 * To test this example with the coap-client tool from libcoap:
 * ```
 * coap-client-notls -p 5683 -T aaaa -v 9 -m get -s 60 coap://192.168.0.1/observe
 * ```
 *
 * Note that the option `-s 60` will set the CoAP observe register option and keep the
 * command running to receive notifications for the given amount of seconds.
 *
 * In order to avoid duplicate observers:
 * - `-p 5683` sets a fixed port.
 * - `-T aaaa` sets a fixed starting token (libcoap will use the next one).
 *
 * Multiple subscriptions with equal IP, port, path and token should result in
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
#define SERIAL_PRINT(x) Serial.print(x)
#define SERIAL_PRINT_HEX(x) Serial.print(x, HEX)
#define SERIAL_PRINTLN(x) Serial.println(x)
#define SERIAL_WRITE(x) Serial.write(x)
#define SERIAL_WRITE_LEN(x, y) Serial.write(x, y)
#else
#define SERIAL_BEGIN(baud)
#define SERIAL_WHILE_WAIT
#define SERIAL_PRINT(x)
#define SERIAL_PRINT_HEX(x)
#define SERIAL_PRINTLN(x)
#define SERIAL_WRITE(x)
#define SERIAL_WRITE_LEN(x, y)
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

// Declaration of our observe callback.
void observeCallback(Coap::Message &message, IPAddress ip, uint16_t port);

void setup()
{
    // Initialize serial and wait for port to open.
    SERIAL_BEGIN(115200);
    SERIAL_WHILE_WAIT;
    SERIAL_PRINTLN("Booting...");
    SERIAL_PRINT("Configuring Ethernet...");
    Ethernet.begin(mac, ip);
    // Check for hardware issues
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
        SERIAL_PRINTLN("Ethernet shield/hardware not found. Check connections!");
        return;
    }
    SERIAL_PRINTLN("OK");

    coapNode.serve("observe", observeCallback); // Serve the "observe" path using the observeCallback.

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

// Handle observe registration and deregistration requests.
void observeCallback(Coap::Message &message, IPAddress ip, uint16_t port)
{

    if (message.isObserveRegister())
    {
        // This is a subscription request. Add a new observer in the registry.
        Coap::ErrorCode err = myObservers.add(ip, port, message.getToken(), message.getTokenLength());
        Coap::Message response;
        if (err == Coap::ErrorCode::OK)
        {
            // Send ACK response.
            message.buildResponse(Coap::MessageCode::VALID, response);
            coapNode.sendMessage(response, ip, port);
            SERIAL_PRINT("Subscribed with token: ");
            SERIAL_WRITE_LEN(message.getToken(), message.getTokenLength());
            SERIAL_PRINTLN();
        }
        else
        {
            // Tell the client that the subscription failed.
            message.buildResponse(Coap::MessageCode::SERVICE_UNAVAILABLE, response);
            coapNode.sendMessage(response, ip, port);
            SERIAL_PRINTLN("Observer could not be added!");
        }
    }
    else if (message.isObserveDeregister())
    {
        Coap::ErrorCode err = myObservers.remove(ip, port, message.getToken(), message.getTokenLength());
        Coap::Message response;
        if (err == Coap::ErrorCode::OK)
        {
            message.buildResponse(Coap::MessageCode::VALID, response);
            coapNode.sendMessage(response, ip, port);
            SERIAL_PRINTLN("Unsubscribed!");
        }
        else
        {
            // Tell the client that the subscription failed.
            message.buildResponse(Coap::MessageCode::SERVICE_UNAVAILABLE, response);
            coapNode.sendMessage(response, ip, port);
            SERIAL_PRINTLN("Observer could not be removed!");
        }
    }
    else
    {
        // Not a valid observe request. Ignore.
        SERIAL_PRINTLN("Missing/invalid observe value.");
    }
}

// Demo notification for the "observe" path.
void sendNotification()
{
    char payload[] = "The answer is 42";
    size_t payloadLength = strlen(payload);

    Coap::Message msg(Coap::MessageType::NON, Coap::MessageCode::CONTENT);
    msg.addPath("observe"); // Path must match the one the observer subscribed to!
    msg.addPayload((const uint8_t *)payload, payloadLength, Coap::ContentFormat::TEXT_PLAIN);

    for (size_t i = 0; i < myObservers.length(); i++)
    {
        if (myObservers[i].isActive())
        {
            // Build a notification message for this observer, using the original message as template.
            Coap::Message notification;
            Coap::ErrorCode err = msg.buildNotification(myObservers[i], notification);
            if (err == Coap::ErrorCode::OK)
            {
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
