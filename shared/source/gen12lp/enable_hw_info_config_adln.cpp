/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adln.h"
#include "shared/source/gen12lp/hw_info_adln.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

static EnableProductProductHelper<IGFX_ALDERLAKE_N> enableADLN;

} // namespace NEO
