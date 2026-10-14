#pragma once

#include "Types.h"
#include "DeviceEnumerator.h"

#include <Windows.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <queue>
#include <memory>

namespace TouchpadTracker {

// Callback types
using TouchFrameCallback = std::function<void(const TouchFrame&)>;
using RawDataCallback = std::function<void(const uint8_t*, size_t)>;

class TouchpadInput {
public:
    TouchpadInput();
    ~TouchpadInput();
    
    // Initialize with a specific device or auto-detect
    bool Initialize(const TouchpadInfo& device);
    bool InitializeAutoDetect();
    
    // Start/stop input capture
    bool StartCapture();
    void StopCapture();
    bool IsCapturing() const { return m_isCapturing; }
    
    // Get current touch state
    TouchFrame GetCurrentFrame() const;
    TouchpadInfo GetDeviceInfo() const { return m_deviceInfo; }
    
    // Callbacks
    void SetTouchFrameCallback(TouchFrameCallback callback);
    void SetRawDataCallback(RawDataCallback callback);
    
    // Statistics
    uint64_t GetFrameCount() const { return m_frameCount; }
    double GetInputRate() const { return m_inputRate; }
    
    // For window procedure (Raw Input)
    bool ProcessRawInput(LPARAM lParam);
    
    // Window handle for raw input registration
    void SetWindowHandle(HWND hwnd) { m_hwnd = hwnd; }
    
private:
    bool RegisterForRawInput();
    void UnregisterRawInput();
    void ParseHidReport(const uint8_t* data, size_t size);
    void UpdateContactState(TouchContact& contact, bool isDown, float x, float y, float pressure);
    
    // Device info
    TouchpadInfo m_deviceInfo;
    HANDLE m_deviceHandle = INVALID_HANDLE_VALUE;
    HWND m_hwnd = nullptr;
    
    // Preparsed data for HID parsing
    void* m_preparsedData = nullptr;
    size_t m_preparsedDataSize = 0;
    
    // Current state
    TouchFrame m_currentFrame;
    mutable std::mutex m_frameMutex;
    
    // Previous contacts for state tracking
    std::array<TouchContact, 10> m_previousContacts;
    
    // Active contacts - persistent tracking across reports
    std::array<TouchContact, 10> m_activeContacts;
    
    // Capture state
    std::atomic<bool> m_isCapturing{false};
    std::atomic<bool> m_initialized{false};
    std::atomic<uint64_t> m_frameCount{0};
    std::atomic<double> m_inputRate{0.0};
    TimePoint m_lastRateCalc;
    uint64_t m_framesForRate = 0;
    
    // Callbacks
    TouchFrameCallback m_touchFrameCallback;
    RawDataCallback m_rawDataCallback;
    std::mutex m_callbackMutex;
};

} // namespace TouchpadTracker
