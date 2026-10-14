#pragma once

#include "Types.h"
#include <vector>
#include <string>
#include <functional>

namespace TouchpadTracker {

class DeviceEnumerator {
public:
    DeviceEnumerator();
    ~DeviceEnumerator();
    
    // Enumerate all HID touch devices
    std::vector<TouchpadInfo> EnumerateTouchDevices();
    
    // Find the best touchpad device automatically
    TouchpadInfo FindBestTouchpad();
    
    // Print device information to console
    static void PrintDeviceInfo(const TouchpadInfo& info);
    
    // Get human-readable device list
    std::vector<std::string> GetDeviceList();

private:
    void GetDeviceCapabilities(const std::wstring& devicePath, TouchpadInfo& info);
    bool IsPrecisionTouchpad(const TouchpadInfo& info);
    void EnumerateViaRawInput(std::vector<TouchpadInfo>& devices);
};

} // namespace TouchpadTracker
