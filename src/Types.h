#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <array>

namespace TouchpadTracker {

// High-resolution clock for timestamps
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = std::chrono::duration<double>;

// Contact state enumeration
enum class ContactState {
    None,
    Down,       // Finger just touched
    Move,       // Finger moving
    Up          // Finger just lifted
};

// Gesture types
enum class GestureType {
    None,
    // Single finger
    Tap,
    DoubleTap,
    LongPress,
    Drag,
    Swipe,
    Flick,
    // Two finger
    TwoFingerTap,
    PinchIn,
    PinchOut,
    Rotation,
    TwoFingerScroll,
    TwoFingerScrollHorizontal,
    // Three finger
    ThreeFingerSwipeLeft,
    ThreeFingerSwipeRight,
    ThreeFingerSwipeUp,
    ThreeFingerSwipeDown,
    ThreeFingerTap
};

// Direction for gestures
enum class Direction {
    None,
    Up,
    Down,
    Left,
    Right,
    UpLeft,
    UpRight,
    DownLeft,
    DownRight
};

// Single touch contact point
struct TouchContact {
    uint32_t id = 0;              // Unique finger tracking ID
    float x = 0.0f;               // X position (0-1 normalized or raw)
    float y = 0.0f;               // Y position (0-1 normalized or raw)
    float rawX = 0.0f;            // Raw X coordinate from device
    float rawY = 0.0f;            // Raw Y coordinate from device
    float pressure = 0.0f;        // Contact pressure/strength (0-1)
    float width = 0.0f;           // Contact width if available
    float height = 0.0f;          // Contact height if available
    ContactState state = ContactState::None;
    TimePoint timestamp;
    TimePoint lastSeen;           // When this contact was last updated
    bool isValid = false;
    
    // For tracking motion
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float deltaX = 0.0f;
    float deltaY = 0.0f;
};

// Touch frame containing all current contacts
struct TouchFrame {
    std::array<TouchContact, 10> contacts;  // Support up to 10 fingers
    uint32_t activeCount = 0;
    TimePoint timestamp;
    uint32_t frameNumber = 0;
    bool hasClick = false;                  // Physical click detected
    bool hasRightClick = false;             // Right click zone or two-finger click
};

// Touchpad device information
struct TouchpadInfo {
    std::wstring devicePath;
    std::wstring deviceName;
    std::wstring manufacturer;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    uint16_t usagePage = 0;
    uint16_t usage = 0;
    
    // Coordinate bounds
    int32_t minX = 0;
    int32_t maxX = 0;
    int32_t minY = 0;
    int32_t maxY = 0;
    
    // Pressure bounds
    int32_t minPressure = 0;
    int32_t maxPressure = 0;
    
    uint32_t maxContacts = 0;
    bool isPrecisionTouchpad = false;
    bool supportsClick = false;
    bool supportsPressure = false;
};

// Gesture event
struct GestureEvent {
    GestureType type = GestureType::None;
    uint32_t fingerCount = 0;
    Direction direction = Direction::None;
    float velocity = 0.0f;
    float magnitude = 0.0f;       // For pinch/rotation
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    float centerX = 0.0f;
    float centerY = 0.0f;
    float rotation = 0.0f;        // Rotation angle in degrees
    TimePoint timestamp;
    Duration duration;
    bool isActive = false;        // Gesture still in progress
    bool isComplete = false;      // Gesture finished
};

// Raw input log entry
struct RawInputLogEntry {
    TimePoint timestamp;
    uint32_t fingerId;
    float x;
    float y;
    float pressure;
    ContactState state;
};

// Gesture log entry
struct GestureLogEntry {
    TimePoint timestamp;
    GestureType type;
    uint32_t fingerCount;
    Direction direction;
    float velocity;
    float centerX;
    float centerY;
};

// Helper functions
inline std::string GestureTypeToString(GestureType type) {
    switch (type) {
        case GestureType::None: return "None";
        case GestureType::Tap: return "Tap";
        case GestureType::DoubleTap: return "Double Tap";
        case GestureType::LongPress: return "Long Press";
        case GestureType::Drag: return "Drag";
        case GestureType::Swipe: return "Swipe";
        case GestureType::Flick: return "Flick";
        case GestureType::TwoFingerTap: return "Two-Finger Tap";
        case GestureType::PinchIn: return "Pinch In";
        case GestureType::PinchOut: return "Pinch Out";
        case GestureType::Rotation: return "Rotation";
        case GestureType::TwoFingerScroll: return "Two-Finger Scroll";
        case GestureType::TwoFingerScrollHorizontal: return "Two-Finger Scroll (H)";
        case GestureType::ThreeFingerSwipeLeft: return "3-Finger Swipe Left";
        case GestureType::ThreeFingerSwipeRight: return "3-Finger Swipe Right";
        case GestureType::ThreeFingerSwipeUp: return "3-Finger Swipe Up";
        case GestureType::ThreeFingerSwipeDown: return "3-Finger Swipe Down";
        case GestureType::ThreeFingerTap: return "3-Finger Tap";
        default: return "Unknown";
    }
}

inline std::string DirectionToString(Direction dir) {
    switch (dir) {
        case Direction::None: return "None";
        case Direction::Up: return "Up";
        case Direction::Down: return "Down";
        case Direction::Left: return "Left";
        case Direction::Right: return "Right";
        case Direction::UpLeft: return "Up-Left";
        case Direction::UpRight: return "Up-Right";
        case Direction::DownLeft: return "Down-Left";
        case Direction::DownRight: return "Down-Right";
        default: return "Unknown";
    }
}

inline std::string ContactStateToString(ContactState state) {
    switch (state) {
        case ContactState::None: return "None";
        case ContactState::Down: return "Down";
        case ContactState::Move: return "Move";
        case ContactState::Up: return "Up";
        default: return "Unknown";
    }
}

} // namespace TouchpadTracker
