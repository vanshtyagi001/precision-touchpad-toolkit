// Touchpad Shortcuts - Zone-based touchpad shortcuts
// Runs in background with system tray icon

#include "TouchpadZones.h"
#include "SystemActions.h"
#include "TrayIcon.h"

#include <Windows.h>
#include <iostream>

using namespace TouchpadShortcuts;

// Global instances
static TouchpadZones* g_touchpad = nullptr;
static TrayIcon* g_trayIcon = nullptr;
static bool g_enabled = true;
static bool g_running = true;

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INPUT: {
            if (g_enabled && g_touchpad) {
                g_touchpad->ProcessRawInput(lParam);
            }
            break;
        }
        
        case WM_TRAYICON: {
            if (g_trayIcon) {
                g_trayIcon->HandleMessage(wParam, lParam);
            }
            break;
        }
        
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Create hidden message window
HWND CreateMessageWindow(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TouchpadShortcutsClass";
    
    RegisterClassExW(&wc);
    
    return CreateWindowExW(
        0,
        L"TouchpadShortcutsClass",
        L"Touchpad Shortcuts",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,  // Message-only window
        nullptr,
        hInstance,
        nullptr
    );
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Allow only one instance
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"TouchpadShortcutsMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Touchpad Shortcuts is already running!", L"Info", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    
    // Create message window
    HWND hwnd = CreateMessageWindow(hInstance);
    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create message window!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Initialize touchpad
    g_touchpad = new TouchpadZones();
    if (!g_touchpad->Initialize()) {
        MessageBoxW(nullptr, L"No touchpad device found!", L"Error", MB_OK | MB_ICONERROR);
        delete g_touchpad;
        return 1;
    }
    
    // Set tap callback
    g_touchpad->SetTapCallback([](Zone zone) {
        if (g_enabled) {
            // Execute the action for this zone
            SystemActions::ExecuteZoneAction(zone);
        }
    });
    
    // Start touchpad monitoring
    if (!g_touchpad->Start(hwnd)) {
        MessageBoxW(nullptr, L"Failed to start touchpad monitoring!", L"Error", MB_OK | MB_ICONERROR);
        delete g_touchpad;
        return 1;
    }
    
    // Create tray icon
    g_trayIcon = new TrayIcon();
    if (!g_trayIcon->Create(hwnd, hInstance)) {
        MessageBoxW(nullptr, L"Failed to create tray icon!", L"Error", MB_OK | MB_ICONERROR);
        delete g_touchpad;
        delete g_trayIcon;
        return 1;
    }
    
    // Set tray icon callbacks
    g_trayIcon->onExit = []() {
        g_running = false;
        PostQuitMessage(0);
    };
    
    g_trayIcon->onToggle = []() {
        g_enabled = !g_enabled;
        g_trayIcon->SetEnabled(g_enabled);
    };
    
    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Cleanup
    g_touchpad->Stop();
    delete g_touchpad;
    delete g_trayIcon;
    
    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    
    return 0;
}
