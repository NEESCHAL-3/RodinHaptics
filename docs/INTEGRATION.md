# AOSP / Device-Tree Integration

This document describes source integration. RodinHaptics does not require a
prebuilt HAL binary in the repository.

## 1. Place the source in the Android tree

Recommended path:

```text
hardware/neeschal/rodin_haptics/
```

The exact path is not technically required; it is only a clean convention.

## 2. Remove the stock implementation conflict

RodinHaptics intentionally builds the module:

```text
vendor.qti.hardware.vibrator.service.rodin
```

Before enabling it, make sure the tree does not provide the same service from a
second source or from an extracted prebuilt.

Typical places to check:

```text
proprietary-files.txt
vendor/*/*-vendor.mk
device.mk
product makefiles
other vibrator source directories
```

If the stock service is extracted as blobs, remove/exclude the corresponding
entries for the service executable, init rc and VINTF fragment when those files
would conflict with the source-built output.

Do not build two Soong modules with the same name and do not install two default
vibrator AIDL services.

## 3. Add the package to the product

In the appropriate device/product makefile:

```make
PRODUCT_PACKAGES += \
    vendor.qti.hardware.vibrator.service.rodin
```

No prebuilt copy rule is needed.

## 4. Build the HAL

From the Android build environment:

```bash
m vendor.qti.hardware.vibrator.service.rodin
```

Expected outputs are installed by Soong from `Android.bp`:

```text
/vendor/bin/hw/vendor.qti.hardware.vibrator.service.rodin
/vendor/etc/init/vendor.qti.hardware.vibrator.service.rodin.rc
/vendor/etc/vintf/manifest/vendor.qti.hardware.vibrator.service.rodin.xml
```

## 5. AIDL / VINTF

The included VINTF fragment registers:

```text
android.hardware.vibrator.IVibrator/default
```

Do not add a second `default` vibrator AIDL instance elsewhere in the product.

## 6. Init

The included rc starts:

```text
service vendor.qti.vibrator /vendor/bin/hw/vendor.qti.hardware.vibrator.service.rodin
```

Keeping the stock service name and path minimizes changes for rodin trees that
already carry vendor SELinux policy for this implementation.

## 7. SELinux

Expected labels on the validated setup:

```text
process:    u:r:hal_vibrator_default:s0
executable: u:object_r:hal_vibrator_default_exec:s0
```

Do not disable SELinux and do not add broad allow rules in advance. Build and
boot first, then inspect real AVC denials if the target tree differs.

## 8. Build-order recommendation

A clean maintainer workflow is:

```text
1. Add RodinHaptics source.
2. Remove/disable the conflicting stock prebuilt or source module.
3. Add the PRODUCT_PACKAGES entry.
4. Build only the vibrator HAL.
5. Resolve any tree-specific Soong/VINTF/SELinux issue.
6. Build the complete ROM.
7. Run the hardware validation checklist.
```

## 9. Runtime checks

After boot:

```bash
adb shell getprop init.svc.vendor.qti.vibrator
adb shell ps -AZ | grep vendor.qti.hardware.vibrator.service.rodin
adb shell service list | grep -i vibrator
```

Expected init state:

```text
running
```

Then verify the effect and UI paths described in `VALIDATION.md`.

## Device scope

The implementation is calibrated for Xiaomi POCO X7 Pro / Redmi Turbo 4
(`rodin`). The source is intentionally easy to adapt, but the native effect IDs
and magnitude values should not be copied to another actuator without hardware
validation.
