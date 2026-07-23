#pragma once

// Permanent Wi-Fi network created by the hardware ESP32 for the display ESP32.
// These values must exactly match AM4_DISPLAY/DisplayLinkConfig.h.
constexpr char AM4_HARDWARE_AP_SSID[] = "AM4-AAA001";
constexpr char AM4_HARDWARE_AP_PASSWORD[] = "bitahomes";
static_assert(sizeof(AM4_HARDWARE_AP_PASSWORD) - 1 >= 8,
              "ESP32 SoftAP password must contain at least 8 characters");

constexpr uint16_t AM4_DISPLAY_TCP_PORT = 5050;
constexpr char AM4_DISPLAY_LINK_TOKEN[] = "AM4-KRC-LINK-2026";

// Link timing. The timeout is intentionally longer than the MQTT/TLS timeout so
// a slow broker connection cannot unnecessarily disconnect the local display.
constexpr uint32_t AM4_DISPLAY_STATE_INTERVAL_MS = 500;
constexpr uint32_t AM4_DISPLAY_CLIENT_TIMEOUT_MS = 15000;
constexpr uint32_t AM4_HOST_AP_HEALTH_CHECK_MS = 5000;
constexpr uint32_t AM4_HOST_LINK_STATUS_INTERVAL_MS = 5000;

constexpr size_t AM4_DISPLAY_RX_BUFFER_SIZE = 512;
constexpr size_t AM4_DISPLAY_TX_BUFFER_SIZE = 1280;

// Set to 1 while commissioning. It prints connection stages and transmitted
// JSON to the hardware Serial Monitor.
#define AM4_LINK_DEBUG 1
