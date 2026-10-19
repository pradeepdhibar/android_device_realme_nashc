TARGET_INCLUDE_AXFX := true
#
# SPDX-FileCopyrightText: 2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
# Disable Axion kernel modules before product inheritance
TARGET_SHIPS_AXION_KERNEL_MODULES := false

$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from device makefile.
$(call inherit-product, device/realme/nashc/device.mk)

# Inherit some common LineageOS stuff.
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

PRODUCT_NAME := lineage_nashc
PRODUCT_DEVICE := nashc
PRODUCT_MANUFACTURER := realme
PRODUCT_BRAND := Realme
PRODUCT_MODEL := RMX3085

PRODUCT_GMS_CLIENTID_BASE := android-realme

PRODUCT_BUILD_PROP_OVERRIDES += \
    BuildFingerprint=realme/RMX3085/RMX3085L1:13/SP1A.210812.016/R.1913398-178f6:user/release-keys \
    DeviceName=RMX3085L1 \
    DeviceProduct=RMX3085L1 \
    SystemDevice=RMX3085L1 \
    SystemName=RMX3085

# AxionOS device information
AXION_CAMERA_REAR_INFO := 64,8,2,2
AXION_CAMERA_FRONT_INFO := 16
AXION_MAINTAINER := Pradeep_Dhibar
AXION_PROCESSOR := MediaTek_Helio_G95
