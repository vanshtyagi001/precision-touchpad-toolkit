#include "SystemActions.h"

#define NOMINMAX
#include <Windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <algorithm>
#include <string>

#pragma comment(lib, "ole32.lib")

namespace TouchpadShortcuts {

// Get the directory where the exe is located
static std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring exePath(path);
    size_t pos = exePath.find_last_of(L"\\/");
    return exePath.substr(0, pos);
}

// Run a PowerShell script file
static void RunPowerShellScript(const wchar_t* scriptName) {
    std::wstring exeDir = GetExeDirectory();
    
    // Try scripts folder relative to exe first, then relative to source
    std::wstring scriptPath = exeDir + L"\\..\\..\\..\\scripts\\" + scriptName;
    
    std::wstring cmd = L"powershell.exe -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + scriptPath + L"\"";
    
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    
    if (CreateProcessW(nullptr, const_cast<LPWSTR>(cmd.c_str()), 
                       nullptr, nullptr, FALSE, CREATE_NO_WINDOW, 
                       nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// Helper to send media keys
static void SendMediaKey(WORD vkCode) {
    INPUT inputs[2] = {};
    
    // Key down
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vkCode;
    inputs[0].ki.dwFlags = 0;
    
    // Key up
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vkCode;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    
    SendInput(2, inputs, sizeof(INPUT));
}

void SystemActions::VolumeUp() {
    // Use VK_VOLUME_UP - simulates media key
    SendMediaKey(VK_VOLUME_UP);
}

void SystemActions::VolumeDown() {
    // Use VK_VOLUME_DOWN - simulates media key
    SendMediaKey(VK_VOLUME_DOWN);
}

void SystemActions::VolumeMute() {
    SendMediaKey(VK_VOLUME_MUTE);
}

void SystemActions::BrightnessUp() {
    RunPowerShellScript(L"brightness_up.ps1");
}

void SystemActions::BrightnessDown() {
    RunPowerShellScript(L"brightness_down.ps1");
}

void SystemActions::ExecuteZoneAction(Zone zone) {
    switch (zone) {
        case Zone::TopLeft:
            BrightnessDown();
            break;
        case Zone::TopRight:
            BrightnessUp();
            break;
        case Zone::BottomLeft:
            VolumeDown();
            break;
        case Zone::BottomRight:
            VolumeUp();
            break;
        default:
            break;
    }
}

void SystemActions::ShowNotification(const char* title, const char* message) {
    // Could implement balloon notification here if desired
    // For now, we rely on Windows OSD for volume/brightness
}

} // namespace TouchpadShortcuts
