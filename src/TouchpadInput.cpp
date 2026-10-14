#include "TouchpadInput.h"

#include <Windows.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <iostream>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "hid.lib")

namespace TouchpadTracker {

// HID Usage definitions - use TOUCH_ prefix to avoid conflicts with Windows SDK macros
constexpr USHORT TOUCH_USAGE_PAGE_DIGITIZER = 0x0D;
constexpr USHORT TOUCH_USAGE_PAGE_GENERIC = 0x01;
constexpr USHORT TOUCH_USAGE_PAGE_BUTTON = 0x09;

constexpr USHORT TOUCH_USAGE_DIGITIZER_TOUCH_PAD = 0x05;
constexpr USHORT TOUCH_USAGE_DIGITIZER_FINGER = 0x22;
constexpr USHORT TOUCH_USAGE_DIGITIZER_TIP_SWITCH = 0x42;
constexpr USHORT TOUCH_USAGE_DIGITIZER_IN_RANGE = 0x32;
constexpr USHORT TOUCH_USAGE_DIGITIZER_CONTACT_ID = 0x51;
constexpr USHORT TOUCH_USAGE_DIGITIZER_CONTACT_COUNT = 0x54;
constexpr USHORT TOUCH_USAGE_DIGITIZER_TIP_PRESSURE = 0x30;
constexpr USHORT TOUCH_USAGE_DIGITIZER_SCAN_TIME = 0x56;
constexpr USHORT TOUCH_USAGE_DIGITIZER_WIDTH = 0x48;
constexpr USHORT TOUCH_USAGE_DIGITIZER_HEIGHT = 0x49;

constexpr USHORT TOUCH_USAGE_GENERIC_X = 0x30;
constexpr USHORT TOUCH_USAGE_GENERIC_Y = 0x31;

TouchpadInput::TouchpadInput() {
    m_lastRateCalc = Clock::now();
}

TouchpadInput::~TouchpadInput() {
    StopCapture();
    
    if (m_preparsedData) {
        // If we got preparsed data from CreateFile, use HidD_FreePreparsedData
        // If from GetRawInputDeviceInfo, use delete[]
        if (m_preparsedDataSize > 0) {
            // Got from GetRawInputDeviceInfo
            delete[] static_cast<uint8_t*>(m_preparsedData);
        } else {
            // Got from HidD_GetPreparsedData
            HidD_FreePreparsedData(static_cast<PHIDP_PREPARSED_DATA>(m_preparsedData));
        }
    }
    
    if (m_deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_deviceHandle);
    }
}

bool TouchpadInput::Initialize(const TouchpadInfo& device) {
    m_deviceInfo = device;
    
    // Try to open device for reading preparsed data
    // Note: On Windows 10/11, precision touchpads are often exclusively held by the system
    // We try to open it, but if we fail with ERROR_SHARING_VIOLATION, we can still work
    // using Raw Input API - we just won't have preparsed data for detailed parsing
    m_deviceHandle = CreateFile(
        device.devicePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    
    if (m_deviceHandle != INVALID_HANDLE_VALUE) {
        // Great! We can access the device directly
        PHIDP_PREPARSED_DATA preparsed = nullptr;
        if (HidD_GetPreparsedData(m_deviceHandle, &preparsed)) {
            m_preparsedData = preparsed;
            std::cout << "Got preparsed data from device." << std::endl;
        }
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_SHARING_VIOLATION) {
            std::cout << "Device is exclusively held by system (normal for precision touchpad)." << std::endl;
            std::cout << "Will use Raw Input API for data capture." << std::endl;
        } else {
            std::cerr << "Warning: Could not open device directly. Error: " << error << std::endl;
        }
        // Continue anyway - we can still use Raw Input
    }
    
    // Initialize previous contacts
    for (auto& contact : m_previousContacts) {
        contact.isValid = false;
    }
    
    m_initialized = true;
    
    std::cout << "Touchpad initialized successfully." << std::endl;
    std::cout << "Device: " << std::string(device.deviceName.begin(), device.deviceName.end()) << std::endl;
    std::cout << "Coordinate range: X(" << device.minX << "-" << device.maxX << ") Y(" << device.minY << "-" << device.maxY << ")" << std::endl;
    
    return true;
}

bool TouchpadInput::InitializeAutoDetect() {
    DeviceEnumerator enumerator;
    TouchpadInfo device = enumerator.FindBestTouchpad();
    
    if (device.devicePath.empty()) {
        std::cerr << "No touchpad device found!" << std::endl;
        return false;
    }
    
    return Initialize(device);
}

bool TouchpadInput::RegisterForRawInput() {
    if (!m_hwnd) {
        std::cerr << "Window handle not set!" << std::endl;
        return false;
    }
    
    RAWINPUTDEVICE rid[1];
    
    // Register for digitizer/touchpad input
    rid[0].usUsagePage = TOUCH_USAGE_PAGE_DIGITIZER;
    rid[0].usUsage = TOUCH_USAGE_DIGITIZER_TOUCH_PAD;
    rid[0].dwFlags = RIDEV_INPUTSINK; // Receive input even when not focused
    rid[0].hwndTarget = m_hwnd;
    
    if (!RegisterRawInputDevices(rid, 1, sizeof(rid[0]))) {
        std::cerr << "Failed to register for raw input. Error: " << GetLastError() << std::endl;
        return false;
    }
    
    std::cout << "Registered for raw touchpad input." << std::endl;
    return true;
}

void TouchpadInput::UnregisterRawInput() {
    RAWINPUTDEVICE rid[1];
    
    rid[0].usUsagePage = TOUCH_USAGE_PAGE_DIGITIZER;
    rid[0].usUsage = TOUCH_USAGE_DIGITIZER_TOUCH_PAD;
    rid[0].dwFlags = RIDEV_REMOVE;
    rid[0].hwndTarget = nullptr;
    
    RegisterRawInputDevices(rid, 1, sizeof(rid[0]));
}

bool TouchpadInput::StartCapture() {
    if (!RegisterForRawInput()) {
        return false;
    }
    
    m_isCapturing = true;
    m_frameCount = 0;
    m_lastRateCalc = Clock::now();
    m_framesForRate = 0;
    
    std::cout << "Touch capture started." << std::endl;
    return true;
}

void TouchpadInput::StopCapture() {
    if (m_isCapturing) {
        UnregisterRawInput();
        m_isCapturing = false;
        std::cout << "Touch capture stopped." << std::endl;
    }
}

TouchFrame TouchpadInput::GetCurrentFrame() const {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    return m_currentFrame;
}

void TouchpadInput::SetTouchFrameCallback(TouchFrameCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_touchFrameCallback = callback;
}

void TouchpadInput::SetRawDataCallback(RawDataCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_rawDataCallback = callback;
}

bool TouchpadInput::ProcessRawInput(LPARAM lParam) {
    UINT size = 0;
    
    // Get size of raw input data
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1) {
        return false;
    }
    
    if (size == 0) return false;
    
    // Allocate buffer and get data
    std::vector<uint8_t> buffer(size);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) {
        return false;
    }
    
    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer.data());
    
    // Only process HID input
    if (raw->header.dwType != RIM_TYPEHID) {
        return false;
    }
    
    // If we don't have preparsed data, try to get it from Raw Input device info
    if (!m_preparsedData) {
        UINT preparsedSize = 0;
        GetRawInputDeviceInfo(raw->header.hDevice, RIDI_PREPARSEDDATA, nullptr, &preparsedSize);
        
        if (preparsedSize > 0) {
            auto* preparsedBuffer = new uint8_t[preparsedSize];
            if (GetRawInputDeviceInfo(raw->header.hDevice, RIDI_PREPARSEDDATA, preparsedBuffer, &preparsedSize) == preparsedSize) {
                m_preparsedData = preparsedBuffer;
                m_preparsedDataSize = preparsedSize;
                std::cout << "Got preparsed data from Raw Input device (" << preparsedSize << " bytes)" << std::endl;
                
                // Also get detailed caps
                PHIDP_PREPARSED_DATA preparsed = reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsedBuffer);
                HIDP_CAPS caps;
                if (HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS) {
                    // Update device info with actual coordinate ranges
                    USHORT valueCapsLength = caps.NumberInputValueCaps;
                    if (valueCapsLength > 0) {
                        auto* valueCaps = new HIDP_VALUE_CAPS[valueCapsLength];
                        if (HidP_GetValueCaps(HidP_Input, valueCaps, &valueCapsLength, preparsed) == HIDP_STATUS_SUCCESS) {
                            for (USHORT i = 0; i < valueCapsLength; i++) {
                                auto& vc = valueCaps[i];
                                if (vc.UsagePage == TOUCH_USAGE_PAGE_GENERIC && vc.Range.UsageMin == TOUCH_USAGE_GENERIC_X) {
                                    m_deviceInfo.minX = vc.LogicalMin;
                                    m_deviceInfo.maxX = vc.LogicalMax;
                                    std::cout << "X range: " << m_deviceInfo.minX << " - " << m_deviceInfo.maxX << std::endl;
                                }
                                else if (vc.UsagePage == TOUCH_USAGE_PAGE_GENERIC && vc.Range.UsageMin == TOUCH_USAGE_GENERIC_Y) {
                                    m_deviceInfo.minY = vc.LogicalMin;
                                    m_deviceInfo.maxY = vc.LogicalMax;
                                    std::cout << "Y range: " << m_deviceInfo.minY << " - " << m_deviceInfo.maxY << std::endl;
                                }
                                else if (vc.UsagePage == TOUCH_USAGE_PAGE_DIGITIZER && vc.Range.UsageMin == TOUCH_USAGE_DIGITIZER_CONTACT_COUNT) {
                                    m_deviceInfo.maxContacts = vc.LogicalMax;
                                    std::cout << "Max contacts: " << m_deviceInfo.maxContacts << std::endl;
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
    
    // Get HID data
    const uint8_t* hidData = raw->data.hid.bRawData;
    size_t hidDataSize = raw->data.hid.dwSizeHid * raw->data.hid.dwCount;
    
    if (hidDataSize == 0) return false;
    
    // Call raw data callback if set
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        if (m_rawDataCallback) {
            m_rawDataCallback(hidData, hidDataSize);
        }
    }
    
    // Parse the HID report
    ParseHidReport(hidData, raw->data.hid.dwSizeHid);
    
    return true;
}

void TouchpadInput::ParseHidReport(const uint8_t* data, size_t size) {
    if (size == 0) return;
    
    auto now = Clock::now();
    
    // Try raw parsing first for ASUS precision touchpad format
    // Format: ReportID(1) ContactInfo(1) X(2) Y(2) ScanTime(2) ContactCount(1) ...
    if (size >= 9 && data[0] == 0x04) {
        // ASUS precision touchpad format detected
        uint8_t contactInfo = data[1];
        bool fingerDown = (contactInfo & 0x03) != 0;  // Tip switch or in-range
        uint8_t contactId = (contactInfo >> 2) & 0x3F;  // Contact ID in upper bits
        
        // Also try alternate parsing: bit 0 = tip, bit 1 = in-range
        if (!fingerDown) {
            fingerDown = (contactInfo & 0x01) != 0;
            contactId = (contactInfo >> 1) & 0x7F;
        }
        
        uint16_t rawX = data[2] | (data[3] << 8);
        uint16_t rawY = data[4] | (data[5] << 8);
        uint8_t contactCount = data[8];
        
        // Track this contact
        if (fingerDown && (rawX != 0 || rawY != 0)) {
            // Find or create slot for this contact ID
            int slot = -1;
            for (int i = 0; i < 10; i++) {
                if (m_activeContacts[i].isValid && m_activeContacts[i].id == contactId) {
                    slot = i;
                    break;
                }
            }
            if (slot < 0) {
                for (int i = 0; i < 10; i++) {
                    if (!m_activeContacts[i].isValid) {
                        slot = i;
                        break;
                    }
                }
            }
            if (slot < 0) slot = 0;
            
            TouchContact& contact = m_activeContacts[slot];
            bool wasDown = contact.isValid && contact.id == contactId;
            
            contact.id = contactId;
            contact.rawX = static_cast<float>(rawX);
            contact.rawY = static_cast<float>(rawY);
            
            // Normalize coordinates using actual device range
            float rangeX = static_cast<float>(m_deviceInfo.maxX - m_deviceInfo.minX);
            float rangeY = static_cast<float>(m_deviceInfo.maxY - m_deviceInfo.minY);
            
            float newX = rangeX > 0 ? (contact.rawX - m_deviceInfo.minX) / rangeX : contact.rawX / 4096.0f;
            float newY = rangeY > 0 ? (contact.rawY - m_deviceInfo.minY) / rangeY : contact.rawY / 2400.0f;
            
            // Clamp to 0-1 range
            newX = std::clamp(newX, 0.0f, 1.0f);
            newY = std::clamp(newY, 0.0f, 1.0f);
            
            // Calculate velocity before updating position
            if (wasDown) {
                auto dt = std::chrono::duration<float>(now - contact.timestamp).count();
                if (dt > 0 && dt < 0.5f) {
                    contact.deltaX = newX - contact.x;
                    contact.deltaY = newY - contact.y;
                    contact.velocityX = contact.deltaX / dt;
                    contact.velocityY = contact.deltaY / dt;
                }
            } else {
                contact.deltaX = contact.deltaY = 0;
                contact.velocityX = contact.velocityY = 0;
            }
            
            contact.x = newX;
            contact.y = newY;
            contact.pressure = 1.0f;
            contact.timestamp = now;
            contact.isValid = true;
            contact.lastSeen = now;
            contact.state = wasDown ? ContactState::Move : ContactState::Down;
        }
        
        // Handle multi-touch - parse additional contacts if present
        // Check if there are more contacts in this report
        if (contactCount > 1 && size >= 14) {
            // Additional contacts start at offset 9 (after scan time and contact count)
            // Format for each: ContactInfo(1) X(2) Y(2) = 5 bytes per contact
            size_t offset = 9;
            for (uint32_t i = 1; i < contactCount && i < 10 && offset + 4 < size; i++) {
                // Check if there's padding/scan time between contacts
                // Try parsing at offset and offset+1
                uint8_t cInfo = data[offset];
                bool cDown = (cInfo & 0x03) != 0;
                uint8_t cId = (cInfo >> 2) & 0x3F;
                
                if (!cDown) {
                    cDown = (cInfo & 0x01) != 0;
                    cId = (cInfo >> 1) & 0x7F;
                }
                
                uint16_t cX = data[offset + 1] | (data[offset + 2] << 8);
                uint16_t cY = data[offset + 3] | (data[offset + 4] << 8);
                
                if (cDown && (cX != 0 || cY != 0)) {
                    int slot = -1;
                    for (int j = 0; j < 10; j++) {
                        if (m_activeContacts[j].isValid && m_activeContacts[j].id == cId) {
                            slot = j;
                            break;
                        }
                    }
                    if (slot < 0) {
                        for (int j = 0; j < 10; j++) {
                            if (!m_activeContacts[j].isValid) {
                                slot = j;
                                break;
                            }
                        }
                    }
                    if (slot >= 0) {
                        TouchContact& contact = m_activeContacts[slot];
                        bool wasDown = contact.isValid && contact.id == cId;
                        
                        contact.id = cId;
                        contact.rawX = static_cast<float>(cX);
                        contact.rawY = static_cast<float>(cY);
                        
                        float rangeX = static_cast<float>(m_deviceInfo.maxX - m_deviceInfo.minX);
                        float rangeY = static_cast<float>(m_deviceInfo.maxY - m_deviceInfo.minY);
                        
                        contact.x = rangeX > 0 ? (contact.rawX - m_deviceInfo.minX) / rangeX : contact.rawX / 4096.0f;
                        contact.y = rangeY > 0 ? (contact.rawY - m_deviceInfo.minY) / rangeY : contact.rawY / 2400.0f;
                        contact.x = std::clamp(contact.x, 0.0f, 1.0f);
                        contact.y = std::clamp(contact.y, 0.0f, 1.0f);
                        
                        contact.pressure = 1.0f;
                        contact.timestamp = now;
                        contact.isValid = true;
                        contact.lastSeen = now;
                        contact.state = wasDown ? ContactState::Move : ContactState::Down;
                    }
                }
                
                offset += 5;  // 5 bytes per additional contact
            }
        }
        
        // Expire old contacts that haven't been seen recently
        for (int i = 0; i < 10; i++) {
            if (m_activeContacts[i].isValid) {
                auto age = std::chrono::duration<float>(now - m_activeContacts[i].lastSeen).count();
                if (age > 0.05f) {  // 50ms timeout
                    m_activeContacts[i].state = ContactState::Up;
                    m_activeContacts[i].isValid = false;
                }
            }
        }
        
        // Build frame from active contacts
        TouchFrame frame;
        frame.timestamp = now;
        frame.frameNumber = static_cast<uint32_t>(++m_frameCount);
        frame.hasClick = fingerDown;
        frame.activeCount = 0;
        
        for (int i = 0; i < 10; i++) {
            if (m_activeContacts[i].isValid) {
                frame.contacts[frame.activeCount++] = m_activeContacts[i];
            }
        }
        
        // Update current frame
        {
            std::lock_guard<std::mutex> lock(m_frameMutex);
            m_currentFrame = frame;
        }
        
        // Calculate input rate
        m_framesForRate++;
        auto elapsed = std::chrono::duration<double>(now - m_lastRateCalc).count();
        if (elapsed >= 1.0) {
            m_inputRate = m_framesForRate / elapsed;
            m_framesForRate = 0;
            m_lastRateCalc = now;
        }
        
        // Call frame callback
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            if (m_touchFrameCallback) {
                m_touchFrameCallback(frame);
            }
        }
        
        return;  // Done with raw parsing
    }
    
    // Fallback to standard HID parsing if preparsed data available
    if (!m_preparsedData) return;
    
    // Create frame for fallback parsing
    TouchFrame frame;
    frame.timestamp = now;
    frame.frameNumber = static_cast<uint32_t>(++m_frameCount);
    
    PHIDP_PREPARSED_DATA preparsed = static_cast<PHIDP_PREPARSED_DATA>(m_preparsedData);
    
    // Get contact count
    ULONG contactCount = 0;
    NTSTATUS status = HidP_GetUsageValue(
        HidP_Input,
        TOUCH_USAGE_PAGE_DIGITIZER,
        0,
        TOUCH_USAGE_DIGITIZER_CONTACT_COUNT,
        &contactCount,
        preparsed,
        const_cast<PCHAR>(reinterpret_cast<const char*>(data)),
        static_cast<ULONG>(size)
    );
    
    if (status != HIDP_STATUS_SUCCESS) {
        contactCount = 0;
    }
    
    frame.activeCount = std::min(contactCount, 10UL);
    
    // Get button state (click)
    ULONG usageLength = 1;
    USAGE usages[10];
    status = HidP_GetUsages(
        HidP_Input,
        TOUCH_USAGE_PAGE_DIGITIZER,
        0,
        usages,
        &usageLength,
        preparsed,
        const_cast<PCHAR>(reinterpret_cast<const char*>(data)),
        static_cast<ULONG>(size)
    );
    
    // Check for tip switch (physical click)
    for (ULONG i = 0; i < usageLength; i++) {
        if (usages[i] == TOUCH_USAGE_DIGITIZER_TIP_SWITCH) {
            frame.hasClick = true;
        }
    }
    
    // Also check button page for clicks
    usageLength = 10;
    status = HidP_GetUsages(
        HidP_Input,
        TOUCH_USAGE_PAGE_BUTTON,
        0,
        usages,
        &usageLength,
        preparsed,
        const_cast<PCHAR>(reinterpret_cast<const char*>(data)),
        static_cast<ULONG>(size)
    );
    
    if (status == HIDP_STATUS_SUCCESS && usageLength > 0) {
        frame.hasClick = true;
    }
    
    // Parse each contact (link collection)
    HIDP_CAPS caps;
    if (HidP_GetCaps(preparsed, &caps) != HIDP_STATUS_SUCCESS) return;
    
    // Get link collection nodes to find finger collections
    ULONG numNodes = caps.NumberLinkCollectionNodes;
    if (numNodes == 0) {
        // Fallback: try to parse single contact from root
        ULONG x = 0, y = 0, contactId = 0, pressure = 0;
        
        HidP_GetUsageValue(HidP_Input, TOUCH_USAGE_PAGE_GENERIC, 0, TOUCH_USAGE_GENERIC_X, &x, preparsed,
            const_cast<PCHAR>(reinterpret_cast<const char*>(data)), static_cast<ULONG>(size));
        HidP_GetUsageValue(HidP_Input, TOUCH_USAGE_PAGE_GENERIC, 0, TOUCH_USAGE_GENERIC_Y, &y, preparsed,
            const_cast<PCHAR>(reinterpret_cast<const char*>(data)), static_cast<ULONG>(size));
        HidP_GetUsageValue(HidP_Input, TOUCH_USAGE_PAGE_DIGITIZER, 0, TOUCH_USAGE_DIGITIZER_CONTACT_ID, &contactId, preparsed,
            const_cast<PCHAR>(reinterpret_cast<const char*>(data)), static_cast<ULONG>(size));
        HidP_GetUsageValue(HidP_Input, TOUCH_USAGE_PAGE_DIGITIZER, 0, TOUCH_USAGE_DIGITIZER_TIP_PRESSURE, &pressure, preparsed,
            const_cast<PCHAR>(reinterpret_cast<const char*>(data)), static_cast<ULONG>(size));
        
        if (frame.activeCount > 0 && (x != 0 || y != 0)) {
            TouchContact& contact = frame.contacts[0];
            contact.id = contactId;
            contact.rawX = static_cast<float>(x);
            contact.rawY = static_cast<float>(y);
            
            // Normalize coordinates
            float rangeX = static_cast<float>(m_deviceInfo.maxX - m_deviceInfo.minX);
            float rangeY = static_cast<float>(m_deviceInfo.maxY - m_deviceInfo.minY);
            contact.x = rangeX > 0 ? (contact.rawX - m_deviceInfo.minX) / rangeX : 0.0f;
            contact.y = rangeY > 0 ? (contact.rawY - m_deviceInfo.minY) / rangeY : 0.0f;
            
            // Normalize pressure
            if (m_deviceInfo.supportsPressure && m_deviceInfo.maxPressure > 0) {
                contact.pressure = static_cast<float>(pressure) / m_deviceInfo.maxPressure;
            } else {
                contact.pressure = 1.0f;
            }
            
            contact.timestamp = frame.timestamp;
            contact.isValid = true;
            
            // Determine state
            bool wasDown = m_previousContacts[0].isValid;
            contact.state = wasDown ? ContactState::Move : ContactState::Down;
            
            // Calculate velocity
            if (wasDown) {
                auto dt = std::chrono::duration<float>(frame.timestamp - m_previousContacts[0].timestamp).count();
                if (dt > 0) {
                    contact.deltaX = contact.x - m_previousContacts[0].x;
                    contact.deltaY = contact.y - m_previousContacts[0].y;
                    contact.velocityX = contact.deltaX / dt;
                    contact.velocityY = contact.deltaY / dt;
                }
            }
        }
    } else {
        // Parse using link collections for multi-touch
        auto* nodes = new HIDP_LINK_COLLECTION_NODE[numNodes];
        if (HidP_GetLinkCollectionNodes(nodes, &numNodes, preparsed) == HIDP_STATUS_SUCCESS) {
            uint32_t fingerIndex = 0;
            
            for (ULONG n = 0; n < numNodes && fingerIndex < frame.activeCount && fingerIndex < 10; n++) {
                // Look for finger collections
                if (nodes[n].LinkUsagePage == TOUCH_USAGE_PAGE_DIGITIZER &&
                    nodes[n].LinkUsage == TOUCH_USAGE_DIGITIZER_FINGER) {
                    
                    USHORT linkCollection = static_cast<USHORT>(n);
                    
                    // Check tip switch (finger touching)
                    ULONG tipUsageLength = 1;
                    USAGE tipUsage[1];
                    status = HidP_GetUsages(
                        HidP_Input,
                        TOUCH_USAGE_PAGE_DIGITIZER,
                        linkCollection,
                        tipUsage,
                        &tipUsageLength,
                        preparsed,
                        const_cast<PCHAR>(reinterpret_cast<const char*>(data)),
                        static_cast<ULONG>(size)
                    );
                    
                    bool fingerDown = false;
                    if (status == HIDP_STATUS_SUCCESS) {
                        for (ULONG u = 0; u < tipUsageLength; u++) {
                            if (tipUsage[u] == TOUCH_USAGE_DIGITIZER_TIP_SWITCH) {
                                fingerDown = true;
                                break;
                            }
                        }
                    }
                    
                    if (!fingerDown) {
                        fingerIndex++;
                        continue;
                    }
                    
                    ULONG x = 0, y = 0, contactId = 0, pressure = 0;
                    
                    HidP_GetUsageValue(HidP_Input, TOUCH_USAGE_PAGE_GENERIC, linkCollection, TOUCH_USAGE_GENERIC_X, &x, preparsed,
                        const_cast<PCHAR>(reinterpret_cast<const char*>(data)), static_cast<ULONG>(size));
                    HidP_GetUsageValue(HidP_Input, TOUCH_USAGE_PAGE_GENERIC, linkCollection, TOUCH_USAGE_GENERIC_Y, &y, preparsed,
                        const_cast<PCHAR>(reinterpret_cast<const char*>(data)), static_cast<ULONG>(size));
                    HidP_GetUsageValue(HidP_Input, TOUCH_USAGE_PAGE_DIGITIZER, linkCollection, TOUCH_USAGE_DIGITIZER_CONTACT_ID, &contactId, preparsed,
                        const_cast<PCHAR>(reinterpret_cast<const char*>(data)), static_cast<ULONG>(size));
                    HidP_GetUsageValue(HidP_Input, TOUCH_USAGE_PAGE_DIGITIZER, linkCollection, TOUCH_USAGE_DIGITIZER_TIP_PRESSURE, &pressure, preparsed,
                        const_cast<PCHAR>(reinterpret_cast<const char*>(data)), static_cast<ULONG>(size));
                    
                    TouchContact& contact = frame.contacts[fingerIndex];
                    contact.id = contactId;
                    contact.rawX = static_cast<float>(x);
                    contact.rawY = static_cast<float>(y);
                    
                    // Normalize coordinates
                    float rangeX = static_cast<float>(m_deviceInfo.maxX - m_deviceInfo.minX);
                    float rangeY = static_cast<float>(m_deviceInfo.maxY - m_deviceInfo.minY);
                    contact.x = rangeX > 0 ? (contact.rawX - m_deviceInfo.minX) / rangeX : 0.0f;
                    contact.y = rangeY > 0 ? (contact.rawY - m_deviceInfo.minY) / rangeY : 0.0f;
                    
                    // Clamp to valid range
                    contact.x = std::clamp(contact.x, 0.0f, 1.0f);
                    contact.y = std::clamp(contact.y, 0.0f, 1.0f);
                    
                    // Normalize pressure
                    if (m_deviceInfo.supportsPressure && m_deviceInfo.maxPressure > 0) {
                        contact.pressure = static_cast<float>(pressure) / m_deviceInfo.maxPressure;
                    } else {
                        contact.pressure = 1.0f;
                    }
                    
                    contact.timestamp = frame.timestamp;
                    contact.isValid = true;
                    
                    // Find matching previous contact by ID
                    TouchContact* prevContact = nullptr;
                    for (auto& prev : m_previousContacts) {
                        if (prev.isValid && prev.id == contact.id) {
                            prevContact = &prev;
                            break;
                        }
                    }
                    
                    // Determine state
                    if (prevContact) {
                        contact.state = ContactState::Move;
                        auto dt = std::chrono::duration<float>(frame.timestamp - prevContact->timestamp).count();
                        if (dt > 0) {
                            contact.deltaX = contact.x - prevContact->x;
                            contact.deltaY = contact.y - prevContact->y;
                            contact.velocityX = contact.deltaX / dt;
                            contact.velocityY = contact.deltaY / dt;
                        }
                    } else {
                        contact.state = ContactState::Down;
                    }
                    
                    fingerIndex++;
                }
            }
            
            // Mark lifted fingers
            for (auto& prev : m_previousContacts) {
                if (prev.isValid) {
                    bool stillActive = false;
                    for (uint32_t i = 0; i < frame.activeCount; i++) {
                        if (frame.contacts[i].isValid && frame.contacts[i].id == prev.id) {
                            stillActive = true;
                            break;
                        }
                    }
                    
                    if (!stillActive) {
                        // Create an up event
                        for (uint32_t i = 0; i < 10; i++) {
                            if (!frame.contacts[i].isValid) {
                                frame.contacts[i] = prev;
                                frame.contacts[i].state = ContactState::Up;
                                frame.contacts[i].timestamp = frame.timestamp;
                                break;
                            }
                        }
                    }
                }
            }
        }
        delete[] nodes;
    }
    
    // Update current frame
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_currentFrame = frame;
    }
    
    // Update previous contacts
    for (size_t i = 0; i < m_previousContacts.size(); i++) {
        m_previousContacts[i].isValid = false;
    }
    for (uint32_t i = 0; i < frame.activeCount && i < 10; i++) {
        if (frame.contacts[i].isValid && frame.contacts[i].state != ContactState::Up) {
            m_previousContacts[i] = frame.contacts[i];
        }
    }
    
    // Calculate input rate
    m_framesForRate++;
    auto elapsed = std::chrono::duration<double>(now - m_lastRateCalc).count();
    if (elapsed >= 1.0) {
        m_inputRate = m_framesForRate / elapsed;
        m_framesForRate = 0;
        m_lastRateCalc = now;
    }
    
    // Call frame callback
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        if (m_touchFrameCallback) {
            m_touchFrameCallback(frame);
        }
    }
}

void TouchpadInput::UpdateContactState(TouchContact& contact, bool isDown, float x, float y, float pressure) {
    // Not used in current implementation - kept for future expansion
}

} // namespace TouchpadTracker
