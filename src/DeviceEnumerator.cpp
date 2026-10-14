#include "DeviceEnumerator.h"

#include <Windows.h>
#include <SetupAPI.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <devguid.h>
#include <iostream>
#include <iomanip>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace TouchpadTracker {

// HID Usage Pages and Usages for touch devices
// Use TOUCH_ prefix to avoid conflicts with Windows SDK macros
constexpr USHORT TOUCH_USAGE_PAGE_DIGITIZER = 0x0D;
constexpr USHORT TOUCH_USAGE_DIGITIZER_TOUCH_PAD = 0x05;
constexpr USHORT TOUCH_USAGE_DIGITIZER_TOUCH_SCREEN = 0x04;
constexpr USHORT TOUCH_USAGE_DIGITIZER_FINGER = 0x22;
constexpr USHORT TOUCH_USAGE_DIGITIZER_TIP_SWITCH = 0x42;
constexpr USHORT TOUCH_USAGE_DIGITIZER_CONTACT_ID = 0x51;
constexpr USHORT TOUCH_USAGE_DIGITIZER_CONTACT_COUNT = 0x54;
constexpr USHORT TOUCH_USAGE_DIGITIZER_TIP_PRESSURE = 0x30;

constexpr USHORT TOUCH_USAGE_PAGE_GENERIC = 0x01;
constexpr USHORT TOUCH_USAGE_GENERIC_X = 0x30;
constexpr USHORT TOUCH_USAGE_GENERIC_Y = 0x31;

DeviceEnumerator::DeviceEnumerator() {
}

DeviceEnumerator::~DeviceEnumerator() {
}

std::vector<TouchpadInfo> DeviceEnumerator::EnumerateTouchDevices() {
    std::vector<TouchpadInfo> devices;
    
    // First, try Raw Input API - this sees devices that SetupAPI can't
    EnumerateViaRawInput(devices);
    
    // If we found devices via Raw Input, use those
    if (!devices.empty()) {
        return devices;
    }
    
    // Fallback to SetupAPI enumeration
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
        &hidGuid,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to get device info set. Error: " << GetLastError() << std::endl;
        return devices;
    }
    
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &hidGuid, i, &deviceInterfaceData); i++) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, nullptr, 0, &requiredSize, nullptr);
        
        if (requiredSize == 0) continue;
        
        auto* detailData = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(requiredSize));
        if (!detailData) continue;
        
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        
        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        
        if (SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, detailData, requiredSize, nullptr, &devInfoData)) {
            std::wstring devicePath = detailData->DevicePath;
            
            HANDLE deviceHandle = CreateFile(
                devicePath.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr
            );
            
            if (deviceHandle != INVALID_HANDLE_VALUE) {
                HIDD_ATTRIBUTES attributes;
                attributes.Size = sizeof(HIDD_ATTRIBUTES);
                
                if (HidD_GetAttributes(deviceHandle, &attributes)) {
                    PHIDP_PREPARSED_DATA preparsedData = nullptr;
                    
                    if (HidD_GetPreparsedData(deviceHandle, &preparsedData)) {
                        HIDP_CAPS caps;
                        
                        if (HidP_GetCaps(preparsedData, &caps) == HIDP_STATUS_SUCCESS) {
                            // Check if this is a touch device (digitizer)
                            if (caps.UsagePage == TOUCH_USAGE_PAGE_DIGITIZER &&
                                (caps.Usage == TOUCH_USAGE_DIGITIZER_TOUCH_PAD || 
                                 caps.Usage == TOUCH_USAGE_DIGITIZER_TOUCH_SCREEN ||
                                 caps.Usage == TOUCH_USAGE_DIGITIZER_FINGER)) {
                                
                                TouchpadInfo info;
                                info.devicePath = devicePath;
                                info.vendorId = attributes.VendorID;
                                info.productId = attributes.ProductID;
                                info.usagePage = caps.UsagePage;
                                info.usage = caps.Usage;
                                
                                // Get device name
                                wchar_t productString[256] = {0};
                                if (HidD_GetProductString(deviceHandle, productString, sizeof(productString))) {
                                    info.deviceName = productString;
                                }
                                
                                wchar_t manufacturerString[256] = {0};
                                if (HidD_GetManufacturerString(deviceHandle, manufacturerString, sizeof(manufacturerString))) {
                                    info.manufacturer = manufacturerString;
                                }
                                
                                // Get detailed capabilities
                                GetDeviceCapabilities(devicePath, info);
                                info.isPrecisionTouchpad = IsPrecisionTouchpad(info);
                                
                                // Only add touchpads (usage 0x05), not touchscreens
                                if (caps.Usage == TOUCH_USAGE_DIGITIZER_TOUCH_PAD) {
                                    devices.push_back(info);
                                }
                            }
                        }
                        
                        HidD_FreePreparsedData(preparsedData);
                    }
                }
                
                CloseHandle(deviceHandle);
            }
        }
        
        free(detailData);
    }
    
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    
    return devices;
}

void DeviceEnumerator::EnumerateViaRawInput(std::vector<TouchpadInfo>& devices) {
    UINT deviceCount = 0;
    GetRawInputDeviceList(nullptr, &deviceCount, sizeof(RAWINPUTDEVICELIST));
    
    if (deviceCount == 0) return;
    
    std::vector<RAWINPUTDEVICELIST> deviceList(deviceCount);
    GetRawInputDeviceList(deviceList.data(), &deviceCount, sizeof(RAWINPUTDEVICELIST));
    
    for (UINT i = 0; i < deviceCount; i++) {
        const auto& device = deviceList[i];
        
        if (device.dwType != RIM_TYPEHID) continue;
        
        RID_DEVICE_INFO deviceInfo;
        deviceInfo.cbSize = sizeof(RID_DEVICE_INFO);
        UINT size = sizeof(RID_DEVICE_INFO);
        GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICEINFO, &deviceInfo, &size);
        
        // Check if this is a touchpad (Digitizer page, Touchpad usage)
        if (deviceInfo.hid.usUsagePage == TOUCH_USAGE_PAGE_DIGITIZER &&
            deviceInfo.hid.usUsage == TOUCH_USAGE_DIGITIZER_TOUCH_PAD) {
            
            // Get device name/path
            UINT nameSize = 0;
            GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, nullptr, &nameSize);
            
            std::wstring devicePath(nameSize, L'\0');
            GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, &devicePath[0], &nameSize);
            
            // Remove null terminator if present
            if (!devicePath.empty() && devicePath.back() == L'\0') {
                devicePath.pop_back();
            }
            
            TouchpadInfo info;
            info.devicePath = devicePath;
            info.vendorId = deviceInfo.hid.dwVendorId;
            info.productId = deviceInfo.hid.dwProductId;
            info.usagePage = deviceInfo.hid.usUsagePage;
            info.usage = deviceInfo.hid.usUsage;
            info.deviceName = L"Precision Touchpad";
            info.manufacturer = L"Unknown";
            
            // Try to open the device to get more details
            // Note: We may not have direct access, but we can still use Raw Input
            HANDLE deviceHandle = CreateFile(
                devicePath.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr
            );
            
            if (deviceHandle != INVALID_HANDLE_VALUE) {
                wchar_t productString[256] = {0};
                if (HidD_GetProductString(deviceHandle, productString, sizeof(productString))) {
                    info.deviceName = productString;
                }
                
                wchar_t manufacturerString[256] = {0};
                if (HidD_GetManufacturerString(deviceHandle, manufacturerString, sizeof(manufacturerString))) {
                    info.manufacturer = manufacturerString;
                }
                
                PHIDP_PREPARSED_DATA preparsedData = nullptr;
                if (HidD_GetPreparsedData(deviceHandle, &preparsedData)) {
                    HIDP_CAPS caps;
                    if (HidP_GetCaps(preparsedData, &caps) == HIDP_STATUS_SUCCESS) {
                        // Get value capabilities for coordinate ranges
                        USHORT valueCapsLength = caps.NumberInputValueCaps;
                        if (valueCapsLength > 0) {
                            auto* valueCaps = new HIDP_VALUE_CAPS[valueCapsLength];
                            
                            if (HidP_GetValueCaps(HidP_Input, valueCaps, &valueCapsLength, preparsedData) == HIDP_STATUS_SUCCESS) {
                                for (USHORT j = 0; j < valueCapsLength; j++) {
                                    auto& vc = valueCaps[j];
                                    
                                    if (vc.UsagePage == TOUCH_USAGE_PAGE_GENERIC && vc.Range.UsageMin == TOUCH_USAGE_GENERIC_X) {
                                        info.minX = vc.LogicalMin;
                                        info.maxX = vc.LogicalMax;
                                    }
                                    else if (vc.UsagePage == TOUCH_USAGE_PAGE_GENERIC && vc.Range.UsageMin == TOUCH_USAGE_GENERIC_Y) {
                                        info.minY = vc.LogicalMin;
                                        info.maxY = vc.LogicalMax;
                                    }
                                    else if (vc.UsagePage == TOUCH_USAGE_PAGE_DIGITIZER && vc.Range.UsageMin == TOUCH_USAGE_DIGITIZER_CONTACT_COUNT) {
                                        info.maxContacts = vc.LogicalMax;
                                    }
                                }
                            }
                            delete[] valueCaps;
                        }
                    }
                    HidD_FreePreparsedData(preparsedData);
                }
                
                CloseHandle(deviceHandle);
            } else {
                // Can't open directly - set defaults for precision touchpad
                info.minX = 0;
                info.maxX = 1920;
                info.minY = 0;
                info.maxY = 1080;
                info.maxContacts = 5;
            }
            
            if (info.maxContacts == 0) info.maxContacts = 5;
            info.isPrecisionTouchpad = true; // Devices on this usage page/usage are PTP
            info.supportsClick = true;
            
            devices.push_back(info);
        }
    }
}

void DeviceEnumerator::GetDeviceCapabilities(const std::wstring& devicePath, TouchpadInfo& info) {
    HANDLE deviceHandle = CreateFile(
        devicePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    
    if (deviceHandle == INVALID_HANDLE_VALUE) return;
    
    PHIDP_PREPARSED_DATA preparsedData = nullptr;
    if (!HidD_GetPreparsedData(deviceHandle, &preparsedData)) {
        CloseHandle(deviceHandle);
        return;
    }
    
    HIDP_CAPS caps;
    if (HidP_GetCaps(preparsedData, &caps) != HIDP_STATUS_SUCCESS) {
        HidD_FreePreparsedData(preparsedData);
        CloseHandle(deviceHandle);
        return;
    }
    
    // Get value capabilities to find coordinate ranges
    USHORT valueCapsLength = caps.NumberInputValueCaps;
    if (valueCapsLength > 0) {
        auto* valueCaps = new HIDP_VALUE_CAPS[valueCapsLength];
        
        if (HidP_GetValueCaps(HidP_Input, valueCaps, &valueCapsLength, preparsedData) == HIDP_STATUS_SUCCESS) {
            for (USHORT i = 0; i < valueCapsLength; i++) {
                auto& vc = valueCaps[i];
                
                // X coordinate
                if (vc.UsagePage == TOUCH_USAGE_PAGE_GENERIC && vc.Range.UsageMin == TOUCH_USAGE_GENERIC_X) {
                    info.minX = vc.LogicalMin;
                    info.maxX = vc.LogicalMax;
                }
                // Y coordinate  
                else if (vc.UsagePage == TOUCH_USAGE_PAGE_GENERIC && vc.Range.UsageMin == TOUCH_USAGE_GENERIC_Y) {
                    info.minY = vc.LogicalMin;
                    info.maxY = vc.LogicalMax;
                }
                // Pressure
                else if (vc.UsagePage == TOUCH_USAGE_PAGE_DIGITIZER && vc.Range.UsageMin == TOUCH_USAGE_DIGITIZER_TIP_PRESSURE) {
                    info.minPressure = vc.LogicalMin;
                    info.maxPressure = vc.LogicalMax;
                    info.supportsPressure = true;
                }
                // Contact count
                else if (vc.UsagePage == TOUCH_USAGE_PAGE_DIGITIZER && vc.Range.UsageMin == TOUCH_USAGE_DIGITIZER_CONTACT_COUNT) {
                    info.maxContacts = vc.LogicalMax;
                }
            }
        }
        
        delete[] valueCaps;
    }
    
    // Get button capabilities for click detection
    USHORT buttonCapsLength = caps.NumberInputButtonCaps;
    if (buttonCapsLength > 0) {
        auto* buttonCaps = new HIDP_BUTTON_CAPS[buttonCapsLength];
        
        if (HidP_GetButtonCaps(HidP_Input, buttonCaps, &buttonCapsLength, preparsedData) == HIDP_STATUS_SUCCESS) {
            for (USHORT i = 0; i < buttonCapsLength; i++) {
                if (buttonCaps[i].UsagePage == TOUCH_USAGE_PAGE_DIGITIZER) {
                    info.supportsClick = true;
                    break;
                }
            }
        }
        
        delete[] buttonCaps;
    }
    
    // Default max contacts if not specified
    if (info.maxContacts == 0) {
        info.maxContacts = 5;
    }
    
    HidD_FreePreparsedData(preparsedData);
    CloseHandle(deviceHandle);
}

bool DeviceEnumerator::IsPrecisionTouchpad(const TouchpadInfo& info) {
    // Precision touchpads typically have:
    // - Coordinate ranges > 1000
    // - Support for multiple contacts
    // - Proper usage page for touchpad
    
    bool hasGoodCoordinateRange = (info.maxX - info.minX) > 1000 && (info.maxY - info.minY) > 500;
    bool hasMultiTouch = info.maxContacts >= 2;
    
    return hasGoodCoordinateRange && hasMultiTouch;
}

TouchpadInfo DeviceEnumerator::FindBestTouchpad() {
    auto devices = EnumerateTouchDevices();
    
    if (devices.empty()) {
        std::cerr << "No touchpad devices found!" << std::endl;
        return TouchpadInfo{};
    }
    
    // Prefer precision touchpads
    for (const auto& device : devices) {
        if (device.isPrecisionTouchpad) {
            return device;
        }
    }
    
    // Return first device if no precision touchpad found
    return devices[0];
}

void DeviceEnumerator::PrintDeviceInfo(const TouchpadInfo& info) {
    std::wcout << L"========================================" << std::endl;
    std::wcout << L"Device: " << info.deviceName << std::endl;
    std::wcout << L"Manufacturer: " << info.manufacturer << std::endl;
    std::wcout << L"Path: " << info.devicePath << std::endl;
    std::wcout << L"VID: 0x" << std::hex << std::setw(4) << std::setfill(L'0') << info.vendorId << std::endl;
    std::wcout << L"PID: 0x" << std::hex << std::setw(4) << std::setfill(L'0') << info.productId << std::dec << std::endl;
    std::wcout << L"Usage Page: 0x" << std::hex << info.usagePage << L", Usage: 0x" << info.usage << std::dec << std::endl;
    std::wcout << L"X Range: " << info.minX << L" - " << info.maxX << std::endl;
    std::wcout << L"Y Range: " << info.minY << L" - " << info.maxY << std::endl;
    std::wcout << L"Max Contacts: " << info.maxContacts << std::endl;
    std::wcout << L"Precision Touchpad: " << (info.isPrecisionTouchpad ? L"Yes" : L"No") << std::endl;
    std::wcout << L"Supports Pressure: " << (info.supportsPressure ? L"Yes" : L"No") << std::endl;
    std::wcout << L"Supports Click: " << (info.supportsClick ? L"Yes" : L"No") << std::endl;
    std::wcout << L"========================================" << std::endl;
}

std::vector<std::string> DeviceEnumerator::GetDeviceList() {
    std::vector<std::string> list;
    auto devices = EnumerateTouchDevices();
    
    for (const auto& device : devices) {
        std::string entry;
        entry += "Device: ";
        
        // Convert wide string to narrow
        std::string name(device.deviceName.begin(), device.deviceName.end());
        entry += name;
        entry += " [";
        entry += device.isPrecisionTouchpad ? "Precision" : "Standard";
        entry += "]";
        
        list.push_back(entry);
    }
    
    return list;
}

} // namespace TouchpadTracker
