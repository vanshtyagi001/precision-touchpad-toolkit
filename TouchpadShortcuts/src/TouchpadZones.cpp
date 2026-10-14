#include "TouchpadZones.h"
#include <hidsdi.h>
#include <hidpi.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "hid.lib")

namespace TouchpadShortcuts {

// Global instance pointer for mouse hook
TouchpadZones* g_touchpadZonesInstance = nullptr;

// Low-level mouse hook callback
static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_touchpadZonesInstance && g_touchpadZonesInstance->IsScrolling()) {
        // Block mouse movement while scrolling
        if (wParam == WM_MOUSEMOVE) {
            // Block the mouse move and keep cursor at scroll start position
            return 1;  // Non-zero = block the message
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// HID Usage definitions
constexpr USHORT USAGE_PAGE_DIGITIZER = 0x0D;
constexpr USHORT USAGE_PAGE_GENERIC = 0x01;
constexpr USHORT USAGE_DIGITIZER_TOUCH_PAD = 0x05;
constexpr USHORT USAGE_GENERIC_X = 0x30;
constexpr USHORT USAGE_GENERIC_Y = 0x31;

TouchpadZones::TouchpadZones() {
    m_lastTapTime = Clock::now() - std::chrono::milliseconds(1000);
    g_touchpadZonesInstance = this;
}

TouchpadZones::~TouchpadZones() {
    Stop();
    StopAutoScroll();
    RemoveMouseHook();
    g_touchpadZonesInstance = nullptr;
    
    if (m_preparsedData && m_preparsedDataSize > 0) {
        delete[] static_cast<uint8_t*>(m_preparsedData);
    }
}

bool TouchpadZones::Initialize() {
    // Enumerate raw input devices to find touchpad
    UINT numDevices = 0;
    if (GetRawInputDeviceList(nullptr, &numDevices, sizeof(RAWINPUTDEVICELIST)) != 0) {
        return false;
    }
    
    if (numDevices == 0) return false;
    
    auto* deviceList = new RAWINPUTDEVICELIST[numDevices];
    if (GetRawInputDeviceList(deviceList, &numDevices, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) {
        delete[] deviceList;
        return false;
    }
    
    bool found = false;
    
    for (UINT i = 0; i < numDevices && !found; i++) {
        if (deviceList[i].dwType != RIM_TYPEHID) continue;
        
        // Get device info
        RID_DEVICE_INFO deviceInfo = {};
        deviceInfo.cbSize = sizeof(RID_DEVICE_INFO);
        UINT infoSize = sizeof(RID_DEVICE_INFO);
        
        if (GetRawInputDeviceInfo(deviceList[i].hDevice, RIDI_DEVICEINFO, &deviceInfo, &infoSize) == (UINT)-1) {
            continue;
        }
        
        // Check for touchpad (digitizer usage page, touchpad usage)
        if (deviceInfo.hid.usUsagePage == USAGE_PAGE_DIGITIZER &&
            deviceInfo.hid.usUsage == USAGE_DIGITIZER_TOUCH_PAD) {
            
            // Get device name
            UINT nameSize = 0;
            GetRawInputDeviceInfo(deviceList[i].hDevice, RIDI_DEVICENAME, nullptr, &nameSize);
            
            if (nameSize > 0) {
                auto* nameBuf = new wchar_t[nameSize];
                if (GetRawInputDeviceInfo(deviceList[i].hDevice, RIDI_DEVICENAME, nameBuf, &nameSize) > 0) {
                    m_deviceInfo.devicePath = nameBuf;
                    m_deviceInfo.isValid = true;
                    found = true;
                }
                delete[] nameBuf;
            }
        }
    }
    
    delete[] deviceList;
    return found;
}

bool TouchpadZones::RegisterForRawInput() {
    if (!m_hwnd) return false;
    
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = USAGE_PAGE_DIGITIZER;
    rid.usUsage = USAGE_DIGITIZER_TOUCH_PAD;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = m_hwnd;
    
    return RegisterRawInputDevices(&rid, 1, sizeof(rid)) != FALSE;
}

void TouchpadZones::UnregisterRawInput() {
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = USAGE_PAGE_DIGITIZER;
    rid.usUsage = USAGE_DIGITIZER_TOUCH_PAD;
    rid.dwFlags = RIDEV_REMOVE;
    rid.hwndTarget = nullptr;
    
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

bool TouchpadZones::Start(HWND messageWindow) {
    m_hwnd = messageWindow;
    
    if (!RegisterForRawInput()) {
        return false;
    }
    
    // Install low-level mouse hook to block cursor movement during scroll
    InstallMouseHook();
    
    m_running = true;
    return true;
}

void TouchpadZones::Stop() {
    if (m_running) {
        UnregisterRawInput();
        RemoveMouseHook();
        m_running = false;
    }
}

void TouchpadZones::InstallMouseHook() {
    if (!m_mouseHook) {
        m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, 
                                         GetModuleHandle(nullptr), 0);
    }
}

void TouchpadZones::RemoveMouseHook() {
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
}

void TouchpadZones::SetTapCallback(ZoneTapCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_tapCallback = callback;
}

bool TouchpadZones::ProcessRawInput(LPARAM lParam) {
    UINT size = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0) {
        return false;
    }
    
    if (size == 0) return false;
    
    std::vector<uint8_t> buffer(size);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
        return false;
    }
    
    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer.data());
    
    if (raw->header.dwType != RIM_TYPEHID) {
        return false;
    }
    
    // Get preparsed data if we don't have it
    if (!m_preparsedData) {
        UINT preparsedSize = 0;
        GetRawInputDeviceInfo(raw->header.hDevice, RIDI_PREPARSEDDATA, nullptr, &preparsedSize);
        
        if (preparsedSize > 0) {
            auto* preparsedBuffer = new uint8_t[preparsedSize];
            if (GetRawInputDeviceInfo(raw->header.hDevice, RIDI_PREPARSEDDATA, preparsedBuffer, &preparsedSize) == preparsedSize) {
                m_preparsedData = preparsedBuffer;
                m_preparsedDataSize = preparsedSize;
                
                // Get coordinate ranges
                PHIDP_PREPARSED_DATA preparsed = reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsedBuffer);
                HIDP_CAPS caps;
                if (HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS) {
                    USHORT valueCapsLength = caps.NumberInputValueCaps;
                    if (valueCapsLength > 0) {
                        auto* valueCaps = new HIDP_VALUE_CAPS[valueCapsLength];
                        if (HidP_GetValueCaps(HidP_Input, valueCaps, &valueCapsLength, preparsed) == HIDP_STATUS_SUCCESS) {
                            for (USHORT i = 0; i < valueCapsLength; i++) {
                                auto& vc = valueCaps[i];
                                if (vc.UsagePage == USAGE_PAGE_GENERIC && vc.Range.UsageMin == USAGE_GENERIC_X) {
                                    m_deviceInfo.minX = vc.LogicalMin;
                                    m_deviceInfo.maxX = vc.LogicalMax;
                                }
                                else if (vc.UsagePage == USAGE_PAGE_GENERIC && vc.Range.UsageMin == USAGE_GENERIC_Y) {
                                    m_deviceInfo.minY = vc.LogicalMin;
                                    m_deviceInfo.maxY = vc.LogicalMax;
                                }
                            }
                        }
                        delete[] valueCaps;
                    }
                }
            } else {
                delete[] preparsedBuffer;
            }
        }
    }
    
    // Parse the HID data
    const uint8_t* hidData = raw->data.hid.bRawData;
    size_t hidDataSize = raw->data.hid.dwSizeHid;
    
    if (hidDataSize > 0) {
        ParseHidReport(hidData, hidDataSize);
    }
    
    return true;
}

void TouchpadZones::ParseHidReport(const uint8_t* data, size_t size) {
    // Parse ASUS precision touchpad format: ReportID(1) ContactInfo(1) X(2) Y(2) ...
    if (size >= 6 && data[0] == 0x04) {
        uint8_t contactInfo = data[1];
        bool fingerDown = (contactInfo & 0x03) != 0;
        
        if (!fingerDown) {
            fingerDown = (contactInfo & 0x01) != 0;
        }
        
        uint16_t rawX = data[2] | (data[3] << 8);
        uint16_t rawY = data[4] | (data[5] << 8);
        
        // Normalize coordinates
        float rangeX = static_cast<float>(m_deviceInfo.maxX - m_deviceInfo.minX);
        float rangeY = static_cast<float>(m_deviceInfo.maxY - m_deviceInfo.minY);
        
        float x = rangeX > 0 ? (static_cast<float>(rawX) - m_deviceInfo.minX) / rangeX : 0.0f;
        float y = rangeY > 0 ? (static_cast<float>(rawY) - m_deviceInfo.minY) / rangeY : 0.0f;
        
        x = std::clamp(x, 0.0f, 1.0f);
        y = std::clamp(y, 0.0f, 1.0f);
        
        // Handle touch state transitions
        if (fingerDown && !m_touchState.isDown) {
            // Finger just touched down
            HandleTouchDown(x, y);
        } else if (!fingerDown && m_touchState.isDown) {
            // Finger just lifted
            HandleTouchUp(x, y);
        } else if (fingerDown && m_touchState.isDown) {
            // Finger moving - handle scroll or update position
            HandleTouchMove(x, y);
        }
    }
}

void TouchpadZones::HandleTouchDown(float x, float y) {
    m_touchState.isDown = true;
    m_touchState.x = x;
    m_touchState.y = y;
    m_touchState.startX = x;
    m_touchState.startY = y;
    m_touchState.downTime = Clock::now();
    m_touchState.startZone = GetZone(x, y);
    
    // Check if starting in scroll zone
    if (m_touchState.startZone == Zone::ScrollArea) {
        m_isScrolling = true;
        m_scrollStarted = false;  // Wait for actual movement
        m_lastScrollY = y;
        m_scrollAccumulator = 0.0f;
        m_scrollVelocity = 0.0f;
        m_lastScrollTime = Clock::now();
        // Save cursor position when entering scroll zone
        GetCursorPos(&m_scrollStartCursor);
    } else {
        m_isScrolling = false;
        m_scrollStarted = false;
    }
}

void TouchpadZones::HandleTouchMove(float x, float y) {
    float oldX = m_touchState.x;
    float oldY = m_touchState.y;
    m_touchState.x = x;
    m_touchState.y = y;
    
    // Check if finger is currently in scroll zone
    Zone currentZone = GetZone(x, y);
    bool inScrollZone = (currentZone == Zone::ScrollArea);
    
    // Handle scrolling - only if finger is still in scroll zone
    if (m_isScrolling && inScrollZone) {
        auto now = Clock::now();
        float deltaTime = std::chrono::duration<float, std::milli>(now - m_lastScrollTime).count();
        m_lastScrollTime = now;
        
        // Calculate delta (inverted: finger up = scroll up = positive delta)
        float deltaY = m_lastScrollY - y;
        m_lastScrollY = y;
        
        // Ignore very small movements (noise filtering)
        if (std::abs(deltaY) < SCROLL_THRESHOLD && !m_scrollStarted) {
            return;
        }
        
        // Mark scroll as started after first significant movement
        if (!m_scrollStarted) {
            m_scrollStarted = true;
            // Save cursor position when scroll actually starts
            GetCursorPos(&m_scrollStartCursor);
        }
        
        // Keep cursor at the saved position while scrolling
        SetCursorPos(m_scrollStartCursor.x, m_scrollStartCursor.y);
        
        // Check for auto-scroll at edges
        // Top edge of scroll zone (y close to ZONE_SCROLL.minY)
        float distFromTop = y - ZONE_SCROLL.minY;
        float distFromBottom = ZONE_SCROLL.maxY - y;
        
        if (distFromTop < AUTO_SCROLL_EDGE_THRESHOLD) {
            // At top edge - auto scroll UP (negative direction in content)
            StartAutoScroll(-1);
        } else if (distFromBottom < AUTO_SCROLL_EDGE_THRESHOLD) {
            // At bottom edge - auto scroll DOWN (positive direction in content)
            StartAutoScroll(1);
        } else {
            // Not at edge - stop auto scroll if active
            StopAutoScroll();
        }
        
        // Apply velocity smoothing for natural feel
        float instantVelocity = deltaY * SCROLL_SENSITIVITY;
        m_scrollVelocity = m_scrollVelocity * SCROLL_SMOOTHING + instantVelocity * (1.0f - SCROLL_SMOOTHING);
        
        // Accumulate scroll
        m_scrollAccumulator += m_scrollVelocity * 100.0f;
        
        // Send scroll events in small increments for smoothness
        // Use smaller threshold for more responsive scrolling
        const float scrollUnit = 30.0f;
        int scrollUnits = static_cast<int>(m_scrollAccumulator / scrollUnit);
        
        if (scrollUnits != 0) {
            // Send scroll - positive = scroll up, negative = scroll down
            int scrollAmount = scrollUnits * (SCROLL_WHEEL_DELTA / 4);  // Smaller increments
            SendScrollWheel(scrollAmount);
            m_scrollAccumulator -= scrollUnits * scrollUnit;
        }
    } else if (m_isScrolling && !inScrollZone) {
        // Finger left the scroll zone - stop scrolling
        m_isScrolling = false;
        m_scrollStarted = false;
        m_scrollAccumulator = 0.0f;
        m_scrollVelocity = 0.0f;
        StopAutoScroll();
    }
}

void TouchpadZones::HandleTouchUp(float x, float y) {
    if (!m_touchState.isDown) return;
    
    // Stop scrolling and auto-scroll
    StopAutoScroll();
    if (m_isScrolling) {
        m_isScrolling = false;
        m_scrollStarted = false;
        m_scrollAccumulator = 0.0f;
        m_scrollVelocity = 0.0f;
    }
    
    auto now = Clock::now();
    float duration = std::chrono::duration<float, std::milli>(now - m_touchState.downTime).count();
    float cooldown = std::chrono::duration<float, std::milli>(now - m_lastTapTime).count();
    
    // Calculate movement distance
    float dx = x - m_touchState.startX;
    float dy = y - m_touchState.startY;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    // Check if this qualifies as a tap in a zone (not for scroll zone)
    if (m_touchState.startZone != Zone::None &&
        m_touchState.startZone != Zone::ScrollArea &&
        duration < TAP_MAX_DURATION_MS &&
        distance < TAP_MAX_MOVEMENT &&
        cooldown > TAP_COOLDOWN_MS) {
        
        // Valid zone tap!
        Zone endZone = GetZone(x, y);
        if (endZone == m_touchState.startZone) {
            OnZoneTap(m_touchState.startZone);
            m_lastTapTime = now;
        }
    }
    
    m_touchState.isDown = false;
    m_touchState.startZone = Zone::None;
}

void TouchpadZones::SendScrollWheel(int delta) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(delta);
    SendInput(1, &input, sizeof(INPUT));
}

void TouchpadZones::OnZoneTap(Zone zone) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    if (m_tapCallback) {
        m_tapCallback(zone);
    }
}

// Auto-scroll timer callback
void CALLBACK TouchpadZones::AutoScrollTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    if (g_touchpadZonesInstance) {
        g_touchpadZonesInstance->DoAutoScroll();
    }
}

void TouchpadZones::StartAutoScroll(int direction) {
    if (m_autoScrollActive && m_autoScrollDirection == direction) {
        return;  // Already scrolling in this direction
    }
    
    StopAutoScroll();
    
    m_autoScrollDirection = direction;
    m_autoScrollActive = true;
    m_autoScrollTimer = SetTimer(m_hwnd, 1, AUTO_SCROLL_INTERVAL_MS, AutoScrollTimerProc);
}

void TouchpadZones::StopAutoScroll() {
    if (m_autoScrollTimer) {
        KillTimer(m_hwnd, m_autoScrollTimer);
        m_autoScrollTimer = 0;
    }
    m_autoScrollActive = false;
    m_autoScrollDirection = 0;
}

void TouchpadZones::DoAutoScroll() {
    if (!m_autoScrollActive || m_autoScrollDirection == 0) {
        return;
    }
    
    // Keep cursor in place during auto-scroll
    SetCursorPos(m_scrollStartCursor.x, m_scrollStartCursor.y);
    
    // Send scroll based on direction
    // direction: -1 = scroll up (finger at top), 1 = scroll down (finger at bottom)
    int scrollAmount = static_cast<int>(-m_autoScrollDirection * AUTO_SCROLL_SPEED * (SCROLL_WHEEL_DELTA / 4));
    SendScrollWheel(scrollAmount);
}

} // namespace TouchpadShortcuts
