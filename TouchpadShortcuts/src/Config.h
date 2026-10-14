#pragma once

#include <cstdint>
#include <string>
#include <chrono>

namespace TouchpadShortcuts {

// High-resolution clock for timestamps
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

// Zone definitions - corner areas of the touchpad
enum class Zone {
    None,
    TopLeft,      // Brightness Down
    TopRight,     // Brightness Up
    BottomLeft,   // Volume Down
    BottomRight,  // Volume Up
    ScrollArea    // Right edge scroll zone
};

// Zone boundaries (normalized 0.0 - 1.0)
struct ZoneBounds {
    float minX, maxX;
    float minY, maxY;
};

// Predefined zone boundaries - larger zones for easier tapping
// Corners are now 15% x 15% of the touchpad
constexpr ZoneBounds ZONE_TOP_LEFT     = { 0.00f, 0.15f, 0.00f, 0.15f };
constexpr ZoneBounds ZONE_TOP_RIGHT    = { 0.85f, 1.00f, 0.00f, 0.15f };
constexpr ZoneBounds ZONE_BOTTOM_LEFT  = { 0.00f, 0.15f, 0.85f, 1.00f };
constexpr ZoneBounds ZONE_BOTTOM_RIGHT = { 0.85f, 1.00f, 0.85f, 1.00f };

// Scroll zone - right edge, excluding corners (from 15% to 85% height)
constexpr ZoneBounds ZONE_SCROLL       = { 0.90f, 1.00f, 0.15f, 0.85f };

// Scroll parameters
constexpr float SCROLL_SENSITIVITY = 8.0f;       // Scroll multiplier (higher = faster)
constexpr float SCROLL_THRESHOLD = 0.005f;       // Minimum movement to trigger scroll
constexpr float SCROLL_SMOOTHING = 0.7f;         // Velocity smoothing (0-1, higher = smoother)
constexpr int SCROLL_WHEEL_DELTA = 120;          // Standard wheel delta (WHEEL_DELTA)

// Auto-scroll at edges
constexpr float AUTO_SCROLL_EDGE_THRESHOLD = 0.03f;  // How close to edge to trigger (3%)
constexpr float AUTO_SCROLL_SPEED = 4.0f;            // Auto-scroll speed multiplier
constexpr int AUTO_SCROLL_INTERVAL_MS = 30;          // Auto-scroll tick interval (ms)

// Tap detection parameters
constexpr float TAP_MAX_DURATION_MS = 300.0f;    // Max duration for a tap (ms)
constexpr float TAP_MAX_MOVEMENT = 0.02f;        // Max movement allowed during tap
constexpr float TAP_COOLDOWN_MS = 200.0f;        // Cooldown between taps

// Touchpad info
struct TouchpadInfo {
    std::wstring devicePath;
    int32_t minX = 0, maxX = 4096;
    int32_t minY = 0, maxY = 2400;
    bool isValid = false;
};

// Touch state
struct TouchState {
    bool isDown = false;
    float x = 0.0f;
    float y = 0.0f;
    float startX = 0.0f;
    float startY = 0.0f;
    TimePoint downTime;
    Zone startZone = Zone::None;
};

// Helper to check if point is in zone
inline bool IsInZone(float x, float y, const ZoneBounds& zone) {
    return x >= zone.minX && x <= zone.maxX && 
           y >= zone.minY && y <= zone.maxY;
}

// Get zone from coordinates
inline Zone GetZone(float x, float y) {
    // Check corners first (they have priority)
    if (IsInZone(x, y, ZONE_TOP_LEFT))     return Zone::TopLeft;
    if (IsInZone(x, y, ZONE_TOP_RIGHT))    return Zone::TopRight;
    if (IsInZone(x, y, ZONE_BOTTOM_LEFT))  return Zone::BottomLeft;
    if (IsInZone(x, y, ZONE_BOTTOM_RIGHT)) return Zone::BottomRight;
    // Then check scroll zone
    if (IsInZone(x, y, ZONE_SCROLL))       return Zone::ScrollArea;
    return Zone::None;
}

inline const char* ZoneToString(Zone zone) {
    switch (zone) {
        case Zone::TopLeft:     return "Top-Left (Bright-)";
        case Zone::TopRight:    return "Top-Right (Bright+)";
        case Zone::BottomLeft:  return "Bottom-Left (Vol-)";
        case Zone::BottomRight: return "Bottom-Right (Vol+)";
        case Zone::ScrollArea:  return "Scroll Zone";
        default:                return "None";
    }
}

} // namespace TouchpadShortcuts
