#pragma once

#include "Types.h"
#include "TouchpadInput.h"
#include "GestureRecognizer.h"
#include "Logger.h"

#include <Windows.h>
#include <memory>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>

namespace TouchpadTracker {

// Trail point for finger motion visualization
struct TrailPoint {
    float x, y;
    float alpha;
    TimePoint timestamp;
};

// Heatmap cell
struct HeatmapCell {
    float intensity;
    TimePoint lastUpdate;
};

class TouchpadGUI {
public:
    TouchpadGUI();
    ~TouchpadGUI();
    
    // Initialize and run
    bool Initialize();
    void Run();
    void Shutdown();
    
    // Window procedure for raw input
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
private:
    bool CreateAppWindow();
    bool InitializeOpenGL();
    bool InitializeImGui();
    
    void ProcessInput();
    void Update();
    void Render();
    
    void RenderTouchpadVisualization();
    void RenderHeatmap();
    void RenderFingerMarkers();
    void RenderTrails();
    void RenderNumericDisplay();
    void RenderGestureDisplay();
    void RenderControls();
    void RenderLogDisplay();
    void RenderDeviceInfo();
    
    void UpdateTrails(const TouchFrame& frame);
    void UpdateHeatmap(const TouchFrame& frame);
    
    // Callbacks from input/gesture
    void OnTouchFrame(const TouchFrame& frame);
    void OnGesture(const GestureEvent& gesture);
    
    // Window handles
    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_hglrc = nullptr;
    
    // Components
    std::unique_ptr<TouchpadInput> m_touchpadInput;
    std::unique_ptr<GestureRecognizer> m_gestureRecognizer;
    std::unique_ptr<Logger> m_logger;
    
    // State
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_capturing{false};
    std::atomic<bool> m_imguiInitialized{false};
    
    // Current touch data (thread-safe copy)
    TouchFrame m_currentFrame;
    GestureEvent m_currentGesture;
    std::mutex m_dataMutex;
    
    // Visualization data
    std::deque<std::vector<TrailPoint>> m_fingerTrails;
    std::vector<std::vector<HeatmapCell>> m_heatmap;
    static constexpr int HEATMAP_RESOLUTION = 32;
    
    // Recent gestures for display
    std::deque<GestureEvent> m_recentGestures;
    static constexpr size_t MAX_RECENT_GESTURES = 10;
    
    // Display options
    bool m_showHeatmap = false;
    bool m_showTrails = true;
    bool m_showNumericData = true;
    bool m_showGestureLog = true;
    float m_trailFadeTime = 0.5f;
    
    // Performance tracking
    double m_fps = 0;
    double m_inputRate = 0;
    TimePoint m_lastFrameTime;
    int m_frameCount = 0;
    
    // Window size
    int m_windowWidth = 1280;
    int m_windowHeight = 720;
    
    // Touchpad visualization area
    float m_touchpadDisplayX = 50;
    float m_touchpadDisplayY = 50;
    float m_touchpadDisplayWidth = 500;
    float m_touchpadDisplayHeight = 350;
};

} // namespace TouchpadTracker
