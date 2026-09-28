# Contributing to RodinHaptics

Contributions are welcome.

RodinHaptics is intended to provide a clean, source-based vibrator HAL implementation for Xiaomi POCO X7 Pro / Redmi Turbo 4 (`rodin`) and to serve as a base for further AOSP development.

## Before submitting a change

Please describe:

- What was changed.
- Why the change is needed.
- Which device and ROM were used for testing.
- Which Android version was tested.
- Whether the change affects `perform()`, `on()`, amplitude handling, generic vibration, or effect mapping.
- The result of real-device testing.

## Hardware validation

Changes affecting haptic behaviour should be tested on real `rodin` hardware.

Please avoid submitting tuning values or behavioural changes based only on generic Linux force-feedback assumptions.

The `si_haptic` driver behaviour on `rodin` should be verified directly on the device.

## Areas welcome for development

Examples include:

- Native double-click sequencing.
- AIDL composition support.
- Composite primitive support.
- Improved thread safety.
- Additional calibrated haptic profiles.
- Better test coverage.
- Integration improvements for different AOSP ROMs.
- Reuse on related Xiaomi / MediaTek devices.

## Pull requests

Keep changes focused and documented.

If a change alters existing validated `rodin` behaviour, explain the reason and include before/after testing results.

Source-only contributions are preferred.
