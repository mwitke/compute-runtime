/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/helpers/simd_helper_tests_pvc_and_later.inl"

using namespace NEO;

using TestSimdConfigSet = ::testing::Test;

XE_HPC_CORETEST_F(TestSimdConfigSet, GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturnedXeHpcCore) {
    GivenSimdSizeWhenGetSimdConfigCalledThenCorrectEnumReturnedPVCAndLater<typename FamilyType::COMPUTE_WALKER>::testBodyImpl();
}