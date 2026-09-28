# RodinHaptics

RodinHaptics is a source implementation of the Android AIDL vibrator HAL for
Xiaomi POCO X7 Pro / Redmi Turbo 4 (`rodin`) using the Linux `si_haptic`
force-feedback interface.

The project is intended for ROM/device-tree maintainers who want to build the
HAL directly as part of AOSP. **No prebuilt vibrator binary is distributed.**

## Goals

- keep the standard Android vibrator framework and AIDL interface;
- discover the vibrator input device dynamically by name instead of hard-coding
  `/dev/input/eventN`;
- map Android prebaked effects to rodin-native firmware effects;
- improve short UI haptics such as keyboard, touch and SystemUI slider feedback;
- preserve Android/app timing for generic and long-duration waveforms;
- advertise only the capabilities actually implemented by the HAL.

## Data path

```text
Apps / SystemUI / IME
        |
Android VibratorManager / VibratorService
        |
android.hardware.vibrator.IVibrator/default
        |
vendor.qti.hardware.vibrator.service.rodin
        |
InputFFDevice
        |
Linux force-feedback API
        |
si_haptic
```

## Source layout

```text
RodinHaptics/
├── Android.bp
├── Vibrator.cpp
├── Vibrator.h
├── service.cpp
├── vendor.qti.hardware.vibrator.service.rodin.rc
├── vendor.qti.hardware.vibrator.service.rodin.xml
├── README.md
├── LICENSE
├── NOTICE
├── CHANGELOG.md
├── docs/
│   ├── ARCHITECTURE.md
│   ├── INTEGRATION.md
│   ├── VALIDATION.md
│   └── UPSTREAM_DIFF.md
└── patches/
    └── vendor-base-to-RodinHaptics.patch
```

## Supported hardware

The calibration in this repository is for `rodin` and has been validated with
an input device named:

```text
si_haptic
```

The device scanner also retains the compatible names used by the vendor base:

```text
qcom-hv-haptics
qti-haptics
aw8697_haptic
awinic_haptic
si_haptic
```

The additional names are discovery fallbacks only. The rodin effect IDs and
magnitude calibration are not claimed to be correct for other devices.

## Native effect mapping

| Android AIDL effect | rodin native effect ID |
| --- | ---: |
| `CLICK` | `7` |
| `DOUBLE_CLICK` | `8` |
| `TICK` | `7` |
| `THUD` | `6` |
| `POP` | `9` |
| `HEAVY_CLICK` | `6` |

Current calibrated magnitudes:

```text
LIGHT       0x5fff
MEDIUM      0x8fff
STRONG      0xafff
GENERIC_MIN 0x0800
```

Short Android UI waveform segments up to 80 ms are translated to native rodin
firmware effects. The first framework amplitude sample is used as the strength
signal and later samples in that translated pulse are ignored to preserve a
crisp actuator response.

Generic and long-duration requests remain on the ordinary `FF_CONSTANT` path so
notification, ringtone and application timing is not replaced with synthetic
click patterns.

## Implemented AIDL capabilities

The service exposes capabilities according to what the input device reports:

- `CAP_ON_CALLBACK`;
- `CAP_PERFORM_CALLBACK` when `FF_CUSTOM` is available;
- `CAP_AMPLITUDE_CONTROL` when `FF_GAIN` is available.

The source intentionally does **not** advertise unsupported advanced features.
Primitive composition, external control, always-on effects and frequency
profiles remain unsupported.

## AOSP / device-tree integration

Recommended source location:

```text
hardware/neeschal/rodin_haptics/
```

Add the module to the target product:

```make
PRODUCT_PACKAGES += \
    vendor.qti.hardware.vibrator.service.rodin
```

Then build:

```bash
m vendor.qti.hardware.vibrator.service.rodin
```

The module installs:

```text
/vendor/bin/hw/vendor.qti.hardware.vibrator.service.rodin
/vendor/etc/init/vendor.qti.hardware.vibrator.service.rodin.rc
/vendor/etc/vintf/manifest/vendor.qti.hardware.vibrator.service.rodin.xml
```

### Important when replacing vendor blobs

If the device tree currently extracts the stock vibrator service, remove the
matching vibrator binary/rc/VINTF blob entries from the proprietary-file list or
otherwise exclude them from the vendor makefiles. Do not ship both the stock
prebuilt and this source-built module.

Likewise, if another source directory already defines the Soong module
`vendor.qti.hardware.vibrator.service.rodin`, remove or disable the duplicate.

The AIDL instance must remain:

```text
android.hardware.vibrator.IVibrator/default
```

See [`docs/INTEGRATION.md`](docs/INTEGRATION.md) for a complete integration
checklist.

## SELinux

The implementation is designed to use the standard vibrator HAL domain and
executable label:

```text
process:    u:r:hal_vibrator_default:s0
executable: u:object_r:hal_vibrator_default_exec:s0
```

Do not add permissive policy as part of the integration. Existing rodin vendor
policy normally already covers the stock service path; inspect actual AVCs if a
specific ROM differs.

## Validation status

The source behavior has been validated on physical `rodin` hardware for:

- service startup under enforcing SELinux;
- Gboard typing;
- Android `CLICK` and `TICK`;
- brightness and volume slider haptics;
- Settings touch/toggle feedback;
- ordinary real-notification waveform behavior;
- effect-strength handling.

The source also passes a standalone Android cross-compile with
`-Wall -Wextra -Werror`. ROM maintainers should still run the normal full Soong
build and device regression tests for their tree.

See [`docs/VALIDATION.md`](docs/VALIDATION.md).

## Development policy

RodinHaptics is intentionally source-first. Maintainers are expected to build
and integrate it in their own AOSP/device tree and may adapt placement, module
naming or product wiring to their tree as long as the vibrator AIDL instance,
init/VINTF wiring and SELinux policy remain consistent.

## License

BSD-3-Clause. Original The Linux Foundation notices are preserved in derived
source files. See [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).

## Maintainer

**NEESCHAL**
