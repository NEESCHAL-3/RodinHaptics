# Architecture

## Design goal

RodinHaptics is deliberately small. It does not replace Android vibration
policy, notification patterns, IME policy or SystemUI logic. It implements the
AIDL vibrator HAL and adapts requests to the rodin `si_haptic` force-feedback
interface.

## Runtime path

```text
VibrationEffect / performHapticFeedback
        -> VibratorManagerService
        -> android.hardware.vibrator.IVibrator/default
        -> Vibrator
        -> InputFFDevice
        -> EVIOCSFF / EV_FF / FF_GAIN
        -> si_haptic
```

## Input device discovery

The HAL scans `/dev/input` at startup and identifies the haptic device by the
kernel-reported input name. The rodin target is `si_haptic`; event numbering is
not assumed because `/dev/input/eventN` can change between builds or boots.

Compatible names retained from the vendor base are:

```text
qcom-hv-haptics
qti-haptics
aw8697_haptic
awinic_haptic
si_haptic
```

## Prebaked effects

AIDL effects are translated to rodin native firmware IDs before `FF_CUSTOM` is
submitted through an `FF_PERIODIC` effect.

```text
CLICK       -> 7
DOUBLE_CLICK-> 8
TICK        -> 7
THUD        -> 6
POP         -> 9
HEAVY_CLICK -> 6
```

The driver writes the real play duration back through the custom-data array;
that duration is returned to Android and used for the completion callback.

## Short UI waveform translation

Android often represents touch feedback as tiny waveform segments rather than a
prebaked effect. On rodin, direct `FF_CONSTANT` playback for these very short
segments felt less precise than the native firmware patterns.

RodinHaptics therefore translates only segments up to 80 ms:

```text
 1-10 ms -> native ID 7, light base
11-40 ms -> native ID 8, medium base
41-80 ms -> native ID 7, light base
```

The first framework amplitude update is treated as the strength signal. A
calibrated 1.5x compensation is applied and clamped to the release magnitude
range. Later envelope points from that same short request are ignored.

This behavior is intentionally narrow. It avoids reinterpreting long application
or notification vibrations.

## Generic waveforms

Requests longer than 80 ms use the generic `FF_CONSTANT` path. Framework
amplitude is converted to `FF_GAIN` and Android retains responsibility for the
pattern cadence.

This is why notification patterns are not hard-coded into RodinHaptics.
Different apps/channels may legitimately send different timings.

## Magnitude representation

The validated rodin calibration uses values up to `0xafff`. Linux `ff_effect`
periodic/constant magnitude fields are 16-bit signed fields, but the rodin
`si_haptic` path consumes the raw bit pattern used by the tested vendor-derived
implementation. The explicit `static_cast<int16_t>` in `play()` is therefore
intentional; do not "fix" it without hardware validation.

## Capability policy

RodinHaptics reports only capabilities backed by the current implementation:

- on callback;
- amplitude control when `FF_GAIN` is available;
- perform callback when `FF_CUSTOM` is available.

Composition/primitives and advanced frequency APIs are not advertised in v1.
