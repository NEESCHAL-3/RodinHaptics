# Changelog

## Initial public source

- added rodin-specific AIDL vibrator HAL source;
- added dynamic `/dev/input` discovery including `si_haptic`;
- added validated Android-to-native effect mapping;
- added calibrated LIGHT/MEDIUM/STRONG magnitudes;
- added short-UI waveform translation for improved keyboard/SystemUI haptics;
- preserved generic and long-duration Android/app waveforms;
- retained standard Android vibrator AIDL instance and stock-compatible init
  service/path for straightforward device-tree replacement;
- documented AOSP/device-tree integration, validation and upstream differences;
- no prebuilt HAL binary is distributed.
