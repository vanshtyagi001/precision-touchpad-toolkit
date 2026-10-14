// Device listing utility
// Lists all HID touch devices on the system

#include "DeviceEnumerator.h"
#include <Windows.h>
#include <SetupAPI.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <devpkey.h>
#include <devguid.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

using namespace TouchpadTracker;

void ListRawInputDevices() {
    std::cout << "Scanning Raw Input devices..." << std::endl << std::endl;
    
    UINT deviceCount = 0;
    GetRawInputDeviceList(nullptr, &deviceCount, sizeof(RAWINPUTDEVICELIST));
    
    if (deviceCount == 0) {
        std::cout << "No raw input devices found." << std::endl;
        return;
    }
    
    std::vector<RAWINPUTDEVICELIST> deviceList(deviceCount);
    GetRawInputDeviceList(deviceList.data(), &deviceCount, sizeof(RAWINPUTDEVICELIST));
    
    int hidCount = 0, mouseCount = 0, keyboardCount = 0;
    
    for (UINT i = 0; i < deviceCount; i++) {
        const auto& device = deviceList[i];
        
        // Get device name
        UINT nameSize = 0;
        GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, nullptr, &nameSize);
        
        std::wstring deviceName(nameSize, L'\0');
        GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, &deviceName[0], &nameSize);
        
        // Check if it's an ASUS device or a digitizer
        bool isAsus = (deviceName.find(L"asuf") != std::wstring::npos);
        
        if (device.dwType == RIM_TYPEHID) {
            RID_DEVICE_INFO deviceInfo;
            deviceInfo.cbSize = sizeof(RID_DEVICE_INFO);
            UINT size = sizeof(RID_DEVICE_INFO);
            GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICEINFO, &deviceInfo, &size);
            
            bool isDigitizer = (deviceInfo.hid.usUsagePage == 0x0D);
            
            if (isAsus || isDigitizer) {
                hidCount++;
                std::cout << "--- HID Device ---" << std::endl;
                std::cout << "  Type: HID" << std::endl;
                std::cout << "  VID: 0x" << std::hex << std::setw(4) << std::setfill('0') << deviceInfo.hid.dwVendorId;
                std::cout << " PID: 0x" << std::setw(4) << std::setfill('0') << deviceInfo.hid.dwProductId << std::dec << std::endl;
                std::cout << "  Usage Page: 0x" << std::hex << deviceInfo.hid.usUsagePage;
                std::cout << " Usage: 0x" << deviceInfo.hid.usUsage << std::dec << std::endl;
                
                std::string pageType;
                if (deviceInfo.hid.usUsagePage == 0x01) pageType = "Generic Desktop";
                else if (deviceInfo.hid.usUsagePage == 0x0D) pageType = "Digitizer";
                else if (deviceInfo.hid.usUsagePage == 0xFF00) pageType = "Vendor-specific";
                else pageType = "Other";
                std::cout << "  Page Type: " << pageType << std::endl;
                
                if (deviceInfo.hid.usUsagePage == 0x0D) {
                    std::string usageType;
                    switch (deviceInfo.hid.usUsage) {
                        case 0x01: usageType = "Digitizer (generic)"; break;
                        case 0x02: usageType = "Pen"; break;
                        case 0x04: usageType = "Touch Screen"; break;
                        case 0x05: usageType = "Touch Pad ***FOUND***"; break;
                        case 0x0E: usageType = "Device Configuration"; break;
                        case 0x22: usageType = "Finger"; break;
                        default: usageType = "Unknown"; break;
                    }
                    std::cout << "  Usage Type: " << usageType << std::endl;
                }
                
                std::wcout << L"  Device Path: " << deviceName << std::endl;
                std::cout << std::endl;
            }
        } else if (device.dwType == RIM_TYPEMOUSE && isAsus) {
            mouseCount++;
            std::cout << "--- Mouse Device (ASUS) ---" << std::endl;
            std::wcout << L"  Device Path: " << deviceName << std::endl;
            std::cout << std::endl;
        }
    }
    
    std::cout << "Raw Input Summary: " << deviceCount << " total devices" << std::endl;
    std::cout << "  HID (ASUS/Digitizers): " << hidCount << std::endl;
    std::cout << "  Mouse (ASUS): " << mouseCount << std::endl << std::endl;
}

void ListAllHidDevices() {
    std::cout << "Scanning ALL HID devices..." << std::endl << std::endl;
    
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
        return;
    }
    
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    int digitizerCount = 0;
    int touchpadCount = 0;
    int touchscreenCount = 0;
    int totalDevices = 0;
    
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
            HANDLE deviceHandle = CreateFile(
                detailData->DevicePath,
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
                            totalDevices++;
                            
                            // Check path for ASUS touchpad
                            std::wstring path(detailData->DevicePath);
                            bool isAsus = (path.find(L"asuf") != std::wstring::npos);
                            bool isDigitizer = (caps.UsagePage == 0x0D);
                            bool isMouse = (caps.UsagePage == 0x01 && caps.Usage == 0x02);
                            
                            // Show ALL ASUS devices, all digitizers, and mice
                            if (isAsus || isDigitizer || isMouse) {
                                wchar_t productString[256] = {0};
                                wchar_t manufacturerString[256] = {0};
                                HidD_GetProductString(deviceHandle, productString, sizeof(productString));
                                HidD_GetManufacturerString(deviceHandle, manufacturerString, sizeof(manufacturerString));
                                
                                if (isDigitizer) digitizerCount++;
                                if (caps.UsagePage == 0x0D && caps.Usage == 0x05) touchpadCount++;
                                if (caps.UsagePage == 0x0D && caps.Usage == 0x04) touchscreenCount++;
                                
                                std::cout << "--- Device ---" << std::endl;
                                std::wcout << L"  Name: " << productString << std::endl;
                                std::wcout << L"  Manufacturer: " << manufacturerString << std::endl;
                                std::cout << "  VID: 0x" << std::hex << std::setw(4) << std::setfill('0') << attributes.VendorID;
                                std::cout << " PID: 0x" << std::setw(4) << std::setfill('0') << attributes.ProductID << std::dec << std::endl;
                                std::cout << "  Usage Page: 0x" << std::hex << caps.UsagePage << " Usage: 0x" << caps.Usage << std::dec << std::endl;
                                
                                // Decode usage page
                                std::string usagePage;
                                switch (caps.UsagePage) {
                                    case 0x01: usagePage = "Generic Desktop"; break;
                                    case 0x02: usagePage = "Simulation"; break;
                                    case 0x0C: usagePage = "Consumer"; break;
                                    case 0x0D: usagePage = "Digitizer"; break;
                                    case 0xFF00: usagePage = "Vendor-specific"; break;
                                    default: usagePage = "Other"; break;
                                }
                                std::cout << "  Page Type: " << usagePage << std::endl;
                                
                                // Decode usage
                                std::string usageType;
                                if (caps.UsagePage == 0x0D) {
                                    switch (caps.Usage) {
                                        case 0x01: usageType = "Digitizer (generic)"; break;
                                        case 0x02: usageType = "Pen"; break;
                                        case 0x04: usageType = "Touch Screen"; break;
                                        case 0x05: usageType = "Touch Pad ***FOUND***"; break;
                                        case 0x0E: usageType = "Device Configuration"; break;
                                        case 0x22: usageType = "Finger"; break;
                                        default: usageType = "Digitizer Unknown"; break;
                                    }
                                } else if (caps.UsagePage == 0x01) {
                                    switch (caps.Usage) {
                                        case 0x01: usageType = "Pointer"; break;
                                        case 0x02: usageType = "Mouse"; break;
                                        case 0x04: usageType = "Joystick"; break;
                                        case 0x05: usageType = "Gamepad"; break;
                                        case 0x06: usageType = "Keyboard"; break;
                                        default: usageType = "Generic Desktop Unknown"; break;
                                    }
                                } else {
                                    usageType = "Unknown";
                                }
                                std::cout << "  Usage Type: " << usageType << std::endl;
                                
                                // Show report sizes
                                std::cout << "  Input Report Size: " << caps.InputReportByteLength << " bytes" << std::endl;
                                std::cout << "  Number Link Collections: " << caps.NumberLinkCollectionNodes << std::endl;
                                
                                // Get value capabilities
                                USHORT valueCapsLength = caps.NumberInputValueCaps;
                                if (valueCapsLength > 0) {
                                    auto* valueCaps = new HIDP_VALUE_CAPS[valueCapsLength];
                                    if (HidP_GetValueCaps(HidP_Input, valueCaps, &valueCapsLength, preparsedData) == HIDP_STATUS_SUCCESS) {
                                        std::cout << "  Input Value Caps (" << valueCapsLength << "):" << std::endl;
                                        for (USHORT j = 0; j < valueCapsLength && j < 10; j++) {
                                            std::cout << "    - Page: 0x" << std::hex << valueCaps[j].UsagePage;
                                            std::cout << " Usage: 0x" << valueCaps[j].Range.UsageMin << std::dec;
                                            std::cout << " Range: " << valueCaps[j].LogicalMin << "-" << valueCaps[j].LogicalMax;
                                            if (valueCaps[j].UsagePage == 0x01) {
                                                if (valueCaps[j].Range.UsageMin == 0x30) std::cout << " (X)";
                                                if (valueCaps[j].Range.UsageMin == 0x31) std::cout << " (Y)";
                                            }
                                            if (valueCaps[j].UsagePage == 0x0D) {
                                                if (valueCaps[j].Range.UsageMin == 0x48) std::cout << " (Width)";
                                                if (valueCaps[j].Range.UsageMin == 0x49) std::cout << " (Height)";
                                                if (valueCaps[j].Range.UsageMin == 0x51) std::cout << " (Contact ID)";
                                                if (valueCaps[j].Range.UsageMin == 0x54) std::cout << " (Contact Count)";
                                                if (valueCaps[j].Range.UsageMin == 0x56) std::cout << " (Scan Time)";
                                            }
                                            std::cout << std::endl;
                                        }
                                        if (valueCapsLength > 10) {
                                            std::cout << "    ... and " << (valueCapsLength - 10) << " more" << std::endl;
                                        }
                                    }
                                    delete[] valueCaps;
                                }
                                
                                // Get button capabilities
                                USHORT buttonCapsLength = caps.NumberInputButtonCaps;
                                if (buttonCapsLength > 0) {
                                    auto* buttonCaps = new HIDP_BUTTON_CAPS[buttonCapsLength];
                                    if (HidP_GetButtonCaps(HidP_Input, buttonCaps, &buttonCapsLength, preparsedData) == HIDP_STATUS_SUCCESS) {
                                        std::cout << "  Input Button Caps (" << buttonCapsLength << "):" << std::endl;
                                        for (USHORT j = 0; j < buttonCapsLength && j < 5; j++) {
                                            std::cout << "    - Page: 0x" << std::hex << buttonCaps[j].UsagePage;
                                            std::cout << " Usage: 0x" << buttonCaps[j].Range.UsageMin << "-0x" << buttonCaps[j].Range.UsageMax << std::dec;
                                            if (buttonCaps[j].UsagePage == 0x0D && buttonCaps[j].Range.UsageMin == 0x42) std::cout << " (Tip Switch)";
                                            std::cout << std::endl;
                                        }
                                    }
                                    delete[] buttonCaps;
                                }
                                
                                std::cout << "  Path: " << std::endl;
                                std::wcout << L"    " << detailData->DevicePath << std::endl;
                                std::cout << std::endl;
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
    
    std::cout << "========================================" << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  Total HID devices scanned: " << totalDevices << std::endl;
    std::cout << "  Digitizer page devices: " << digitizerCount << std::endl;
    std::cout << "  Touch Pads (0x0D:0x05): " << touchpadCount << std::endl;
    std::cout << "  Touch Screens (0x0D:0x04): " << touchscreenCount << std::endl;
    std::cout << "========================================" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   HID Touch Device Enumerator" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // First, check Raw Input devices
    ListRawInputDevices();
    
    // Then show all HID digitizer devices
    ListAllHidDevices();
    std::cout << std::endl;
    
    // Then use our enumerator
    std::cout << "========================================" << std::endl;
    std::cout << "   Precision Touchpad Detection" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    DeviceEnumerator enumerator;
    auto devices = enumerator.EnumerateTouchDevices();
    
    std::cout << "Found " << devices.size() << " precision touchpad device(s):" << std::endl;
    std::cout << std::endl;
    
    for (size_t i = 0; i < devices.size(); i++) {
        std::cout << "--- Device " << i << " ---" << std::endl;
        DeviceEnumerator::PrintDeviceInfo(devices[i]);
        std::cout << std::endl;
    }
    
    if (devices.empty()) {
        std::cout << "No precision touchpad devices found via standard enumeration." << std::endl;
        std::cout << std::endl;
        std::cout << "Possible reasons:" << std::endl;
        std::cout << "  - No precision touchpad present" << std::endl;
        std::cout << "  - Legacy/non-PTP touchpad" << std::endl;
        std::cout << "  - Driver not supporting HID protocol" << std::endl;
        std::cout << "  - Device in use by another application" << std::endl;
    } else {
        std::cout << "Recommended device: ";
        auto best = enumerator.FindBestTouchpad();
        std::wcout << best.deviceName << std::endl;
        std::cout << "  Precision Touchpad: " << (best.isPrecisionTouchpad ? "Yes" : "No") << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    
    return 0;
}
