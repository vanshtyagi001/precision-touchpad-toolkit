#pragma once

#include "Config.h"

namespace TouchpadShortcuts {

class SystemActions {
public:
    // Volume control
    static void VolumeUp();
    static void VolumeDown();
    static void VolumeMute();
    
    // Brightness control
    static void BrightnessUp();
    static void BrightnessDown();
    
    // Execute action for a zone
    static void ExecuteZoneAction(Zone zone);
    
    // Show on-screen notification (optional visual feedback)
    static void ShowNotification(const char* title, const char* message);
};

} // namespace TouchpadShortcuts
