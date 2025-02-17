/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/gen12lp/hw_info_dg1.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_bdw_and_later.inl"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_DG1;

#include "shared/source/gen12lp/dg1/os_agnostic_hw_info_config_dg1.inl"
#include "shared/source/gen12lp/os_agnostic_hw_info_config_gen12lp.inl"

namespace NEO {

template <>
void ProductHelperHw<gfxProduct>::setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) {
    coherencyFlag = false;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
