/*
 * Copyright (C) 2023 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "android.hardware.light-service.asus_sdm660"

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android-base/logging.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Lights.h"

using aidl::android::hardware::light::Lights;

// LCD
const static std::string kLcdBacklightPath = "/sys/class/leds/lcd-backlight/brightness";
const static std::string kLcdMaxBacklightPath = "/sys/class/leds/lcd-backlight/max_brightness";

// Red led
const static std::string kRedBreathPath = "/sys/class/leds/red/breath";
const static std::string kRedLedPath = "/sys/class/leds/red/brightness";

// Green led
const static std::string kGreenBreathPath = "/sys/class/leds/green/breath";
const static std::string kGreenLedPath = "/sys/class/leds/green/brightness";

int main() {
    uint32_t lcdMaxBrightness = 255;

    std::ofstream lcdBacklight(kLcdBacklightPath);
    if (!lcdBacklight) {
        LOG(ERROR) << "Failed to open " << kLcdBacklightPath << ", error=" << errno << " ("
                   << strerror(errno) << ")";
        return -errno;
    }

    std::ifstream lcdMaxBacklight(kLcdMaxBacklightPath);
    if (!lcdMaxBacklight) {
        LOG(ERROR) << "Failed to open " << kLcdMaxBacklightPath << ", error=" << errno << " ("
                   << strerror(errno) << ")";
        return -errno;
    } else {
        lcdMaxBacklight >> lcdMaxBrightness;
    }

    std::ofstream redBreath(kRedBreathPath);
    if (!redBreath) {
        LOG(ERROR) << "Failed to open " << kRedBreathPath << ", error=" << errno << " ("
                   << strerror(errno) << ")";
        return -errno;
    }

    std::ofstream redLed(kRedLedPath);
    if (!redLed) {
        LOG(ERROR) << "Failed to open " << kRedLedPath << ", error=" << errno << " ("
                   << strerror(errno) << ")";
        return -errno;
    }

    std::ofstream greenBreath(kGreenBreathPath);
    if (!greenBreath) {
        LOG(ERROR) << "Failed to open " << kGreenBreathPath << ", error=" << errno << " ("
                   << strerror(errno) << ")";
        return -errno;
    }

    std::ofstream greenLed(kGreenLedPath);
    if (!greenLed) {
        LOG(ERROR) << "Failed to open " << kGreenLedPath << ", error=" << errno << " ("
                   << strerror(errno) << ")";
        return -errno;
    }

    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<Lights> lights = ndk::SharedRefBase::make<Lights>(
            std::make_pair(std::move(lcdBacklight), lcdMaxBrightness), std::move(redBreath),
                  std::move(redLed), std::move(greenBreath), std::move(greenLed));

    const std::string instance = std::string() + Lights::descriptor + "/default";
    binder_status_t status = AServiceManager_addService(lights->asBinder().get(), instance.c_str());
    CHECK(status == STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE; // should not reach
}
