#include "TouchpadGUI.h"

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

// Forward declare ImGui Win32 handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace TouchpadTracker {

// Static instance for window procedure
static TouchpadGUI* g_instance = nullptr;

TouchpadGUI::TouchpadGUI() {
    g_instance = this;
    m_lastFrameTime = Clock::now();
    
    // Initialize heatmap
    m_heatmap.resize(HEATMAP_RESOLUTION);
    for (auto& row : m_heatmap) {
        row.resize(HEATMAP_RESOLUTION);
        for (auto& cell : row) {
            cell.intensity = 0;
        }
    }
}

TouchpadGUI::~TouchpadGUI() {
    Shutdown();
    g_instance = nullptr;
}

bool TouchpadGUI::Initialize() {
    std::cout << "Initialize() started" << std::endl;
    std::cout.flush();
    
    // Create components
    m_touchpadInput = std::make_unique<TouchpadInput>();
    m_gestureRecognizer = std::make_unique<GestureRecognizer>();
    m_logger = std::make_unique<Logger>();
    
    std::cout << "Components created" << std::endl;
    std::cout.flush();
    
    // Initialize logger
    if (!m_logger->Initialize("logs")) {
        std::cerr << "Warning: Failed to initialize logger" << std::endl;
    }
    
    std::cout << "Logger initialized, creating window..." << std::endl;
    std::cout.flush();
    
    // Create window
    if (!CreateAppWindow()) {
        std::cerr << "Failed to create application window" << std::endl;
        return false;
    }
    
    std::cout << "Window created, initializing OpenGL..." << std::endl;
    std::cout.flush();
    
    // Initialize OpenGL
    if (!InitializeOpenGL()) {
        std::cerr << "Failed to initialize OpenGL" << std::endl;
        return false;
    }
    
    std::cout << "OpenGL initialized, initializing ImGui..." << std::endl;
    std::cout.flush();
    
    // Initialize ImGui
    if (!InitializeImGui()) {
        std::cerr << "Failed to initialize ImGui" << std::endl;
        return false;
    }
    
    std::cout << "ImGui initialized, initializing touchpad..." << std::endl;
    std::cout.flush();
    
    // Initialize touchpad
    if (!m_touchpadInput->InitializeAutoDetect()) {
        std::cerr << "Warning: Could not auto-detect touchpad" << std::endl;
    }
    
    m_touchpadInput->SetWindowHandle(m_hwnd);
    
    // Set callbacks
    m_touchpadInput->SetTouchFrameCallback([this](const TouchFrame& frame) {
        OnTouchFrame(frame);
    });
    
    m_gestureRecognizer->SetGestureCallback([this](const GestureEvent& gesture) {
        OnGesture(gesture);
    });
    
    std::cout << "TouchpadGUI initialized successfully" << std::endl;
    std::cout.flush();
    return true;
}

bool TouchpadGUI::CreateAppWindow() {
    std::cout << "CreateAppWindow() - registering class..." << std::endl;
    std::cout.flush();
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"TouchpadTrackerClass";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    
    if (!RegisterClassExW(&wc)) {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            std::cerr << "Failed to register window class. Error: " << error << std::endl;
            return false;
        }
        std::cout << "Window class already registered (continuing)" << std::endl;
    }
    
    std::cout << "CreateAppWindow() - calculating position..." << std::endl;
    std::cout.flush();
    
    // Calculate window position (center of screen)
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - m_windowWidth) / 2;
    int posY = (screenHeight - m_windowHeight) / 2;
    
    RECT wr = {0, 0, m_windowWidth, m_windowHeight};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    
    std::cout << "CreateAppWindow() - creating window..." << std::endl;
    std::cout.flush();
    
    m_hwnd = CreateWindowExW(
        0,
        L"TouchpadTrackerClass",
        L"Touchpad Tracker - Raw Input Visualization",
        WS_OVERLAPPEDWINDOW,
        posX, posY,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    if (!m_hwnd) {
        std::cerr << "Failed to create window. Error: " << GetLastError() << std::endl;
        return false;
    }
    
    std::cout << "CreateAppWindow() - showing window..." << std::endl;
    std::cout.flush();
    
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    
    std::cout << "CreateAppWindow() - complete" << std::endl;
    std::cout.flush();
    
    return true;
}

bool TouchpadGUI::InitializeOpenGL() {
    m_hdc = GetDC(m_hwnd);
    
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    
    int pixelFormat = ChoosePixelFormat(m_hdc, &pfd);
    if (pixelFormat == 0) {
        std::cerr << "Failed to choose pixel format" << std::endl;
        return false;
    }
    
    if (!SetPixelFormat(m_hdc, pixelFormat, &pfd)) {
        std::cerr << "Failed to set pixel format" << std::endl;
        return false;
    }
    
    m_hglrc = wglCreateContext(m_hdc);
    if (!m_hglrc) {
        std::cerr << "Failed to create OpenGL context" << std::endl;
        return false;
    }
    
    if (!wglMakeCurrent(m_hdc, m_hglrc)) {
        std::cerr << "Failed to make OpenGL context current" << std::endl;
        return false;
    }
    
    // Load OpenGL functions
    if (!gladLoadGL()) {
        std::cerr << "Failed to load OpenGL functions via gladLoadGL()" << std::endl;
        // Try alternative: gladLoadGLLoader
        return false;
    }
    
    const GLubyte* version = glGetString(GL_VERSION);
    if (version) {
        std::cout << "OpenGL Version: " << version << std::endl;
    } else {
        std::cout << "OpenGL Version: (unknown - glGetString returned null)" << std::endl;
    }
    
    return true;
}

bool TouchpadGUI::InitializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Set style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    
    // Initialize ImGui backends
    if (!ImGui_ImplWin32_Init(m_hwnd)) {
        std::cerr << "Failed to initialize ImGui Win32" << std::endl;
        return false;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        std::cerr << "Failed to initialize ImGui OpenGL3" << std::endl;
        return false;
    }
    
    m_imguiInitialized = true;
    
    return true;
}

LRESULT CALLBACK TouchpadGUI::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Only forward to ImGui if it's initialized
    if (g_instance && g_instance->m_imguiInitialized) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
            return true;
        }
    }
    
    switch (msg) {
        case WM_INPUT: {
            if (g_instance && g_instance->m_touchpadInput && g_instance->m_capturing) {
                g_instance->m_touchpadInput->ProcessRawInput(lParam);
            }
            break;
        }
        
        case WM_SIZE: {
            if (g_instance && g_instance->m_imguiInitialized) {
                g_instance->m_windowWidth = LOWORD(lParam);
                g_instance->m_windowHeight = HIWORD(lParam);
                glViewport(0, 0, g_instance->m_windowWidth, g_instance->m_windowHeight);
            }
            break;
        }
        
        case WM_CLOSE:
            if (g_instance) {
                g_instance->m_running = false;
            }
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void TouchpadGUI::Run() {
    std::cout << "Run() called, entering main loop..." << std::endl;
    std::cout.flush();
    
    m_running = true;
    
    MSG msg = {};
    int loopCount = 0;
    while (m_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                std::cout << "Got WM_QUIT message" << std::endl;
                m_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        if (!m_running) {
            std::cout << "m_running set to false, exiting loop" << std::endl;
            break;
        }
        
        if (loopCount == 0) {
            std::cout << "First frame rendering..." << std::endl;
            std::cout.flush();
        }
        loopCount++;
        
        Update();
        Render();
        
        // VSync / frame limiting
        SwapBuffers(m_hdc);
        
        // Calculate FPS
        m_frameCount++;
        auto now = Clock::now();
        auto elapsed = std::chrono::duration<double>(now - m_lastFrameTime).count();
        if (elapsed >= 1.0) {
            m_fps = m_frameCount / elapsed;
            m_frameCount = 0;
            m_lastFrameTime = now;
            m_inputRate = m_touchpadInput ? m_touchpadInput->GetInputRate() : 0;
        }
    }
    
    std::cout << "Exited main loop after " << loopCount << " frames" << std::endl;
}

void TouchpadGUI::Shutdown() {
    if (m_capturing && m_touchpadInput) {
        m_touchpadInput->StopCapture();
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    if (m_hglrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_hglrc);
        m_hglrc = nullptr;
    }
    
    if (m_hdc && m_hwnd) {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = nullptr;
    }
    
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    
    UnregisterClassW(L"TouchpadTrackerClass", GetModuleHandle(nullptr));
}

void TouchpadGUI::Update() {
    // Update trails fade
    auto now = Clock::now();
    for (auto& trail : m_fingerTrails) {
        for (auto& point : trail) {
            float age = std::chrono::duration<float>(now - point.timestamp).count();
            point.alpha = std::max(0.0f, 1.0f - (age / m_trailFadeTime));
        }
        // Remove fully faded points
        trail.erase(
            std::remove_if(trail.begin(), trail.end(), 
                [](const TrailPoint& p) { return p.alpha <= 0; }),
            trail.end());
    }
    
    // Fade heatmap
    for (auto& row : m_heatmap) {
        for (auto& cell : row) {
            float age = std::chrono::duration<float>(now - cell.lastUpdate).count();
            if (age > 0.1f) {
                cell.intensity *= 0.98f;
            }
        }
    }
}

void TouchpadGUI::Render() {
    static bool firstRender = true;
    if (firstRender) {
        std::cout << "Render() first call" << std::endl;
        std::cout.flush();
        firstRender = false;
    }
    
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    // Main touchpad visualization window
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(580, 450), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Touchpad Visualization", nullptr, ImGuiWindowFlags_NoCollapse)) {
        RenderTouchpadVisualization();
    }
    ImGui::End();
    
    // Numeric data window
    ImGui::SetNextWindowPos(ImVec2(600, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 250), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Touch Data", nullptr)) {
        RenderNumericDisplay();
    }
    ImGui::End();
    
    // Gesture display window
    ImGui::SetNextWindowPos(ImVec2(600, 270), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 200), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Gestures", nullptr)) {
        RenderGestureDisplay();
    }
    ImGui::End();
    
    // Controls window
    ImGui::SetNextWindowPos(ImVec2(10, 470), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 240), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Controls", nullptr)) {
        RenderControls();
    }
    ImGui::End();
    
    // Device info window
    ImGui::SetNextWindowPos(ImVec2(320, 470), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(270, 240), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Device Info", nullptr)) {
        RenderDeviceInfo();
    }
    ImGui::End();
    
    // Log display window
    ImGui::SetNextWindowPos(ImVec2(600, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 230), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Log", nullptr)) {
        RenderLogDisplay();
    }
    ImGui::End();
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void TouchpadGUI::RenderTouchpadVisualization() {
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 contentPos = ImGui::GetCursorScreenPos();
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    
    // Calculate touchpad display area (maintain aspect ratio)
    float aspectRatio = 1.4f; // Typical touchpad aspect ratio
    float displayWidth = contentSize.x - 20;
    float displayHeight = displayWidth / aspectRatio;
    
    if (displayHeight > contentSize.y - 30) {
        displayHeight = contentSize.y - 30;
        displayWidth = displayHeight * aspectRatio;
    }
    
    float offsetX = (contentSize.x - displayWidth) / 2;
    float offsetY = 10;
    
    m_touchpadDisplayX = contentPos.x + offsetX;
    m_touchpadDisplayY = contentPos.y + offsetY;
    m_touchpadDisplayWidth = displayWidth;
    m_touchpadDisplayHeight = displayHeight;
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Draw touchpad background
    drawList->AddRectFilled(
        ImVec2(m_touchpadDisplayX, m_touchpadDisplayY),
        ImVec2(m_touchpadDisplayX + m_touchpadDisplayWidth, m_touchpadDisplayY + m_touchpadDisplayHeight),
        IM_COL32(40, 40, 50, 255),
        5.0f);
    
    // Draw border
    drawList->AddRect(
        ImVec2(m_touchpadDisplayX, m_touchpadDisplayY),
        ImVec2(m_touchpadDisplayX + m_touchpadDisplayWidth, m_touchpadDisplayY + m_touchpadDisplayHeight),
        IM_COL32(100, 100, 120, 255),
        5.0f, 0, 2.0f);
    
    // Draw grid
    int gridLines = 8;
    for (int i = 1; i < gridLines; i++) {
        float x = m_touchpadDisplayX + (m_touchpadDisplayWidth * i / gridLines);
        float y = m_touchpadDisplayY + (m_touchpadDisplayHeight * i / gridLines);
        
        drawList->AddLine(
            ImVec2(x, m_touchpadDisplayY),
            ImVec2(x, m_touchpadDisplayY + m_touchpadDisplayHeight),
            IM_COL32(60, 60, 70, 100));
        
        drawList->AddLine(
            ImVec2(m_touchpadDisplayX, y),
            ImVec2(m_touchpadDisplayX + m_touchpadDisplayWidth, y),
            IM_COL32(60, 60, 70, 100));
    }
    
    // Render layers
    if (m_showHeatmap) {
        RenderHeatmap();
    }
    
    if (m_showTrails) {
        RenderTrails();
    }
    
    RenderFingerMarkers();
    
    // Status indicator
    const char* status = m_capturing ? "CAPTURING" : "STOPPED";
    ImU32 statusColor = m_capturing ? IM_COL32(0, 255, 100, 255) : IM_COL32(255, 100, 100, 255);
    
    drawList->AddText(
        ImVec2(m_touchpadDisplayX + 5, m_touchpadDisplayY + m_touchpadDisplayHeight + 5),
        statusColor, status);
    
    // FPS display
    char fpsText[64];
    snprintf(fpsText, sizeof(fpsText), "FPS: %.1f | Input: %.0f Hz", m_fps, m_inputRate);
    drawList->AddText(
        ImVec2(m_touchpadDisplayX + m_touchpadDisplayWidth - 180, m_touchpadDisplayY + m_touchpadDisplayHeight + 5),
        IM_COL32(180, 180, 180, 255), fpsText);
}

void TouchpadGUI::RenderHeatmap() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    float cellWidth = m_touchpadDisplayWidth / HEATMAP_RESOLUTION;
    float cellHeight = m_touchpadDisplayHeight / HEATMAP_RESOLUTION;
    
    for (int y = 0; y < HEATMAP_RESOLUTION; y++) {
        for (int x = 0; x < HEATMAP_RESOLUTION; x++) {
            float intensity = m_heatmap[y][x].intensity;
            if (intensity < 0.01f) continue;
            
            intensity = std::min(1.0f, intensity);
            
            // Color gradient from blue to red
            uint8_t r = static_cast<uint8_t>(255 * intensity);
            uint8_t g = static_cast<uint8_t>(100 * (1.0f - intensity));
            uint8_t b = static_cast<uint8_t>(255 * (1.0f - intensity));
            uint8_t a = static_cast<uint8_t>(150 * intensity);
            
            float px = m_touchpadDisplayX + x * cellWidth;
            float py = m_touchpadDisplayY + y * cellHeight;
            
            drawList->AddRectFilled(
                ImVec2(px, py),
                ImVec2(px + cellWidth, py + cellHeight),
                IM_COL32(r, g, b, a));
        }
    }
}

void TouchpadGUI::RenderTrails() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    for (const auto& trail : m_fingerTrails) {
        if (trail.size() < 2) continue;
        
        for (size_t i = 1; i < trail.size(); i++) {
            const auto& p1 = trail[i - 1];
            const auto& p2 = trail[i];
            
            if (p1.alpha <= 0 || p2.alpha <= 0) continue;
            
            float x1 = m_touchpadDisplayX + p1.x * m_touchpadDisplayWidth;
            float y1 = m_touchpadDisplayY + p1.y * m_touchpadDisplayHeight;
            float x2 = m_touchpadDisplayX + p2.x * m_touchpadDisplayWidth;
            float y2 = m_touchpadDisplayY + p2.y * m_touchpadDisplayHeight;
            
            uint8_t alpha = static_cast<uint8_t>(200 * std::min(p1.alpha, p2.alpha));
            float thickness = 2.0f + 4.0f * std::min(p1.alpha, p2.alpha);
            
            drawList->AddLine(
                ImVec2(x1, y1), ImVec2(x2, y2),
                IM_COL32(100, 200, 255, alpha), thickness);
        }
    }
}

void TouchpadGUI::RenderFingerMarkers() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Get time for pulsing animation
    float time = std::chrono::duration<float>(Clock::now().time_since_epoch()).count();
    float pulse = 0.5f + 0.5f * std::sin(time * 5.0f);  // Pulse at 5Hz
    
    // Colors for different fingers - bright and visible
    static const ImU32 fingerColors[] = {
        IM_COL32(0, 220, 255, 255),    // Cyan
        IM_COL32(255, 80, 80, 255),    // Red
        IM_COL32(80, 255, 80, 255),    // Green
        IM_COL32(255, 220, 0, 255),    // Yellow
        IM_COL32(220, 80, 255, 255),   // Purple
        IM_COL32(255, 160, 80, 255),   // Orange
        IM_COL32(80, 255, 220, 255),   // Teal
        IM_COL32(255, 80, 220, 255),   // Pink
        IM_COL32(160, 160, 255, 255),  // Light blue
        IM_COL32(220, 220, 220, 255),  // Gray
    };
    
    // Draw "no touch" indicator if no fingers active
    if (m_currentFrame.activeCount == 0) {
        const char* noTouch = "Touch the trackpad";
        ImVec2 textSize = ImGui::CalcTextSize(noTouch);
        float textX = m_touchpadDisplayX + (m_touchpadDisplayWidth - textSize.x) / 2;
        float textY = m_touchpadDisplayY + (m_touchpadDisplayHeight - textSize.y) / 2;
        
        uint8_t alpha = static_cast<uint8_t>(100 + 50 * pulse);
        drawList->AddText(ImVec2(textX, textY), IM_COL32(150, 150, 180, alpha), noTouch);
    }
    
    for (uint32_t i = 0; i < m_currentFrame.activeCount && i < 10; i++) {
        const auto& contact = m_currentFrame.contacts[i];
        if (!contact.isValid || contact.state == ContactState::Up) continue;
        
        float x = m_touchpadDisplayX + contact.x * m_touchpadDisplayWidth;
        float y = m_touchpadDisplayY + contact.y * m_touchpadDisplayHeight;
        
        // Larger base radius for better visibility
        float baseRadius = 25.0f + 15.0f * contact.pressure;
        float radius = baseRadius + 3.0f * pulse;  // Pulsing effect
        ImU32 color = fingerColors[i % 10];
        
        // Extract color components
        uint8_t r = (color >> 0) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = (color >> 16) & 0xFF;
        
        // Draw outer glow (multiple layers for softer effect)
        for (int glow = 4; glow >= 1; glow--) {
            float glowRadius = radius + glow * 8;
            uint8_t glowAlpha = static_cast<uint8_t>(40 + 20 * pulse) / glow;
            drawList->AddCircleFilled(ImVec2(x, y), glowRadius, 
                IM_COL32(r, g, b, glowAlpha), 32);
        }
        
        // Draw velocity trail (direction indicator)
        if (contact.velocityX != 0 || contact.velocityY != 0) {
            float speed = std::sqrt(contact.velocityX * contact.velocityX + 
                                   contact.velocityY * contact.velocityY);
            if (speed > 0.1f) {
                float trailLen = std::min(speed * 30.0f, 50.0f);
                float dx = -contact.velocityX / speed * trailLen;
                float dy = -contact.velocityY / speed * trailLen;
                
                drawList->AddLine(ImVec2(x, y), ImVec2(x + dx, y + dy),
                    IM_COL32(r, g, b, 150), 4.0f);
            }
        }
        
        // Draw main circle with gradient effect (inner brighter)
        drawList->AddCircleFilled(ImVec2(x, y), radius, color, 48);
        drawList->AddCircleFilled(ImVec2(x, y), radius * 0.7f, 
            IM_COL32(std::min(255, r + 50), std::min(255, g + 50), std::min(255, b + 50), 255), 32);
        
        // Draw white center dot
        drawList->AddCircleFilled(ImVec2(x, y), 6, IM_COL32(255, 255, 255, 255), 16);
        
        // Draw crosshair for precision
        float crossSize = radius + 10;
        drawList->AddLine(ImVec2(x - crossSize, y), ImVec2(x - radius - 5, y), 
            IM_COL32(r, g, b, 150), 2.0f);
        drawList->AddLine(ImVec2(x + radius + 5, y), ImVec2(x + crossSize, y), 
            IM_COL32(r, g, b, 150), 2.0f);
        drawList->AddLine(ImVec2(x, y - crossSize), ImVec2(x, y - radius - 5), 
            IM_COL32(r, g, b, 150), 2.0f);
        drawList->AddLine(ImVec2(x, y + radius + 5), ImVec2(x, y + crossSize), 
            IM_COL32(r, g, b, 150), 2.0f);
        
        // Draw finger number label
        char idText[16];
        snprintf(idText, sizeof(idText), "F%u", i + 1);
        ImVec2 textSize = ImGui::CalcTextSize(idText);
        drawList->AddRectFilled(
            ImVec2(x - textSize.x/2 - 4, y - radius - 25),
            ImVec2(x + textSize.x/2 + 4, y - radius - 8),
            IM_COL32(30, 30, 40, 220), 3.0f);
        drawList->AddText(ImVec2(x - textSize.x/2, y - radius - 23), 
            IM_COL32(255, 255, 255, 255), idText);
        
        // Draw coordinates below finger
        char coordText[32];
        snprintf(coordText, sizeof(coordText), "%.2f, %.2f", contact.x, contact.y);
        ImVec2 coordSize = ImGui::CalcTextSize(coordText);
        drawList->AddText(ImVec2(x - coordSize.x/2, y + radius + 10), 
            IM_COL32(200, 200, 220, 200), coordText);
    }
    
    // Draw finger count indicator
    if (m_currentFrame.activeCount > 0) {
        char countText[32];
        snprintf(countText, sizeof(countText), "%u finger%s", 
            m_currentFrame.activeCount,
            m_currentFrame.activeCount > 1 ? "s" : "");
        
        ImVec2 countPos(m_touchpadDisplayX + 10, m_touchpadDisplayY + 10);
        drawList->AddRectFilled(
            ImVec2(countPos.x - 5, countPos.y - 3),
            ImVec2(countPos.x + ImGui::CalcTextSize(countText).x + 5, countPos.y + 18),
            IM_COL32(0, 180, 100, 200), 3.0f);
        drawList->AddText(countPos, IM_COL32(255, 255, 255, 255), countText);
    }
    
    // Draw click indicator
    if (m_currentFrame.hasClick) {
        // Glowing top bar
        for (int i = 0; i < 3; i++) {
            uint8_t alpha = static_cast<uint8_t>(200 - i * 50);
            drawList->AddRectFilled(
                ImVec2(m_touchpadDisplayX, m_touchpadDisplayY + i * 2),
                ImVec2(m_touchpadDisplayX + m_touchpadDisplayWidth, m_touchpadDisplayY + 5 + i * 2),
                IM_COL32(255, 200, 0, alpha));
        }
        
        // Click label
        const char* clickText = "CLICK";
        ImVec2 clickSize = ImGui::CalcTextSize(clickText);
        float clickX = m_touchpadDisplayX + m_touchpadDisplayWidth - clickSize.x - 10;
        drawList->AddRectFilled(
            ImVec2(clickX - 5, m_touchpadDisplayY + 10),
            ImVec2(clickX + clickSize.x + 5, m_touchpadDisplayY + 28),
            IM_COL32(255, 180, 0, 230), 3.0f);
        drawList->AddText(ImVec2(clickX, m_touchpadDisplayY + 12), 
            IM_COL32(0, 0, 0, 255), clickText);
    }
}

void TouchpadGUI::RenderNumericDisplay() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    ImGui::Text("Active Fingers: %u", m_currentFrame.activeCount);
    ImGui::Text("Physical Click: %s", m_currentFrame.hasClick ? "YES" : "No");
    ImGui::Text("Frame #: %u", m_currentFrame.frameNumber);
    
    ImGui::Separator();
    
    for (uint32_t i = 0; i < m_currentFrame.activeCount && i < 10; i++) {
        const auto& contact = m_currentFrame.contacts[i];
        if (!contact.isValid) continue;
        
        ImGui::PushID(i);
        
        ImVec4 color;
        switch (i % 4) {
            case 0: color = ImVec4(0, 0.8f, 1.0f, 1.0f); break;
            case 1: color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); break;
            case 2: color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); break;
            case 3: color = ImVec4(1.0f, 0.8f, 0, 1.0f); break;
        }
        
        ImGui::TextColored(color, "Finger %u (ID: %u)", i, contact.id);
        ImGui::Text("  Position: (%.4f, %.4f)", contact.x, contact.y);
        ImGui::Text("  Raw: (%.0f, %.0f)", contact.rawX, contact.rawY);
        ImGui::Text("  Pressure: %.2f", contact.pressure);
        ImGui::Text("  State: %s", ContactStateToString(contact.state).c_str());
        ImGui::Text("  Velocity: (%.2f, %.2f)", contact.velocityX, contact.velocityY);
        
        ImGui::PopID();
    }
}

void TouchpadGUI::RenderGestureDisplay() {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    // Current gesture
    ImGui::Text("Current Gesture:");
    ImGui::Separator();
    
    if (m_currentGesture.type != GestureType::None || m_currentGesture.isActive) {
        ImVec4 activeColor = m_currentGesture.isActive ? 
            ImVec4(0, 1, 0.5f, 1) : ImVec4(0.7f, 0.7f, 0.7f, 1);
        
        ImGui::TextColored(activeColor, "%s", GestureTypeToString(m_currentGesture.type).c_str());
        ImGui::Text("  Fingers: %u", m_currentGesture.fingerCount);
        ImGui::Text("  Direction: %s", DirectionToString(m_currentGesture.direction).c_str());
        ImGui::Text("  Center: (%.3f, %.3f)", m_currentGesture.centerX, m_currentGesture.centerY);
        ImGui::Text("  Magnitude: %.4f", m_currentGesture.magnitude);
        ImGui::Text("  Velocity: %.2f", m_currentGesture.velocity);
        
        if (m_currentGesture.type == GestureType::Rotation) {
            ImGui::Text("  Rotation: %.1f deg", m_currentGesture.rotation);
        }
    } else {
        ImGui::TextDisabled("No active gesture");
    }
    
    ImGui::Separator();
    ImGui::Text("Recent Gestures:");
    
    for (const auto& gesture : m_recentGestures) {
        ImGui::BulletText("%s (%u fingers)", 
            GestureTypeToString(gesture.type).c_str(),
            gesture.fingerCount);
    }
}

void TouchpadGUI::RenderControls() {
    // Capture controls
    if (m_capturing) {
        if (ImGui::Button("Stop Capture", ImVec2(130, 30))) {
            m_touchpadInput->StopCapture();
            m_capturing = false;
        }
    } else {
        if (ImGui::Button("Start Capture", ImVec2(130, 30))) {
            if (m_touchpadInput->StartCapture()) {
                m_capturing = true;
            }
        }
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Clear Data", ImVec2(100, 30))) {
        m_fingerTrails.clear();
        m_recentGestures.clear();
        m_gestureRecognizer->Reset();
        
        for (auto& row : m_heatmap) {
            for (auto& cell : row) {
                cell.intensity = 0;
            }
        }
    }
    
    ImGui::Separator();
    
    // Visualization options
    ImGui::Text("Display Options:");
    ImGui::Checkbox("Show Trails", &m_showTrails);
    ImGui::Checkbox("Show Heatmap", &m_showHeatmap);
    ImGui::Checkbox("Show Numeric Data", &m_showNumericData);
    ImGui::Checkbox("Show Gesture Log", &m_showGestureLog);
    
    ImGui::SliderFloat("Trail Fade Time", &m_trailFadeTime, 0.1f, 2.0f, "%.1f s");
    
    ImGui::Separator();
    
    // Logging controls
    ImGui::Text("Logging:");
    
    bool loggingEnabled = m_logger->IsLoggingEnabled();
    if (ImGui::Checkbox("Enable Logging", &loggingEnabled)) {
        m_logger->SetLoggingEnabled(loggingEnabled);
    }
    
    if (ImGui::Button("Save Raw Input Log")) {
        m_logger->SaveRawInputLog("raw_input.csv");
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Save Gesture Log")) {
        m_logger->SaveGestureLog("gestures.csv");
    }
}

void TouchpadGUI::RenderDeviceInfo() {
    if (!m_touchpadInput) {
        ImGui::TextDisabled("No device");
        return;
    }
    
    const auto& info = m_touchpadInput->GetDeviceInfo();
    
    std::string name(info.deviceName.begin(), info.deviceName.end());
    std::string mfg(info.manufacturer.begin(), info.manufacturer.end());
    
    ImGui::Text("Device: %s", name.c_str());
    ImGui::Text("Manufacturer: %s", mfg.c_str());
    
    ImGui::Separator();
    
    ImGui::Text("VID: 0x%04X  PID: 0x%04X", info.vendorId, info.productId);
    ImGui::Text("Precision Touchpad: %s", info.isPrecisionTouchpad ? "Yes" : "No");
    ImGui::Text("Max Contacts: %u", info.maxContacts);
    
    ImGui::Separator();
    
    ImGui::Text("Coordinate Range:");
    ImGui::Text("  X: %d - %d", info.minX, info.maxX);
    ImGui::Text("  Y: %d - %d", info.minY, info.maxY);
    
    if (info.supportsPressure) {
        ImGui::Text("Pressure: %d - %d", info.minPressure, info.maxPressure);
    } else {
        ImGui::TextDisabled("Pressure: Not supported");
    }
    
    ImGui::Separator();
    
    ImGui::Text("Frames: %llu", m_touchpadInput->GetFrameCount());
}

void TouchpadGUI::RenderLogDisplay() {
    ImGui::Text("Raw Inputs: %zu  |  Gestures: %zu", 
        m_logger->GetRawInputCount(), 
        m_logger->GetGestureCount());
    
    ImGui::Separator();
    
    // Show recent raw inputs
    auto recentInputs = m_logger->GetRecentRawInputs(10);
    
    ImGui::BeginChild("LogScroll", ImVec2(0, 0), true);
    
    for (const auto& entry : recentInputs) {
        ImGui::Text("F%u: (%.3f, %.3f) %s", 
            entry.fingerId,
            entry.x, entry.y,
            ContactStateToString(entry.state).c_str());
    }
    
    ImGui::EndChild();
}

void TouchpadGUI::OnTouchFrame(const TouchFrame& frame) {
    // Update data (thread-safe)
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_currentFrame = frame;
    }
    
    // Update gesture recognizer
    m_gestureRecognizer->ProcessFrame(frame);
    
    // Update current gesture
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_currentGesture = m_gestureRecognizer->GetCurrentGesture();
    }
    
    // Log raw input
    m_logger->LogRawInput(frame);
    
    // Update trails
    UpdateTrails(frame);
    
    // Update heatmap
    if (m_showHeatmap) {
        UpdateHeatmap(frame);
    }
}

void TouchpadGUI::OnGesture(const GestureEvent& gesture) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    
    m_recentGestures.push_front(gesture);
    if (m_recentGestures.size() > MAX_RECENT_GESTURES) {
        m_recentGestures.pop_back();
    }
    
    // Log gesture
    m_logger->LogGesture(gesture);
}

void TouchpadGUI::UpdateTrails(const TouchFrame& frame) {
    // Ensure we have enough trail vectors
    while (m_fingerTrails.size() < 10) {
        m_fingerTrails.emplace_back();
    }
    
    for (uint32_t i = 0; i < frame.activeCount && i < 10; i++) {
        const auto& contact = frame.contacts[i];
        if (!contact.isValid || contact.state == ContactState::Up) continue;
        
        // Find trail by contact ID or use index
        int trailIndex = -1;
        for (size_t t = 0; t < m_fingerTrails.size(); t++) {
            if (!m_fingerTrails[t].empty() && i < m_fingerTrails.size()) {
                trailIndex = static_cast<int>(i);
                break;
            }
        }
        
        if (trailIndex < 0) trailIndex = i;
        
        if (contact.state == ContactState::Down) {
            // New contact - start fresh trail
            m_fingerTrails[trailIndex].clear();
        }
        
        TrailPoint point;
        point.x = contact.x;
        point.y = contact.y;
        point.alpha = 1.0f;
        point.timestamp = contact.timestamp;
        
        m_fingerTrails[trailIndex].push_back(point);
        
        // Limit trail length
        if (m_fingerTrails[trailIndex].size() > 100) {
            m_fingerTrails[trailIndex].erase(m_fingerTrails[trailIndex].begin());
        }
    }
}

void TouchpadGUI::UpdateHeatmap(const TouchFrame& frame) {
    auto now = Clock::now();
    
    for (uint32_t i = 0; i < frame.activeCount && i < 10; i++) {
        const auto& contact = frame.contacts[i];
        if (!contact.isValid || contact.state == ContactState::Up) continue;
        
        int cellX = static_cast<int>(contact.x * (HEATMAP_RESOLUTION - 1));
        int cellY = static_cast<int>(contact.y * (HEATMAP_RESOLUTION - 1));
        
        cellX = std::clamp(cellX, 0, HEATMAP_RESOLUTION - 1);
        cellY = std::clamp(cellY, 0, HEATMAP_RESOLUTION - 1);
        
        // Add intensity based on pressure
        float intensity = 0.1f + 0.2f * contact.pressure;
        m_heatmap[cellY][cellX].intensity = std::min(1.0f, 
            m_heatmap[cellY][cellX].intensity + intensity);
        m_heatmap[cellY][cellX].lastUpdate = now;
        
        // Spread to neighbors
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                
                int nx = cellX + dx;
                int ny = cellY + dy;
                
                if (nx >= 0 && nx < HEATMAP_RESOLUTION && ny >= 0 && ny < HEATMAP_RESOLUTION) {
                    m_heatmap[ny][nx].intensity = std::min(1.0f,
                        m_heatmap[ny][nx].intensity + intensity * 0.3f);
                    m_heatmap[ny][nx].lastUpdate = now;
                }
            }
        }
    }
}

} // namespace TouchpadTracker
