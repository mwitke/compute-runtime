/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/windows/hw_info_config_win_tests.h"

using namespace NEO;

using Dg2ProductHelperWindows = ProductHelperTestWindows;

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenCheckingIsMultiContextResourceDeferDeletionSupportedThenReturnFalse, IGFX_DG2);

HWTEST2_F(Dg2ProductHelperWindows, givenProductHelperWhenCheckingIsMultiContextResourceDeferDeletionSupportedThenReturnFalse, IsDG2) {
    EXPECT_FALSE(productHelper->isMultiContextResourceDeferDeletionSupported());
}