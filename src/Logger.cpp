#include "Logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>

namespace TouchpadTracker {

Logger::Logger() {
}

Logger::~Logger() {
    Shutdown();
}

bool Logger::Initialize(const std::string& basePath) {
    m_basePath = basePath;
    
    // Create logs directory if it doesn't exist
    try {
        std::filesystem::create_directories(basePath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create log directory: " << e.what() << std::endl;
        return false;
    }
    
    std::cout << "Logger initialized. Log path: " << basePath << std::endl;
    return true;
}

void Logger::Shutdown() {
    // Could flush any pending logs here
}

std::string Logger::FormatTimestamp(TimePoint tp) const {
    auto now = std::chrono::system_clock::now();
    auto sinceEpoch = now.time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(sinceEpoch);
    
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time);
    
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << (ms.count() % 1000);
    
    return ss.str();
}

void Logger::LogRawInput(const TouchFrame& frame) {
    if (!m_enabled) return;
    
    std::lock_guard<std::mutex> lock(m_rawMutex);
    
    for (uint32_t i = 0; i < frame.activeCount && i < 10; i++) {
        const auto& contact = frame.contacts[i];
        if (!contact.isValid) continue;
        
        RawInputLogEntry entry;
        entry.timestamp = contact.timestamp;
        entry.fingerId = contact.id;
        entry.x = contact.x;
        entry.y = contact.y;
        entry.pressure = contact.pressure;
        entry.state = contact.state;
        
        m_rawInputBuffer.push_back(entry);
        
        if (m_rawInputBuffer.size() > MAX_RAW_BUFFER) {
            m_rawInputBuffer.pop_front();
        }
    }
}

void Logger::LogRawInputEntry(const RawInputLogEntry& entry) {
    if (!m_enabled) return;
    
    std::lock_guard<std::mutex> lock(m_rawMutex);
    
    m_rawInputBuffer.push_back(entry);
    
    if (m_rawInputBuffer.size() > MAX_RAW_BUFFER) {
        m_rawInputBuffer.pop_front();
    }
}

void Logger::LogGesture(const GestureEvent& gesture) {
    if (!m_enabled) return;
    
    std::lock_guard<std::mutex> lock(m_gestureMutex);
    
    GestureLogEntry entry;
    entry.timestamp = gesture.timestamp;
    entry.type = gesture.type;
    entry.fingerCount = gesture.fingerCount;
    entry.direction = gesture.direction;
    entry.velocity = gesture.velocity;
    entry.centerX = gesture.centerX;
    entry.centerY = gesture.centerY;
    
    m_gestureBuffer.push_back(entry);
    
    if (m_gestureBuffer.size() > MAX_GESTURE_BUFFER) {
        m_gestureBuffer.pop_front();
    }
}

void Logger::LogGestureEntry(const GestureLogEntry& entry) {
    if (!m_enabled) return;
    
    std::lock_guard<std::mutex> lock(m_gestureMutex);
    
    m_gestureBuffer.push_back(entry);
    
    if (m_gestureBuffer.size() > MAX_GESTURE_BUFFER) {
        m_gestureBuffer.pop_front();
    }
}

std::vector<RawInputLogEntry> Logger::GetRecentRawInputs(size_t count) const {
    std::lock_guard<std::mutex> lock(m_rawMutex);
    
    std::vector<RawInputLogEntry> result;
    size_t actualCount = std::min(count, m_rawInputBuffer.size());
    
    for (size_t i = 0; i < actualCount; i++) {
        result.push_back(m_rawInputBuffer[m_rawInputBuffer.size() - 1 - i]);
    }
    
    return result;
}

std::vector<GestureLogEntry> Logger::GetRecentGestures(size_t count) const {
    std::lock_guard<std::mutex> lock(m_gestureMutex);
    
    std::vector<GestureLogEntry> result;
    size_t actualCount = std::min(count, m_gestureBuffer.size());
    
    for (size_t i = 0; i < actualCount; i++) {
        result.push_back(m_gestureBuffer[m_gestureBuffer.size() - 1 - i]);
    }
    
    return result;
}

bool Logger::SaveRawInputLog(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_rawMutex);
    
    std::string filepath = m_basePath + "/" + filename;
    std::ofstream file(filepath);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filepath << std::endl;
        return false;
    }
    
    // Write CSV header
    file << "timestamp,finger_id,x,y,pressure,state\n";
    
    for (const auto& entry : m_rawInputBuffer) {
        file << FormatTimestamp(entry.timestamp) << ","
             << entry.fingerId << ","
             << std::fixed << std::setprecision(6) << entry.x << ","
             << entry.y << ","
             << entry.pressure << ","
             << ContactStateToString(entry.state) << "\n";
    }
    
    file.close();
    std::cout << "Saved raw input log to: " << filepath << std::endl;
    return true;
}

bool Logger::SaveGestureLog(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_gestureMutex);
    
    std::string filepath = m_basePath + "/" + filename;
    std::ofstream file(filepath);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filepath << std::endl;
        return false;
    }
    
    // Write CSV header
    file << "timestamp,gesture_type,finger_count,direction,velocity,center_x,center_y\n";
    
    for (const auto& entry : m_gestureBuffer) {
        file << FormatTimestamp(entry.timestamp) << ","
             << GestureTypeToString(entry.type) << ","
             << entry.fingerCount << ","
             << DirectionToString(entry.direction) << ","
             << std::fixed << std::setprecision(4) << entry.velocity << ","
             << entry.centerX << ","
             << entry.centerY << "\n";
    }
    
    file.close();
    std::cout << "Saved gesture log to: " << filepath << std::endl;
    return true;
}

void Logger::ClearBuffers() {
    {
        std::lock_guard<std::mutex> lock(m_rawMutex);
        m_rawInputBuffer.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_gestureMutex);
        m_gestureBuffer.clear();
    }
}

} // namespace TouchpadTracker
