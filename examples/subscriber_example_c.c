// examples/subscriber_example_c.c - C API subscriber example
#include <sparkplug/sparkplug_c.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

static volatile int running = 1;

void signal_handler(int signum) {
    (void)signum;
    running = 0;
}

// Callback function that gets called for each received message
void on_message(const char *topic, const uint8_t *payload_data,
                size_t payload_len, void *user_data) {
    (void)user_data;

    printf("\n=== Message Received ===\n");
    printf("Topic: %s\n", topic);
    printf("Payload size: %zu bytes\n", payload_len);

    // For a real application, you would:
    // 1. Parse the protobuf payload
    // 2. Extract metrics
    // 3. Process the data

    // For this example, we'll just show we received it
    printf("========================\n");
}

int main(void) {
    printf("Sparkplug B C Subscriber Example\n");
    printf("=================================\n\n");

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create subscriber with callback
    sparkplug_subscriber_t *sub = sparkplug_subscriber_create(
        "tcp://localhost:1883",
        "c_subscriber_example",
        "Energy",
        on_message,
        NULL  // user_data (optional)
    );

    if (!sub) {
        fprintf(stderr, "Failed to create subscriber\n");
        return 1;
    }

    printf("✓ Subscriber created\n");

    // Connect to broker
    if (sparkplug_subscriber_connect(sub) != 0) {
        fprintf(stderr, "Failed to connect to broker\n");
        sparkplug_subscriber_destroy(sub);
        return 1;
    }

    printf("✓ Connected to broker\n");

    // Subscribe to all messages in the Energy group
    if (sparkplug_subscriber_subscribe_all(sub) != 0) {
        fprintf(stderr, "Failed to subscribe\n");
        sparkplug_subscriber_disconnect(sub);
        sparkplug_subscriber_destroy(sub);
        return 1;
    }

    printf("✓ Subscribed to spBv1.0/Energy/#\n");
    printf("\nListening for messages (Ctrl+C to stop)...\n");

    // Keep running and processing messages
    while (running) {
        sleep(1);
    }

    printf("\n\n⏹ Shutting down...\n");

    // Disconnect
    if (sparkplug_subscriber_disconnect(sub) == 0) {
        printf("✓ Disconnected from broker\n");
    }

    sparkplug_subscriber_destroy(sub);

    printf("✓ Subscriber destroyed\n");
    printf("\nC subscriber example complete!\n");

    return 0;
}
