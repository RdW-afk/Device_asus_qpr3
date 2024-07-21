/*
 * Copyright (C) 2023-2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

//#define HAL_DEBUG
#define LOG_TAG "LightService"

#include "Lights.h"

#ifdef HAL_DEBUG
#include <android-base/logging.h>
#endif

namespace {

static constexpr int DEFAULT_MAX_BRIGHTNESS = 255;

static uint32_t rgbToBrightness(const HwLightState& state) {
    uint32_t color = state.color & 0x00ffffff;
    return ((77 * ((color >> 16) & 0xff)) + (150 * ((color >> 8) & 0xff)) + (29 * (color & 0xff))) >>
           8;
}

static bool isLit(const HwLightState& state) {
    return (state.color & 0x00ffffff);
}

}  // anonymous namespace

namespace aidl {
namespace android {
namespace hardware {
namespace light {

Lights::Lights(std::pair<std::ofstream, uint32_t>&& lcd_backlight, std::ofstream&& red_breath,
               std::ofstream&& red_led, std::ofstream&& green_breath, std::ofstream&& green_led)
    : mLcdBacklight(std::move(lcd_backlight)),
      mRedBreath(std::move(red_breath)),
      mRedLed(std::move(red_led)),
      mGreenBreath(std::move(green_breath)),
      mGreenLed(std::move(green_led)) {
    auto attnFn(std::bind(&Lights::setAttentionLight, this, std::placeholders::_1));
    auto backlightFn(std::bind(&Lights::setLcdBacklight, this, std::placeholders::_1));
    auto batteryFn(std::bind(&Lights::setBatteryLight, this, std::placeholders::_1));
    auto buttonsFn(
        std::bind(&Lights::setButtonsBacklight, this, std::placeholders::_1));  // fake button dummy
    auto notifFn(std::bind(&Lights::setNotificationLight, this, std::placeholders::_1));
    mLights.emplace(std::make_pair(LightType::ATTENTION, attnFn));
    mLights.emplace(std::make_pair(LightType::BACKLIGHT, backlightFn));
    mLights.emplace(std::make_pair(LightType::BATTERY, batteryFn));
    mLights.emplace(std::make_pair(LightType::BUTTONS, buttonsFn));  // fake button dummy
    mLights.emplace(std::make_pair(LightType::NOTIFICATIONS, notifFn));
}

// Methods from ::aidl::android::hardware::light::BnLights follow.
ndk::ScopedAStatus Lights::setLightState(int32_t id, const HwLightState& state) {
    auto it = mLights.find(static_cast<LightType>(id));

    if (it == mLights.end()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    it->second(state);

    return ndk::ScopedAStatus::ok();
}

#define AutoHwLight(light) {.id = (int32_t)light, .type = light, .ordinal = 0}

ndk::ScopedAStatus Lights::getLights(std::vector<HwLight> *_aidl_return) {

    for (auto const& light : mLights) {
        _aidl_return->push_back(AutoHwLight(light.first));
    }

    return ndk::ScopedAStatus::ok();
}

void Lights::setAttentionLight(const HwLightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    mAttentionState = state;
    setSpeakerBatteryLightLocked();
}

void Lights::setLcdBacklight(const HwLightState& state) {
    std::lock_guard<std::mutex> lock(mLock);

    uint32_t brightness = rgbToBrightness(state);

    // If max panel brightness is not the default (255),
    // apply linear scaling across the accepted range.
    if (mLcdBacklight.second != DEFAULT_MAX_BRIGHTNESS) {
#ifdef HAL_DEBUG
        int old_brightness = brightness;
#endif
        brightness = brightness * mLcdBacklight.second / DEFAULT_MAX_BRIGHTNESS;
#ifdef HAL_DEBUG
        LOG(VERBOSE) << "scaling brightness " << old_brightness << " => " << brightness;
#endif
    }

    mLcdBacklight.first << brightness << std::endl;
}

void Lights::setButtonsBacklight(const HwLightState& state) {
    // We have no buttons light management, so do nothing.
    // This function required to shut up warnings about missing functionality.
    (void)state;
}

void Lights::setBatteryLight(const HwLightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    mBatteryState = state;
    setSpeakerBatteryLightLocked();
}

void Lights::setNotificationLight(const HwLightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    mNotificationState = state;
    setSpeakerBatteryLightLocked();
}

void Lights::setSpeakerBatteryLightLocked() {
    if (isLit(mNotificationState)) {
        setSpeakerLightLocked(mNotificationState);
    } else if (isLit(mAttentionState)) {
        setSpeakerLightLocked(mAttentionState);
    } else if (isLit(mBatteryState)) {
        setSpeakerLightLocked(mBatteryState);
    } else {
        // No active LED scenarios, turn off the LEDs
        setSpeakerLightLocked(HwLightState{}); // Set an empty LightState to turn off the LEDs
    }
}

void Lights::setSpeakerLightLocked(const HwLightState& state) {
    int red, green;
    int breath;
    int onMs, offMs;
    uint32_t colorARGB = state.color;

#ifdef HAL_DEBUG
    int stateMode;
#endif

    // Disable previous active light
    mRedBreath << 0 << std::endl;
    mGreenBreath << 0 << std::endl;

    switch (state.flashMode) {
        case FlashMode::TIMED:
            onMs = state.flashOnMs;
            offMs = state.flashOffMs;
#ifdef HAL_DEBUG
            stateMode = 1;
#endif
            break;
        case FlashMode::NONE:
        default:
            onMs = 0;
            offMs = 0;
#ifdef HAL_DEBUG
            stateMode = 0;
#endif
            break;
    }

    red = (colorARGB >> 16) & 0xff;
    green = (colorARGB >> 8) & 0xff;

    if (onMs > 0 && offMs > 0)
        breath = 1;
    else
        breath = 0;

    // Use only 255(0xFF) for base colors
    if (colorARGB > 0xFF000000 && state == mBatteryState) {
        if (red >= green) {
            green = 0;
            red = 0xFF;
        } else if (!breath && red >= 0x50 && green > red) {  // try make orange
            green = 0xFF;
            red = 0x08;
        } else {
            green = 0xFF;
            red = 0;
        }
    } else if (colorARGB > 0xFF000000 && state == mNotificationState) {
        green = 0xFF;
        red = 0;
    } else {
        green = 0;
        red = 0;
    }

#ifdef HAL_DEBUG
    int ledState;
    if (state == mBatteryState)
        ledState = 1;
    else if (state == mNotificationState)
        ledState = 2;
    else
        ledState = 0;

    LOG(VERBOSE) << "Light::setSpeakerLightLocked: mode=" << stateMode << " ledState=" << ledState
                 << " colorARGB=" << colorARGB << " onMS=" << onMs << " offMS=" << offMs
                 << " breath=" << breath << " red=" << red << " green" << green;
#endif

    if (breath) {
        if (green) {
            mGreenBreath << breath << std::endl;  // green breath for notifications only
        }
        if (red) {
            mRedBreath << breath << std::endl;  // red breath for battery only
        }
    } else {
        mRedLed << red << std::endl;
        mGreenLed << green << std::endl;
    }
}

}  // namespace light
}  // namespace hardware
}  // namespace android
}  // namespace aidl
