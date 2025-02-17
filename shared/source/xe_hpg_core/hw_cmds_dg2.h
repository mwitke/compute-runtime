/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/xe_hpg_core/dg2/definitions/device_ids_configs_dg2_base.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "device_ids_configs_dg2.h"

#include <algorithm>

namespace NEO {

constexpr uint16_t REV_ID_A0 = 0;
constexpr uint16_t REV_ID_A1 = 1;
constexpr uint16_t REV_ID_B0 = 4;
constexpr uint16_t REV_ID_B1 = 5;
constexpr uint16_t REV_ID_C0 = 8;

struct DG2 : public XeHpgCoreFamily {
    static const PLATFORM platform;
    static const HardwareInfo hwInfo;
    static const uint64_t defaultHardwareInfoConfig;
    static FeatureTable featureTable;
    static WorkaroundTable workaroundTable;
    // Initial non-zero values for unit tests
    static const uint32_t threadsPerEu = 8;
    static const uint32_t maxEuPerSubslice = 16;
    static const uint32_t maxSlicesSupported = 8;
    static const uint32_t maxSubslicesSupported = 32;
    static const uint32_t maxDualSubslicesSupported = 32;
    static const RuntimeCapabilityTable capabilityTable;
    static void (*setupHardwareInfo)(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig);
    static void setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo);
    static void adjustHardwareInfo(HardwareInfo *hwInfo);
    static void setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);

    static bool isG10(const HardwareInfo &hwInfo) {
        auto it = std::find(dg2G10DeviceIds.begin(), dg2G10DeviceIds.end(), hwInfo.platform.usDeviceID);
        return it != dg2G10DeviceIds.end();
    }

    static bool isG11(const HardwareInfo &hwInfo) {
        auto it = std::find(dg2G11DeviceIds.begin(), dg2G11DeviceIds.end(), hwInfo.platform.usDeviceID);
        return it != dg2G11DeviceIds.end();
    }

    static bool isG12(const HardwareInfo &hwInfo) {
        auto it = std::find(dg2G12DeviceIds.begin(), dg2G12DeviceIds.end(), hwInfo.platform.usDeviceID);
        return it != dg2G12DeviceIds.end();
    }
};

class Dg2HwConfig : public DG2 {
  public:
    static void setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);
    static const HardwareInfo hwInfo;

  private:
    static GT_SYSTEM_INFO gtSystemInfo;
};

#include "hw_cmds_dg2.inl"

} // namespace NEO
