#include "TrayIcon.h"
#include <string>

namespace TouchpadShortcuts {

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    Remove();
}

bool TrayIcon::Create(HWND hwnd, HINSTANCE hInstance) {
    m_hwnd = hwnd;
    m_hInstance = hInstance;
    
    // Initialize NOTIFYICONDATA
    m_nid = {};
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    
    // Use a system icon (keyboard icon)
    m_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    
    // Set tooltip
    wcscpy_s(m_nid.szTip, L"Touchpad Shortcuts - Active");
    
    if (Shell_NotifyIconW(NIM_ADD, &m_nid)) {
        m_created = true;
        return true;
    }
    
    return false;
}

void TrayIcon::Remove() {
    if (m_created) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_created = false;
    }
}

void TrayIcon::SetEnabled(bool enabled) {
    m_enabled = enabled;
    
    if (m_created) {
        wcscpy_s(m_nid.szTip, enabled ? 
            L"Touchpad Shortcuts - Active" : 
            L"Touchpad Shortcuts - Paused");
        m_nid.uFlags = NIF_TIP;
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }
}

void TrayIcon::SetTooltip(const wchar_t* tooltip) {
    if (m_created) {
        wcscpy_s(m_nid.szTip, tooltip);
        m_nid.uFlags = NIF_TIP;
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }
}

void TrayIcon::HandleMessage(WPARAM wParam, LPARAM lParam) {
    switch (LOWORD(lParam)) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowContextMenu();
            break;
            
        case WM_LBUTTONDBLCLK:
            // Double-click toggles on/off
            if (onToggle) onToggle();
            break;
    }
}

void TrayIcon::ShowContextMenu() {
    POINT pt;
    GetCursorPos(&pt);
    
    HMENU hMenu = CreatePopupMenu();
    
    if (m_enabled) {
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_TOGGLE, L"Pause Shortcuts");
    } else {
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_TOGGLE, L"Resume Shortcuts");
    }
    
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_ABOUT, L"About");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
    
    // Required to make menu work properly
    SetForegroundWindow(m_hwnd);
    
    UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_hwnd, nullptr);
    
    DestroyMenu(hMenu);
    
    switch (cmd) {
        case ID_TRAY_EXIT:
            if (onExit) onExit();
            break;
        case ID_TRAY_TOGGLE:
            if (onToggle) onToggle();
            break;
        case ID_TRAY_ABOUT:
            MessageBoxW(m_hwnd, 
                L"Touchpad Shortcuts v1.0\n\n"
                L"Zone Actions:\n"
                L"• Top-Left: Volume Down\n"
                L"• Top-Right: Volume Up\n"
                L"• Bottom-Left: Brightness Down\n"
                L"• Bottom-Right: Brightness Up\n\n"
                L"Tap the corners of your touchpad to trigger actions!",
                L"About Touchpad Shortcuts",
                MB_OK | MB_ICONINFORMATION);
            break;
    }
}

} // namespace TouchpadShortcuts
