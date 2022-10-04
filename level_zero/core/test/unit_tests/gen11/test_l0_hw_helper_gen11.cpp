/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {
namespace ult {

using L0HwHelperTestGen11 = ::testing::Test;

GEN11TEST_F(L0HwHelperTestGen11, GivenGen11WhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnFalse) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    EXPECT_FALSE(L0::L0HwHelperHw<FamilyType>::get().platformSupportsCmdListHeapSharing(hwInfo));
}

GEN11TEST_F(L0HwHelperTestGen11, GivenGen11WhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnFalse) {
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo;
    EXPECT_FALSE(L0::L0HwHelperHw<FamilyType>::get().platformSupportsStateComputeModeTracking(hwInfo));
}

} // namespace ult
} // namespace L0
