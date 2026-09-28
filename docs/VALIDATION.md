# Validation Checklist

RodinHaptics is source-first. Every ROM/device-tree integration should validate
the source-built HAL on physical hardware before shipping.

## Baseline

Expected rodin target:

```text
input device name: si_haptic
AIDL instance:     android.hardware.vibrator.IVibrator/default
init service:      vendor.qti.vibrator
SELinux domain:    u:r:hal_vibrator_default:s0
```

On the validated device, the supported capability bitmask is expected to be:

```text
ON_CALLBACK + PERFORM_CALLBACK + AMPLITUDE_CONTROL = 7
```

## Hardware-validated behavior

| Area | Status | Notes |
| --- | --- | --- |
| Gboard typing | PASS | CLICK/TICK path feels crisp |
| CLICK | PASS | native ID 7 |
| TICK | PASS | native ID 7 |
| THUD | PASS | native ID 6 retained |
| POP | PASS | native ID 9 retained |
| HEAVY_CLICK | PASS | native ID 6 retained |
| Brightness slider | PASS | translated short-waveform path |
| Volume slider | PASS | translated short-waveform path |
| Settings touch/toggles | PASS | framework requests reach the HAL |
| Real notifications | PASS | generic app/channel cadence preserved |
| SELinux enforcing startup | PASS | no permissive workaround required |

## Integration gates

### 1. Soong build

```bash
m vendor.qti.hardware.vibrator.service.rodin
```

The module is built with `-Wall -Werror`. Do not hide integration warnings by
adding local warning suppressions.

### 2. Service startup

```bash
adb shell getprop init.svc.vendor.qti.vibrator
adb shell ps -AZ | grep vendor.qti.hardware.vibrator.service.rodin
adb shell service list | grep -i vibrator
```

Expected init state:

```text
running
```

### 3. SELinux

```bash
adb shell ls -lZ /vendor/bin/hw/vendor.qti.hardware.vibrator.service.rodin
adb shell logcat -b all -d | grep -Ei 'avc:.*denied.*vibrator|hal_vibrator'
```

Do not validate by switching the device to permissive mode.

### 4. Prebaked effects

```bash
adb shell idlcli vibrator perform -b 0 1   # CLICK, MEDIUM
adb shell idlcli vibrator perform -b 1 1   # DOUBLE_CLICK, MEDIUM
adb shell idlcli vibrator perform -b 2 1   # TICK, MEDIUM
adb shell idlcli vibrator perform -b 3 1   # THUD, MEDIUM
adb shell idlcli vibrator perform -b 4 1   # POP, MEDIUM
adb shell idlcli vibrator perform -b 5 1   # HEAVY_CLICK, MEDIUM
```

### 5. UI regression

Verify manually:

- keyboard typing;
- brightness slider;
- volume slider;
- launcher/SystemUI feedback;
- Settings toggles that request haptic feedback.

### 6. Generic-waveform regression

Receive a real notification and verify that the app/channel timing remains
intact. Generic long requests should not be collapsed into synthetic short
clicks.

Also test at least one long-duration vibration from another application.

### 7. Cancellation

Start a long vibration, cancel it and confirm the actuator stops immediately and
the HAL service remains alive.

## Not implemented / not advertised

The current source does not claim support for:

- primitive composition;
- advanced frequency/envelope APIs;
- external control;
- always-on effects;
- a synthesized two-pulse DOUBLE_CLICK sequence.

`DOUBLE_CLICK` currently maps to a single rodin-native firmware effect ID.
