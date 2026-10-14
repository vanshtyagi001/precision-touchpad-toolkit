#include "GestureRecognizer.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace TouchpadTracker {

constexpr float PI = 3.14159265358979323846f;

GestureRecognizer::GestureRecognizer() {
    Reset();
}

GestureRecognizer::~GestureRecognizer() {
}

void GestureRecognizer::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& track : m_fingerTracks) {
        track.active = false;
    }
    
    m_peakFingerCount = 0;
    m_gestureInProgress = false;
    m_currentGesture = GestureEvent{};
    m_waitingForDoubleTap = false;
    m_initialPinchDistance = 0;
    m_initialRotationAngle = 0;
    m_accumulatedRotation = 0;
    m_accumulatedPinch = 0;
}

void GestureRecognizer::SetGestureCallback(GestureCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_gestureCallback = callback;
}

GestureEvent GestureRecognizer::GetCurrentGesture() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentGesture;
}

std::vector<GestureEvent> GestureRecognizer::GetRecentGestures(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<GestureEvent> result;
    size_t actualCount = std::min(count, m_gestureHistory.size());
    
    for (size_t i = 0; i < actualCount; i++) {
        result.push_back(m_gestureHistory[m_gestureHistory.size() - 1 - i]);
    }
    
    return result;
}

void GestureRecognizer::ProcessFrame(const TouchFrame& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_previousFrame = m_currentFrame;
    m_currentFrame = frame;
    
    // Count active fingers
    uint32_t activeFingers = 0;
    for (uint32_t i = 0; i < frame.activeCount && i < 10; i++) {
        if (frame.contacts[i].isValid && frame.contacts[i].state != ContactState::Up) {
            activeFingers++;
        }
    }
    
    // Track finger down/up events
    for (uint32_t i = 0; i < frame.activeCount && i < 10; i++) {
        const auto& contact = frame.contacts[i];
        if (!contact.isValid) continue;
        
        // Find or create finger track
        FingerTrack* track = nullptr;
        for (auto& t : m_fingerTracks) {
            if (t.active && t.id == contact.id) {
                track = &t;
                break;
            }
        }
        
        if (contact.state == ContactState::Down) {
            // New finger - find empty slot
            for (auto& t : m_fingerTracks) {
                if (!t.active) {
                    track = &t;
                    break;
                }
            }
            
            if (track) {
                track->active = true;
                track->id = contact.id;
                track->startX = contact.x;
                track->startY = contact.y;
                track->currentX = contact.x;
                track->currentY = contact.y;
                track->totalDeltaX = 0;
                track->totalDeltaY = 0;
                track->startTime = contact.timestamp;
                track->lastUpdateTime = contact.timestamp;
                track->maxVelocity = 0;
                track->positionHistory.clear();
                track->positionHistory.push_back({contact.x, contact.y});
                
                if (!m_gestureInProgress) {
                    m_gestureInProgress = true;
                    m_gestureStartTime = contact.timestamp;
                    m_peakFingerCount = 0;
                }
            }
        }
        else if (contact.state == ContactState::Move && track) {
            track->currentX = contact.x;
            track->currentY = contact.y;
            track->totalDeltaX = contact.x - track->startX;
            track->totalDeltaY = contact.y - track->startY;
            track->lastUpdateTime = contact.timestamp;
            
            float velocity = std::sqrt(contact.velocityX * contact.velocityX + 
                                       contact.velocityY * contact.velocityY);
            track->maxVelocity = std::max(track->maxVelocity, velocity);
            
            track->positionHistory.push_back({contact.x, contact.y});
            if (track->positionHistory.size() > 20) {
                track->positionHistory.pop_front();
            }
        }
        else if (contact.state == ContactState::Up && track) {
            track->active = false;
        }
    }
    
    // Update peak finger count
    uint32_t currentCount = 0;
    for (const auto& track : m_fingerTracks) {
        if (track.active) currentCount++;
    }
    m_peakFingerCount = std::max(m_peakFingerCount, currentCount);
    
    // Detect gestures based on finger count
    UpdateGestureState();
    
    // Check if gesture ended
    if (currentCount == 0 && m_gestureInProgress) {
        FinalizeGesture();
    }
}

void GestureRecognizer::UpdateGestureState() {
    uint32_t activeCount = 0;
    for (const auto& track : m_fingerTracks) {
        if (track.active) activeCount++;
    }
    
    if (activeCount == 0) return;
    
    // Calculate center of all fingers
    float centerX = 0, centerY = 0;
    for (const auto& track : m_fingerTracks) {
        if (track.active) {
            centerX += track.currentX;
            centerY += track.currentY;
        }
    }
    centerX /= activeCount;
    centerY /= activeCount;
    
    // Calculate total movement
    float totalDeltaX = 0, totalDeltaY = 0;
    for (const auto& track : m_fingerTracks) {
        if (track.active) {
            totalDeltaX += track.totalDeltaX;
            totalDeltaY += track.totalDeltaY;
        }
    }
    totalDeltaX /= activeCount;
    totalDeltaY /= activeCount;
    
    m_currentGesture.centerX = centerX;
    m_currentGesture.centerY = centerY;
    m_currentGesture.deltaX = totalDeltaX;
    m_currentGesture.deltaY = totalDeltaY;
    m_currentGesture.fingerCount = activeCount;
    m_currentGesture.isActive = true;
    m_currentGesture.timestamp = m_currentFrame.timestamp;
    
    if (activeCount == 1) {
        DetectSingleFingerGestures();
    } else if (activeCount == 2) {
        DetectTwoFingerGestures();
    } else if (activeCount >= 3) {
        DetectThreeFingerGestures();
    }
}

void GestureRecognizer::DetectSingleFingerGestures() {
    FingerTrack* track = nullptr;
    for (auto& t : m_fingerTracks) {
        if (t.active) {
            track = &t;
            break;
        }
    }
    
    if (!track) return;
    
    float distance = CalculateDistance(track->startX, track->startY, 
                                        track->currentX, track->currentY);
    auto duration = std::chrono::duration<float>(
        m_currentFrame.timestamp - track->startTime).count();
    
    // Check for drag
    if (distance > m_config.tapMaxMovement) {
        m_currentGesture.type = GestureType::Drag;
        m_currentGesture.direction = CalculateDirection(track->totalDeltaX, track->totalDeltaY);
        m_currentGesture.velocity = track->maxVelocity;
        m_currentGesture.magnitude = distance;
    }
    // Could be a tap or long press (will be determined on release)
}

void GestureRecognizer::DetectTwoFingerGestures() {
    std::vector<FingerTrack*> activeTracks;
    for (auto& track : m_fingerTracks) {
        if (track.active) {
            activeTracks.push_back(&track);
        }
    }
    
    if (activeTracks.size() < 2) return;
    
    auto* track1 = activeTracks[0];
    auto* track2 = activeTracks[1];
    
    // Current distance between fingers
    float currentDistance = CalculateDistance(
        track1->currentX, track1->currentY,
        track2->currentX, track2->currentY);
    
    // Initial distance
    float initialDistance = CalculateDistance(
        track1->startX, track1->startY,
        track2->startX, track2->startY);
    
    if (m_initialPinchDistance == 0) {
        m_initialPinchDistance = initialDistance;
        m_initialRotationAngle = CalculateAngle(
            track2->startX - track1->startX,
            track2->startY - track1->startY, 0, 0);
    }
    
    // Calculate pinch
    float pinchDelta = currentDistance - m_initialPinchDistance;
    m_accumulatedPinch = pinchDelta;
    
    // Calculate rotation
    float currentAngle = CalculateAngle(
        track2->currentX - track1->currentX,
        track2->currentY - track1->currentY, 0, 0);
    float rotationDelta = currentAngle - m_initialRotationAngle;
    
    // Normalize rotation to -180 to 180
    while (rotationDelta > 180) rotationDelta -= 360;
    while (rotationDelta < -180) rotationDelta += 360;
    m_accumulatedRotation = rotationDelta;
    
    // Average movement (for scroll)
    float avgDeltaX = (track1->totalDeltaX + track2->totalDeltaX) / 2;
    float avgDeltaY = (track1->totalDeltaY + track2->totalDeltaY) / 2;
    float avgDistance = std::sqrt(avgDeltaX * avgDeltaX + avgDeltaY * avgDeltaY);
    
    // Determine gesture type
    if (std::abs(pinchDelta) > m_config.pinchMinDistance) {
        if (pinchDelta > 0) {
            m_currentGesture.type = GestureType::PinchOut;
        } else {
            m_currentGesture.type = GestureType::PinchIn;
        }
        m_currentGesture.magnitude = std::abs(pinchDelta);
    }
    else if (std::abs(rotationDelta) > m_config.rotationMinAngle) {
        m_currentGesture.type = GestureType::Rotation;
        m_currentGesture.rotation = rotationDelta;
        m_currentGesture.magnitude = std::abs(rotationDelta);
    }
    else if (avgDistance > m_config.scrollThreshold) {
        // Scroll - determine direction
        if (std::abs(avgDeltaX) > std::abs(avgDeltaY)) {
            m_currentGesture.type = GestureType::TwoFingerScrollHorizontal;
        } else {
            m_currentGesture.type = GestureType::TwoFingerScroll;
        }
        m_currentGesture.direction = CalculateDirection(avgDeltaX, avgDeltaY);
        m_currentGesture.magnitude = avgDistance;
    }
    
    m_currentGesture.fingerCount = 2;
}

void GestureRecognizer::DetectThreeFingerGestures() {
    std::vector<FingerTrack*> activeTracks;
    for (auto& track : m_fingerTracks) {
        if (track.active) {
            activeTracks.push_back(&track);
        }
    }
    
    if (activeTracks.size() < 3) return;
    
    // Calculate average movement
    float avgDeltaX = 0, avgDeltaY = 0;
    for (const auto* track : activeTracks) {
        avgDeltaX += track->totalDeltaX;
        avgDeltaY += track->totalDeltaY;
    }
    avgDeltaX /= activeTracks.size();
    avgDeltaY /= activeTracks.size();
    
    float distance = std::sqrt(avgDeltaX * avgDeltaX + avgDeltaY * avgDeltaY);
    
    if (distance > m_config.swipeMinDistance) {
        // Determine swipe direction
        if (std::abs(avgDeltaX) > std::abs(avgDeltaY)) {
            if (avgDeltaX > 0) {
                m_currentGesture.type = GestureType::ThreeFingerSwipeRight;
            } else {
                m_currentGesture.type = GestureType::ThreeFingerSwipeLeft;
            }
        } else {
            if (avgDeltaY > 0) {
                m_currentGesture.type = GestureType::ThreeFingerSwipeDown;
            } else {
                m_currentGesture.type = GestureType::ThreeFingerSwipeUp;
            }
        }
        m_currentGesture.direction = CalculateDirection(avgDeltaX, avgDeltaY);
        m_currentGesture.magnitude = distance;
    }
    
    m_currentGesture.fingerCount = static_cast<uint32_t>(activeTracks.size());
}

void GestureRecognizer::FinalizeGesture() {
    auto now = m_currentFrame.timestamp;
    auto gestureDuration = std::chrono::duration<float>(now - m_gestureStartTime).count();
    
    m_currentGesture.duration = std::chrono::duration<double>(now - m_gestureStartTime);
    m_currentGesture.isActive = false;
    m_currentGesture.isComplete = true;
    
    // Determine final gesture type based on what happened
    float totalMovement = std::sqrt(
        m_currentGesture.deltaX * m_currentGesture.deltaX +
        m_currentGesture.deltaY * m_currentGesture.deltaY);
    
    // Single finger tap detection
    if (m_peakFingerCount == 1 && totalMovement < m_config.tapMaxMovement) {
        if (gestureDuration >= m_config.longPressMinDuration) {
            m_currentGesture.type = GestureType::LongPress;
        }
        else if (gestureDuration <= m_config.tapMaxDuration) {
            // Check for double tap
            float timeSinceLastTap = std::chrono::duration<float>(now - m_lastTapTime).count();
            float distFromLastTap = CalculateDistance(
                m_currentGesture.centerX, m_currentGesture.centerY,
                m_lastTapX, m_lastTapY);
            
            if (m_waitingForDoubleTap && 
                timeSinceLastTap <= m_config.doubleTapMaxInterval &&
                distFromLastTap < m_config.tapMaxMovement * 2) {
                m_currentGesture.type = GestureType::DoubleTap;
                m_waitingForDoubleTap = false;
            } else {
                m_currentGesture.type = GestureType::Tap;
                m_lastTapTime = now;
                m_lastTapX = m_currentGesture.centerX;
                m_lastTapY = m_currentGesture.centerY;
                m_waitingForDoubleTap = true;
            }
        }
    }
    // Single finger swipe/flick
    else if (m_peakFingerCount == 1 && totalMovement >= m_config.swipeMinDistance) {
        // Find max velocity
        float maxVelocity = 0;
        for (const auto& track : m_fingerTracks) {
            maxVelocity = std::max(maxVelocity, track.maxVelocity);
        }
        
        if (maxVelocity >= m_config.flickMinVelocity) {
            m_currentGesture.type = GestureType::Flick;
        } else {
            m_currentGesture.type = GestureType::Swipe;
        }
        m_currentGesture.direction = CalculateDirection(m_currentGesture.deltaX, m_currentGesture.deltaY);
        m_currentGesture.velocity = maxVelocity;
    }
    // Two finger tap
    else if (m_peakFingerCount == 2 && totalMovement < m_config.tapMaxMovement * 2) {
        if (gestureDuration <= m_config.tapMaxDuration) {
            m_currentGesture.type = GestureType::TwoFingerTap;
        }
    }
    // Three finger tap
    else if (m_peakFingerCount >= 3 && totalMovement < m_config.tapMaxMovement * 3) {
        if (gestureDuration <= m_config.tapMaxDuration * 1.5f) {
            m_currentGesture.type = GestureType::ThreeFingerTap;
        }
    }
    
    // Emit gesture if valid
    if (m_currentGesture.type != GestureType::None) {
        EmitGesture(m_currentGesture);
    }
    
    // Reset state
    m_gestureInProgress = false;
    m_peakFingerCount = 0;
    m_initialPinchDistance = 0;
    m_initialRotationAngle = 0;
    m_accumulatedRotation = 0;
    m_accumulatedPinch = 0;
    
    // Clear finger tracks
    for (auto& track : m_fingerTracks) {
        track.active = false;
    }
}

void GestureRecognizer::EmitGesture(const GestureEvent& gesture) {
    // Add to history
    m_gestureHistory.push_back(gesture);
    if (m_gestureHistory.size() > 100) {
        m_gestureHistory.pop_front();
    }
    
    // Call callback (without lock)
    GestureCallback callback = m_gestureCallback;
    if (callback) {
        callback(gesture);
    }
}

Direction GestureRecognizer::CalculateDirection(float dx, float dy) {
    if (std::abs(dx) < 0.001f && std::abs(dy) < 0.001f) {
        return Direction::None;
    }
    
    float angle = std::atan2(-dy, dx) * 180.0f / PI;  // Negate Y because Y increases downward
    
    // Normalize to 0-360
    if (angle < 0) angle += 360;
    
    // Determine direction based on angle
    if (angle >= 337.5f || angle < 22.5f) return Direction::Right;
    if (angle >= 22.5f && angle < 67.5f) return Direction::UpRight;
    if (angle >= 67.5f && angle < 112.5f) return Direction::Up;
    if (angle >= 112.5f && angle < 157.5f) return Direction::UpLeft;
    if (angle >= 157.5f && angle < 202.5f) return Direction::Left;
    if (angle >= 202.5f && angle < 247.5f) return Direction::DownLeft;
    if (angle >= 247.5f && angle < 292.5f) return Direction::Down;
    if (angle >= 292.5f && angle < 337.5f) return Direction::DownRight;
    
    return Direction::None;
}

float GestureRecognizer::CalculateDistance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

float GestureRecognizer::CalculateAngle(float x1, float y1, float x2, float y2) {
    return std::atan2(y1, x1) * 180.0f / PI;
}

} // namespace TouchpadTracker
