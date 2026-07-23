#pragma once

// Hardcoded credentials for the permanent access point created by the
// hardware ESP32. These values must exactly match the hardware project.
constexpr char AM3_HARDWARE_AP_SSID[] = "AM3-AAA001";
constexpr char AM3_HARDWARE_AP_PASSWORD[] = "bitahomes";
static_assert(sizeof(AM3_HARDWARE_AP_PASSWORD) - 1 >= 8,
              "ESP32 SoftAP password must contain at least 8 characters");
constexpr char AM3_DISPLAY_LINK_TOKEN[] = "AM3-KRC-LINK-2026";

const IPAddress AM3_HARDWARE_HOST_IP(192, 168, 4, 1);
const IPAddress AM3_DISPLAY_LOCAL_IP(192, 168, 4, 50);
const IPAddress AM3_DISPLAY_GATEWAY(192, 168, 4, 1);
const IPAddress AM3_DISPLAY_SUBNET(255, 255, 255, 0);
const IPAddress AM3_DISPLAY_DNS(192, 168, 4, 1);

constexpr uint16_t AM3_DISPLAY_TCP_PORT = 5050;
constexpr uint32_t AM3_WIFI_RETRY_MS = 5000;
constexpr uint32_t AM3_TCP_RETRY_MS = 2000;
constexpr uint32_t AM3_TCP_CONNECT_TIMEOUT_MS = 3000;
constexpr uint32_t AM3_HEARTBEAT_INTERVAL_MS = 2000;
constexpr uint32_t AM3_STATE_TIMEOUT_MS = 12000;
constexpr uint32_t AM3_DISPLAY_STATUS_INTERVAL_MS = 3000;

constexpr size_t AM3_DISPLAY_RX_BUFFER_SIZE = 1280;
constexpr size_t AM3_DISPLAY_TX_BUFFER_SIZE = 384;

// Set to 1 while commissioning. It prints Wi-Fi/TCP status and received JSON
// to the display Serial Monitor.
#define AM3_LINK_DEBUG 1
