#pragma once

#include "Types.h"
#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <deque>

namespace TouchpadTracker {

class Logger {
public:
    Logger();
    ~Logger();
    
    // Initialize logging
    bool Initialize(const std::string& basePath = "logs");
    void Shutdown();
    
    // Raw input logging
    void LogRawInput(const TouchFrame& frame);
    void LogRawInputEntry(const RawInputLogEntry& entry);
    
    // Gesture logging
    void LogGesture(const GestureEvent& gesture);
    void LogGestureEntry(const GestureLogEntry& entry);
    
    // Buffer access for GUI
    std::vector<RawInputLogEntry> GetRecentRawInputs(size_t count = 100) const;
    std::vector<GestureLogEntry> GetRecentGestures(size_t count = 50) const;
    
    // File operations
    bool SaveRawInputLog(const std::string& filename);
    bool SaveGestureLog(const std::string& filename);
    
    // Enable/disable
    void SetLoggingEnabled(bool enabled) { m_enabled = enabled; }
    bool IsLoggingEnabled() const { return m_enabled; }
    
    // Statistics
    size_t GetRawInputCount() const { return m_rawInputBuffer.size(); }
    size_t GetGestureCount() const { return m_gestureBuffer.size(); }
    
    // Clear buffers
    void ClearBuffers();
    
private:
    std::string FormatTimestamp(TimePoint tp) const;
    
    bool m_enabled = true;
    std::string m_basePath;
    
    mutable std::mutex m_rawMutex;
    mutable std::mutex m_gestureMutex;
    
    std::deque<RawInputLogEntry> m_rawInputBuffer;
    std::deque<GestureLogEntry> m_gestureBuffer;
    
    static constexpr size_t MAX_RAW_BUFFER = 10000;
    static constexpr size_t MAX_GESTURE_BUFFER = 1000;
};

} // namespace TouchpadTracker
