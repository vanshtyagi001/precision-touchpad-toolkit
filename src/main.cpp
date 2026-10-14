#include "TouchpadGUI.h"
#include <iostream>
#include <Windows.h>

// Entry point for Windows GUI application
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Console app version - no need to allocate console
    std::cout << "========================================" << std::endl;
    std::cout << "   Touchpad Tracker v1.0" << std::endl;
    std::cout << "   Raw Input Visualization Tool" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Initializing..." << std::endl;
    std::cout.flush();
    
    try {
        TouchpadTracker::TouchpadGUI app;
        
        std::cout << "Created TouchpadGUI instance, calling Initialize()..." << std::endl;
        std::cout.flush();
        
        if (!app.Initialize()) {
            std::cerr << "Failed to initialize application!" << std::endl;
            MessageBoxW(nullptr, L"Failed to initialize application.\nCheck console for details.", 
                        L"Error", MB_ICONERROR | MB_OK);
            std::cout << "Press Enter to exit..." << std::endl;
            std::cin.get();
            return 1;
        }
        
        std::cout << "Application initialized. Starting main loop..." << std::endl;
        std::cout << "Click 'Start Capture' to begin tracking touchpad input." << std::endl;
        std::cout << std::endl;
        std::cout.flush();
        
        app.Run();
        
        std::cout << "Application shutting down..." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        MessageBoxA(nullptr, e.what(), "Error", MB_ICONERROR | MB_OK);
        std::cin.get();
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception!" << std::endl;
        MessageBoxW(nullptr, L"Unknown exception!", L"Error", MB_ICONERROR | MB_OK);
        std::cin.get();
        return 1;
    }
    
    return 0;
}

// Also support console mode entry point
int main(int argc, char* argv[]) {
    std::cout << "main() called" << std::endl;
    return WinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineA(), SW_SHOW);
}
