// Test mode - console application without GUI
// Prints raw touchpad packets for debugging

#include "TouchpadInput.h"
#include "DeviceEnumerator.h"
#include "Logger.h"

#include <iostream>
#include <iomanip>
#include <Windows.h>
#include <atomic>

using namespace TouchpadTracker;

std::atomic<bool> g_running{true};
HWND g_hwnd = nullptr;
TouchpadInput* g_input = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INPUT:
            if (g_input) {
                g_input->ProcessRawInput(lParam);
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            g_running = false;
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        std::cout << "\nStopping capture..." << std::endl;
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   Touchpad Test Mode" << std::endl;
    std::cout << "   Raw Packet Output" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    
    // Enumerate devices
    DeviceEnumerator enumerator;
    auto devices = enumerator.EnumerateTouchDevices();
    
    std::cout << "Found " << devices.size() << " touchpad device(s):" << std::endl;
    
    for (size_t i = 0; i < devices.size(); i++) {
        std::cout << "[" << i << "] ";
        enumerator.PrintDeviceInfo(devices[i]);
    }
    
    if (devices.empty()) {
        std::cout << "No touchpad devices found!" << std::endl;
        return 1;
    }
    
    // Create hidden window for raw input
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"TouchpadTestClass";
    
    RegisterClassExW(&wc);
    
    g_hwnd = CreateWindowExW(
        0, L"TouchpadTestClass", L"",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
    
    if (!g_hwnd) {
        std::cerr << "Failed to create message window" << std::endl;
        return 1;
    }
    
    // Initialize touchpad input
    TouchpadInput input;
    g_input = &input;
    
    if (!input.Initialize(devices[0])) {
        std::cerr << "Failed to initialize touchpad input" << std::endl;
        return 1;
    }
    
    input.SetWindowHandle(g_hwnd);
    
    // Set up callbacks
    input.SetTouchFrameCallback([](const TouchFrame& frame) {
        std::cout << "Frame #" << std::setw(6) << frame.frameNumber 
                  << " | Fingers: " << frame.activeCount;
        
        if (frame.hasClick) {
            std::cout << " | CLICK";
        }
        
        std::cout << std::endl;
        
        for (uint32_t i = 0; i < frame.activeCount && i < 10; i++) {
            const auto& c = frame.contacts[i];
            if (!c.isValid) continue;
            
            std::cout << "  F" << c.id 
                      << " | X: " << std::fixed << std::setprecision(4) << c.x
                      << " | Y: " << c.y
                      << " | P: " << std::setprecision(2) << c.pressure
                      << " | " << ContactStateToString(c.state)
                      << std::endl;
        }
    });
    
    input.SetRawDataCallback([](const uint8_t* data, size_t size) {
        std::cout << "  RAW [" << size << " bytes]: ";
        for (size_t i = 0; i < std::min(size, size_t(32)); i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(data[i]) << " ";
        }
        if (size > 32) std::cout << "...";
        std::cout << std::dec << std::setfill(' ') << std::endl;
    });
    
    // Start capture
    if (!input.StartCapture()) {
        std::cerr << "Failed to start capture" << std::endl;
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "Capturing touchpad input. Press Ctrl+C to stop." << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Message loop
    MSG msg = {};
    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        if (!g_running) break;
        
        Sleep(1); // Prevent busy loop
    }
    
    input.StopCapture();
    DestroyWindow(g_hwnd);
    
    std::cout << std::endl;
    std::cout << "Total frames captured: " << input.GetFrameCount() << std::endl;
    std::cout << "Done." << std::endl;
    
    return 0;
}
