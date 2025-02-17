/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <string>

namespace NEO {

template <PRODUCT_FAMILY Product>
inline void AILConfigurationHw<Product>::modifyKernelIfRequired(std::string &kernel) {
}

//  To avoid a known oneDNN issue in ZEBin handling,
//  fall back to legacy (patchtoken) format when dummy kernel used by nGen is detected.
//  Only this specific kernel with that exact source code will be affected.

template <PRODUCT_FAMILY Product>
inline void AILConfigurationHw<Product>::forceFallbackToPatchtokensIfRequired(const std::string &kernelSources, bool &requiresFallback) {
    std::string_view dummyKernelSource{"kernel void _(){}"};
    if (sourcesContain(kernelSources, dummyKernelSource)) {
        requiresFallback = true;
    }
}

template <PRODUCT_FAMILY Product>
inline void AILConfigurationHw<Product>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}

} // namespace NEO
