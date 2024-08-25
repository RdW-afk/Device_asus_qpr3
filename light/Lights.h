/*
 * Copyright (C) 2023-2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AIDL_ANDROID_HARDWARE_LIGHT_LIGHTS_H
#define AIDL_ANDROID_HARDWARE_LIGHT_LIGHTS_H

#include <aidl/android/hardware/light/BnLights.h>

#include <fstream>
#include <mutex>
#include <unordered_map>

using ::aidl::android::hardware::light::HwLightState;
using ::aidl::android::hardware::light::HwLight;

namespace aidl {
namespace android {
namespace hardware {
namespace light {

class Lights : public BnLights {
  public:
    Lights(std::pair<std::ofstream, uint32_t>&& lcd_backlight, std::ofstream&& red_breath,
           std::ofstream&& red_led, std::ofstream&& green_breath, std::ofstream&& green_led);

    // Methods from ::aidl::android::hardware::light::BnLights follow.
    ndk::ScopedAStatus setLightState(int32_t id, const HwLightState& state) override;
    ndk::ScopedAStatus getLights(std::vector<HwLight> *_aidl_return) override;

  private:
    void setAttentionLight(const HwLightState& state);
    void setBatteryLight(const HwLightState& state);
    void setButtonsBacklight(const HwLightState& state);
    void setLcdBacklight(const HwLightState& state);
    void setNotificationLight(const HwLightState& state);
    void setSpeakerBatteryLightLocked();
    void setSpeakerLightLocked(const HwLightState& state);

    std::pair<std::ofstream, uint32_t> mLcdBacklight;

    // Red led
    std::ofstream mRedBreath;
    std::ofstream mRedLed;

    // Green led
    std::ofstream mGreenBreath;
    std::ofstream mGreenLed;

    HwLightState mAttentionState;
    HwLightState mBatteryState;
    HwLightState mNotificationState;

    std::unordered_map<LightType, std::function<void(const HwLightState&)>> mLights;
    std::mutex mLock;
};

}  // namespace light
}  // namespace hardware
}  // namespace android
}  // namespace aidl

#endif  // AIDL_ANDROID_HARDWARE_LIGHT_LIGHTS_H
