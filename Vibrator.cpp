/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * RodinHaptics modifications Copyright (c) 2026 NEESCHAL.
 */

#define LOG_TAG "RodinHaptics"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <log/log.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <thread>

#include "Vibrator.h"

namespace aidl::android::hardware::vibrator {
namespace {

constexpr int32_t kStrongMagnitude = 0xafff;
constexpr int32_t kMediumMagnitude = 0x8fff;
constexpr int32_t kLightMagnitude = 0x5fff;
constexpr int32_t kMinGenericMagnitude = 0x0800;

constexpr int kInvalidValue = -1;
constexpr int kCustomDataLen = 3;
constexpr int kNameBufferSize = 32;

constexpr int kTinyTouchMaxDurationMs = 10;
constexpr int kShortAccentMaxDurationMs = 40;
constexpr int kShortUiMaxDurationMs = 80;
constexpr int kUiReferenceAmplitude = 13;

// Native si_haptic firmware effect IDs validated on rodin.
constexpr int kNativeClick = 7;
constexpr int kNativeDoubleClick = 8;
constexpr int kNativeTick = 7;
constexpr int kNativeThud = 6;
constexpr int kNativePop = 9;
constexpr int kNativeHeavyClick = 6;

bool testBit(int bit, const uint8_t* array) {
    return array[bit / 8] & (1U << (bit % 8));
}

bool isSupportedHapticDevice(const char* name) {
    return strcmp(name, "qcom-hv-haptics") == 0 ||
           strcmp(name, "qti-haptics") == 0 ||
           strcmp(name, "aw8697_haptic") == 0 ||
           strcmp(name, "awinic_haptic") == 0 ||
           strcmp(name, "si_haptic") == 0;
}

int toNativeEffectId(Effect effect) {
    switch (effect) {
        case Effect::CLICK:
            return kNativeClick;
        case Effect::DOUBLE_CLICK:
            return kNativeDoubleClick;
        case Effect::TICK:
            return kNativeTick;
        case Effect::THUD:
            return kNativeThud;
        case Effect::POP:
            return kNativePop;
        case Effect::HEAVY_CLICK:
            return kNativeHeavyClick;
        default:
            return kInvalidValue;
    }
}

}  // namespace

InputFFDevice::InputFFDevice()
    : mSupportGain(false),
      mSupportEffects(false),
      mVibraFd(kInvalidValue),
      mCurrAppId(kInvalidValue),
      mCurrMagnitude(kMediumMagnitude),
      mUiPulseMode(false),
      mUiAmplitudeLatched(false),
      mUiBaseMagnitude(kMediumMagnitude) {
    DIR* dp;
    struct dirent* dir;
    uint8_t ffBitmask[(FF_CNT + 7) / 8] = {};
    char devicename[PATH_MAX];
    constexpr char kInputDir[] = "/dev/input/";
    char name[kNameBufferSize] = {};

    dp = opendir(kInputDir);
    if (dp == nullptr) {
        ALOGE("Failed to open %s: %s", kInputDir, strerror(errno));
        return;
    }

    while ((dir = readdir(dp)) != nullptr) {
        if (dir->d_name[0] == '.' &&
            (dir->d_name[1] == '\0' ||
             (dir->d_name[1] == '.' && dir->d_name[2] == '\0'))) {
            continue;
        }

        snprintf(devicename, sizeof(devicename), "%s%s", kInputDir, dir->d_name);
        const int fd = TEMP_FAILURE_RETRY(open(devicename, O_RDWR));
        if (fd < 0) {
            continue;
        }

        memset(name, 0, sizeof(name));
        if (TEMP_FAILURE_RETRY(ioctl(fd, EVIOCGNAME(sizeof(name)), name)) == -1) {
            close(fd);
            continue;
        }

        if (!isSupportedHapticDevice(name)) {
            close(fd);
            continue;
        }

        memset(ffBitmask, 0, sizeof(ffBitmask));
        if (TEMP_FAILURE_RETRY(ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffBitmask)), ffBitmask)) == -1) {
            ALOGE("EVIOCGBIT failed for %s: %s", devicename, strerror(errno));
            close(fd);
            continue;
        }

        if (!testBit(FF_CONSTANT, ffBitmask) && !testBit(FF_PERIODIC, ffBitmask)) {
            close(fd);
            continue;
        }

        mVibraFd = fd;
        mSupportEffects = testBit(FF_CUSTOM, ffBitmask);
        mSupportGain = testBit(FF_GAIN, ffBitmask);
        ALOGI("Using haptic input device %s (%s)", name, devicename);
        break;
    }

    closedir(dp);
}

int InputFFDevice::play(int effectId, uint32_t timeoutMs, long* playLengthMs) {
    struct ff_effect effect = {};
    struct input_event play = {};
    int16_t data[kCustomDataLen] = {0, 0, 0};
    int ret;

    // Preserve the stock QMAA behavior: absence of a vibrator device is not a
    // service-level error.
    if (mVibraFd == kInvalidValue) {
        if (playLengthMs != nullptr) {
            *playLengthMs = 0;
        }
        return 0;
    }

    if (timeoutMs != 0) {
        if (mCurrAppId != kInvalidValue) {
            ret = TEMP_FAILURE_RETRY(ioctl(mVibraFd, EVIOCRMFF, mCurrAppId));
            if (ret == -1) {
                ALOGE("EVIOCRMFF failed: %s", strerror(errno));
                goto error;
            }
            mCurrAppId = kInvalidValue;
        }

        if (effectId != kInvalidValue) {
            data[0] = static_cast<int16_t>(effectId);
            effect.type = FF_PERIODIC;
            effect.u.periodic.waveform = FF_CUSTOM;

            // The rodin si_haptic driver consumes the raw 16-bit magnitude
            // pattern in this field. Values above 0x7fff are intentional and
            // match the hand-validated RodinHaptics calibration.
            effect.u.periodic.magnitude = static_cast<int16_t>(mCurrMagnitude);
            effect.u.periodic.custom_data = data;
            effect.u.periodic.custom_len = sizeof(data);
        } else {
            effect.type = FF_CONSTANT;
            effect.u.constant.level = static_cast<int16_t>(mCurrMagnitude);
            effect.replay.length = timeoutMs;
        }

        effect.id = mCurrAppId;
        effect.replay.delay = 0;

        ret = TEMP_FAILURE_RETRY(ioctl(mVibraFd, EVIOCSFF, &effect));
        if (ret == -1) {
            ALOGE("EVIOCSFF failed: %s", strerror(errno));
            goto error;
        }

        mCurrAppId = effect.id;
        if (effectId != kInvalidValue && playLengthMs != nullptr) {
            *playLengthMs = data[1] * 1000L + data[2];
        }

        play.type = EV_FF;
        play.code = mCurrAppId;
        play.value = 1;
        ret = TEMP_FAILURE_RETRY(write(mVibraFd, &play, sizeof(play)));
        if (ret == -1) {
            ALOGE("Failed to start force-feedback effect: %s", strerror(errno));
            TEMP_FAILURE_RETRY(ioctl(mVibraFd, EVIOCRMFF, mCurrAppId));
            goto error;
        }
    } else if (mCurrAppId != kInvalidValue) {
        ret = TEMP_FAILURE_RETRY(ioctl(mVibraFd, EVIOCRMFF, mCurrAppId));
        if (ret == -1) {
            ALOGE("EVIOCRMFF failed: %s", strerror(errno));
            goto error;
        }
        mCurrAppId = kInvalidValue;
    }

    return 0;

error:
    mCurrAppId = kInvalidValue;
    return ret;
}

int InputFFDevice::on(int32_t timeoutMs) {
    long duration = 0;

    // Short Android UI waveforms sound cleaner on rodin when translated to
    // the actuator's native firmware effects instead of FF_CONSTANT.
    if (mSupportEffects && timeoutMs > 0 && timeoutMs <= kShortUiMaxDurationMs) {
        int effectId;

        if (timeoutMs <= kTinyTouchMaxDurationMs) {
            effectId = kNativeTick;
            mUiBaseMagnitude = kLightMagnitude;
        } else if (timeoutMs <= kShortAccentMaxDurationMs) {
            effectId = kNativeDoubleClick;
            mUiBaseMagnitude = kMediumMagnitude;
        } else {
            effectId = kNativeTick;
            mUiBaseMagnitude = kLightMagnitude;
        }

        mUiPulseMode = true;
        mUiAmplitudeLatched = false;
        mCurrMagnitude = mUiBaseMagnitude;
        return play(effectId, kInvalidValue, &duration);
    }

    mUiPulseMode = false;
    mUiAmplitudeLatched = false;
    return play(kInvalidValue, timeoutMs, nullptr);
}

int InputFFDevice::off() {
    mUiPulseMode = false;
    mUiAmplitudeLatched = false;
    return play(kInvalidValue, 0, nullptr);
}

int InputFFDevice::writeGain(int32_t magnitude) {
    struct input_event event = {};
    event.type = EV_FF;
    event.code = FF_GAIN;
    event.value = magnitude;

    const int ret = TEMP_FAILURE_RETRY(write(mVibraFd, &event, sizeof(event)));
    if (ret == -1) {
        ALOGE("Failed to write FF_GAIN: %s", strerror(errno));
        return ret;
    }

    mCurrMagnitude = magnitude;
    return 0;
}

int InputFFDevice::setAmplitude(uint8_t amplitude) {
    if (mVibraFd == kInvalidValue) {
        return 0;
    }

    if (mUiPulseMode) {
        // Android's first amplitude point selects the strength for the native
        // pulse. Later envelope points are intentionally ignored; replaying
        // them through FF_GAIN makes the actuator feel buzzy instead of crisp.
        if (mUiAmplitudeLatched) {
            return 0;
        }

        int32_t magnitude =
                (mUiBaseMagnitude * static_cast<int32_t>(amplitude) * 3 +
                 kUiReferenceAmplitude) /
                (kUiReferenceAmplitude * 2);

        if (magnitude < kMinGenericMagnitude) {
            magnitude = kMinGenericMagnitude;
        } else if (magnitude > kStrongMagnitude) {
            magnitude = kStrongMagnitude;
        }

        const int ret = writeGain(magnitude);
        if (ret == 0) {
            mUiAmplitudeLatched = true;
        }
        return ret;
    }

    // Generic/long waveforms remain generic. This intentionally preserves
    // Android's existing notification, ringtone and application timing.
    int32_t magnitude =
            static_cast<int32_t>(amplitude) * kStrongMagnitude / 255;
    if (magnitude < kMinGenericMagnitude) {
        magnitude = kMinGenericMagnitude;
    }

    return writeGain(magnitude);
}

int InputFFDevice::playEffect(int effectId, EffectStrength strength, long* playLengthMs) {
    mUiPulseMode = false;
    mUiAmplitudeLatched = false;

    switch (strength) {
        case EffectStrength::LIGHT:
            mCurrMagnitude = kLightMagnitude;
            break;
        case EffectStrength::MEDIUM:
            mCurrMagnitude = kMediumMagnitude;
            break;
        case EffectStrength::STRONG:
            mCurrMagnitude = kStrongMagnitude;
            break;
        default:
            return -1;
    }

    return play(effectId, kInvalidValue, playLengthMs);
}

ndk::ScopedAStatus Vibrator::getCapabilities(int32_t* _aidl_return) {
    *_aidl_return = IVibrator::CAP_ON_CALLBACK;

    if (ff.mSupportGain) {
        *_aidl_return |= IVibrator::CAP_AMPLITUDE_CONTROL;
    }
    if (ff.mSupportEffects) {
        *_aidl_return |= IVibrator::CAP_PERFORM_CALLBACK;
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::off() {
    if (ff.off() != 0) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::on(
        int32_t timeoutMs, const std::shared_ptr<IVibratorCallback>& callback) {
    if (ff.on(timeoutMs) != 0) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
    }

    if (callback != nullptr) {
        std::thread([callback, timeoutMs] {
            usleep(timeoutMs * 1000);
            if (!callback->onComplete().isOk()) {
                ALOGE("Failed to invoke on() completion callback");
            }
        }).detach();
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::perform(
        Effect effect, EffectStrength strength,
        const std::shared_ptr<IVibratorCallback>& callback, int32_t* _aidl_return) {
    const int nativeEffectId = toNativeEffectId(effect);
    if (nativeEffectId == kInvalidValue) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }

    if (strength != EffectStrength::LIGHT && strength != EffectStrength::MEDIUM &&
        strength != EffectStrength::STRONG) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }

    long playLengthMs = 0;
    if (ff.playEffect(nativeEffectId, strength, &playLengthMs) != 0) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
    }

    if (callback != nullptr) {
        std::thread([callback, playLengthMs] {
            usleep(playLengthMs * 1000);
            if (!callback->onComplete().isOk()) {
                ALOGE("Failed to invoke perform() completion callback");
            }
        }).detach();
    }

    *_aidl_return = playLengthMs;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedEffects(std::vector<Effect>* _aidl_return) {
    *_aidl_return = {Effect::CLICK, Effect::DOUBLE_CLICK, Effect::TICK,
                     Effect::THUD, Effect::POP, Effect::HEAVY_CLICK};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setAmplitude(float amplitude) {
    if (amplitude <= 0.0f || amplitude > 1.0f) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
    }

    const auto scaledAmplitude = static_cast<uint8_t>(amplitude * 0xff);
    if (ff.setAmplitude(scaledAmplitude) != 0) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setExternalControl(bool /* enabled */) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getCompositionDelayMax(int32_t* /* maxDelayMs */) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getCompositionSizeMax(int32_t* /* maxSize */) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getSupportedPrimitives(
        std::vector<CompositePrimitive>* /* supported */) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getPrimitiveDuration(
        CompositePrimitive /* primitive */, int32_t* /* durationMs */) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::compose(
        const std::vector<CompositeEffect>& /* composite */,
        const std::shared_ptr<IVibratorCallback>& /* callback */) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getSupportedAlwaysOnEffects(
        std::vector<Effect>* /* _aidl_return */) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::alwaysOnEnable(
        int32_t /* id */, Effect /* effect */, EffectStrength /* strength */) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::alwaysOnDisable(int32_t /* id */) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

}  // namespace aidl::android::hardware::vibrator
