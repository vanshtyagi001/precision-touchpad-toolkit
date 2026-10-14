#pragma once

#include "Config.h"
#include <Windows.h>
#include <functional>
#include <mutex>
#include <atomic>

namespace TouchpadShortcuts {

// Callback for zone tap events
using ZoneTapCallback = std::function<void(Zone)>;

class TouchpadZones {
public:
    TouchpadZones();
    ~TouchpadZones();
    
    // Initialize and find touchpad
    bool Initialize();
    
    // Start/stop monitoring
    bool Start(HWND messageWindow);
    void Stop();
    bool IsRunning() const { return m_running; }
    
    // Process raw input message
    bool ProcessRawInput(LPARAM lParam);
    
    // Set callback for zone taps
    void SetTapCallback(ZoneTapCallback callback);
    
    // Get device info
    const TouchpadInfo& GetDeviceInfo() const { return m_deviceInfo; }
    
    // Check if currently scrolling (for mouse hook)
    bool IsScrolling() const { return m_isScrolling && m_scrollStarted; }
    
private:
    bool RegisterForRawInput();
    void UnregisterRawInput();
    void ParseHidReport(const uint8_t* data, size_t size);
    void HandleTouchDown(float x, float y);
    void HandleTouchMove(float x, float y);
    void HandleTouchUp(float x, float y);
    void OnZoneTap(Zone zone);
    void SendScrollWheel(int delta);
    void InstallMouseHook();
    void RemoveMouseHook();
    
    TouchpadInfo m_deviceInfo;
    TouchState m_touchState;
    
    // Scroll tracking
    bool m_isScrolling = false;
    float m_scrollAccumulator = 0.0f;
    float m_lastScrollY = 0.0f;
    float m_scrollVelocity = 0.0f;
    TimePoint m_lastScrollTime;
    bool m_scrollStarted = false;  // True after first movement in scroll zone
    
    // Auto-scroll at edges
    bool m_autoScrollActive = false;
    int m_autoScrollDirection = 0;  // -1 = up, 0 = none, 1 = down
    UINT_PTR m_autoScrollTimer = 0;
    static void CALLBACK AutoScrollTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);
    void StartAutoScroll(int direction);
    void StopAutoScroll();
    void DoAutoScroll();
    
    // Mouse hook for blocking cursor during scroll
    HHOOK m_mouseHook = nullptr;
    POINT m_scrollStartCursor = {0, 0};  // Cursor position when scroll started
    
    HWND m_hwnd = nullptr;
    void* m_preparsedData = nullptr;
    size_t m_preparsedDataSize = 0;
    
    std::atomic<bool> m_running{false};
    TimePoint m_lastTapTime;
    
    ZoneTapCallback m_tapCallback;
    std::mutex m_callbackMutex;
};

// Global pointer for mouse hook callback
extern TouchpadZones* g_touchpadZonesInstance;

} // namespace TouchpadShortcuts
