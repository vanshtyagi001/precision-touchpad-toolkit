#pragma once

#include <Windows.h>
#include <shellapi.h>
#include <functional>

namespace TouchpadShortcuts {

// Menu item IDs
constexpr UINT ID_TRAY_EXIT = 1001;
constexpr UINT ID_TRAY_TOGGLE = 1002;
constexpr UINT ID_TRAY_ABOUT = 1003;

// Custom window message for tray icon
constexpr UINT WM_TRAYICON = WM_USER + 1;

class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();
    
    // Create the tray icon
    bool Create(HWND hwnd, HINSTANCE hInstance);
    
    // Remove the tray icon
    void Remove();
    
    // Update the icon/tooltip
    void SetEnabled(bool enabled);
    void SetTooltip(const wchar_t* tooltip);
    
    // Handle tray icon messages
    void HandleMessage(WPARAM wParam, LPARAM lParam);
    
    // Callbacks
    std::function<void()> onExit;
    std::function<void()> onToggle;
    
private:
    void ShowContextMenu();
    
    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    NOTIFYICONDATAW m_nid = {};
    bool m_created = false;
    bool m_enabled = true;
};

} // namespace TouchpadShortcuts
