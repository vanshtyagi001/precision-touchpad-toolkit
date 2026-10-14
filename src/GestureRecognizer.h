#pragma once

#include "Types.h"
#include <vector>
#include <deque>
#include <functional>
#include <mutex>
#include <array>

namespace TouchpadTracker {

// Gesture event callback
using GestureCallback = std::function<void(const GestureEvent&)>;

// Configuration for gesture recognition
struct GestureConfig {
    // Timing thresholds (in seconds)
    float tapMaxDuration = 0.25f;           // Max duration for a tap
    float doubleTapMaxInterval = 0.35f;     // Max time between taps for double-tap
    float longPressMinDuration = 0.5f;      // Min duration for long press
    
    // Distance thresholds (normalized 0-1)
    float tapMaxMovement = 0.02f;           // Max movement to still count as tap
    float flickMinVelocity = 1.5f;          // Min velocity for flick
    float swipeMinDistance = 0.1f;          // Min distance for swipe
    float pinchMinDistance = 0.05f;         // Min distance change for pinch
    float rotationMinAngle = 15.0f;         // Min rotation angle (degrees)
    float scrollThreshold = 0.01f;          // Min movement for scroll
    
    // Multi-finger thresholds
    float multiFingerSyncTime = 0.1f;       // Max time between fingers touching for multi-finger gesture
};

class GestureRecognizer {
public:
    GestureRecognizer();
    ~GestureRecognizer();
    
    // Process a touch frame
    void ProcessFrame(const TouchFrame& frame);
    
    // Get current/last gesture
    GestureEvent GetCurrentGesture() const;
    std::vector<GestureEvent> GetRecentGestures(size_t count = 10) const;
    
    // Configuration
    void SetConfig(const GestureConfig& config) { m_config = config; }
    GestureConfig GetConfig() const { return m_config; }
    
    // Callbacks
    void SetGestureCallback(GestureCallback callback);
    
    // Reset state
    void Reset();
    
private:
    // Gesture detection methods
    void DetectSingleFingerGestures();
    void DetectTwoFingerGestures();
    void DetectThreeFingerGestures();
    
    void UpdateGestureState();
    void FinalizeGesture();
    
    Direction CalculateDirection(float dx, float dy);
    float CalculateDistance(float x1, float y1, float x2, float y2);
    float CalculateAngle(float x1, float y1, float x2, float y2);
    
    void EmitGesture(const GestureEvent& gesture);
    
    // Configuration
    GestureConfig m_config;
    
    // Current state
    TouchFrame m_currentFrame;
    TouchFrame m_previousFrame;
    GestureEvent m_currentGesture;
    std::deque<GestureEvent> m_gestureHistory;
    mutable std::mutex m_mutex;
    
    // Tracking data for gesture recognition
    struct FingerTrack {
        bool active = false;
        uint32_t id = 0;
        float startX = 0, startY = 0;
        float currentX = 0, currentY = 0;
        float totalDeltaX = 0, totalDeltaY = 0;
        TimePoint startTime;
        TimePoint lastUpdateTime;
        float maxVelocity = 0;
        std::deque<std::pair<float, float>> positionHistory;
    };
    
    std::array<FingerTrack, 10> m_fingerTracks;
    uint32_t m_peakFingerCount = 0;
    TimePoint m_gestureStartTime;
    bool m_gestureInProgress = false;
    
    // For tap detection
    TimePoint m_lastTapTime;
    float m_lastTapX = 0, m_lastTapY = 0;
    bool m_waitingForDoubleTap = false;
    
    // For two-finger gestures
    float m_initialPinchDistance = 0;
    float m_initialRotationAngle = 0;
    float m_accumulatedRotation = 0;
    float m_accumulatedPinch = 0;
    
    // Callback
    GestureCallback m_gestureCallback;
};

} // namespace TouchpadTracker
