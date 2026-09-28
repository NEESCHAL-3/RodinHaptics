# Upstream / Vendor Base Differences

RodinHaptics started from the rodin vendor AIDL vibrator service and intentionally
keeps its binder contract, init service name and VINTF instance.

## Functional changes

1. **Rodin-calibrated magnitude range**
   - base: `0x3fff / 0x5fff / 0x7fff`
   - RodinHaptics: `0x5fff / 0x8fff / 0xafff`

2. **Short UI translator**
   - base: short `on()` requests use `FF_CONSTANT`;
   - RodinHaptics: requests up to 80 ms use calibrated native firmware effects.

3. **Framework intensity handling**
   - first amplitude point selects translated short-effect strength;
   - a 1.5x calibrated compensation is applied;
   - later envelope points are ignored only for the translated short request.

4. **Prebaked effect mapping**
   - Android enum ordinal is no longer passed directly to the driver;
   - each supported effect maps to a validated rodin native firmware ID.

5. **Generic waveform preservation**
   - long/generic requests continue to use `FF_CONSTANT`;
   - notification/app cadence is not hard-coded by the HAL.

6. **Source cleanup**
   - experimental trace logging removed;
   - temporary notification translator removed;
   - abandoned gain experiment removed;
   - AIDL `override` declarations completed;
   - error paths and comments normalized.

## Deliberately unchanged

- AIDL instance: `android.hardware.vibrator.IVibrator/default`;
- init service: `vendor.qti.vibrator`;
- installed binary path;
- supported standard effect list;
- unsupported composition/always-on/external-control behavior.
