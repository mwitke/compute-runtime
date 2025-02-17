/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"

#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/os_interface/hw_info_config.h"

using namespace NEO;

void StateComputeModeProperties::setPropertiesAll(bool requiresCoherency, uint32_t numGrfRequired, int32_t threadArbitrationPolicy, PreemptionMode devicePreemptionMode,
                                                  const RootDeviceEnvironment &rootDeviceEnvironment) {
    initSupport(rootDeviceEnvironment);
    clearIsDirty();

    setCoherencyProperty(requiresCoherency);
    setGrfNumberProperty(numGrfRequired);
    setThreadArbitrationProperty(threadArbitrationPolicy, rootDeviceEnvironment);

    int32_t zPassAsyncComputeThreadLimit = -1;
    if (DebugManager.flags.ForceZPassAsyncComputeThreadLimit.get() != -1) {
        zPassAsyncComputeThreadLimit = DebugManager.flags.ForceZPassAsyncComputeThreadLimit.get();
    }
    if (zPassAsyncComputeThreadLimit != -1 && this->scmPropertiesSupport.zPassAsyncComputeThreadLimit) {
        this->zPassAsyncComputeThreadLimit.set(zPassAsyncComputeThreadLimit);
    }

    int32_t pixelAsyncComputeThreadLimit = -1;
    if (DebugManager.flags.ForcePixelAsyncComputeThreadLimit.get() != -1) {
        pixelAsyncComputeThreadLimit = DebugManager.flags.ForcePixelAsyncComputeThreadLimit.get();
    }
    if (pixelAsyncComputeThreadLimit != -1 && this->scmPropertiesSupport.pixelAsyncComputeThreadLimit) {
        this->pixelAsyncComputeThreadLimit.set(pixelAsyncComputeThreadLimit);
    }
    setDevicePreemptionProperty(devicePreemptionMode);

    setPropertiesExtraPerContext();
    setPropertiesExtraPerKernel();
}

void StateComputeModeProperties::setProperties(const StateComputeModeProperties &properties) {
    clearIsDirty();

    isCoherencyRequired.set(properties.isCoherencyRequired.value);
    largeGrfMode.set(properties.largeGrfMode.value);
    zPassAsyncComputeThreadLimit.set(properties.zPassAsyncComputeThreadLimit.value);
    pixelAsyncComputeThreadLimit.set(properties.pixelAsyncComputeThreadLimit.value);
    threadArbitrationPolicy.set(properties.threadArbitrationPolicy.value);
    devicePreemptionMode.set(properties.devicePreemptionMode.value);

    setPropertiesExtra(properties);
}

bool StateComputeModeProperties::isDirty() const {
    return isCoherencyRequired.isDirty || largeGrfMode.isDirty || zPassAsyncComputeThreadLimit.isDirty ||
           pixelAsyncComputeThreadLimit.isDirty || threadArbitrationPolicy.isDirty || devicePreemptionMode.isDirty || isDirtyExtra();
}

void StateComputeModeProperties::clearIsDirty() {
    isCoherencyRequired.isDirty = false;
    largeGrfMode.isDirty = false;
    zPassAsyncComputeThreadLimit.isDirty = false;
    pixelAsyncComputeThreadLimit.isDirty = false;
    threadArbitrationPolicy.isDirty = false;
    devicePreemptionMode.isDirty = false;

    clearIsDirtyExtraPerContext();
    clearIsDirtyExtraPerKernel();
}

void StateComputeModeProperties::setCoherencyProperty(bool requiresCoherency) {
    if (this->scmPropertiesSupport.coherencyRequired) {
        int32_t isCoherencyRequired = (requiresCoherency ? 1 : 0);
        this->isCoherencyRequired.set(isCoherencyRequired);
    }
}
void StateComputeModeProperties::setDevicePreemptionProperty(PreemptionMode devicePreemptionMode) {
    if (this->scmPropertiesSupport.devicePreemptionMode) {
        this->devicePreemptionMode.set(static_cast<int32_t>(devicePreemptionMode));
    }
}

void StateComputeModeProperties::setGrfNumberProperty(uint32_t numGrfRequired) {
    if (this->scmPropertiesSupport.largeGrfMode &&
        (this->largeGrfMode.value == -1 || numGrfRequired != GrfConfig::NotApplicable)) {
        int32_t largeGrfMode = (numGrfRequired == GrfConfig::LargeGrfNumber ? 1 : 0);
        this->largeGrfMode.set(largeGrfMode);
    }
}

void StateComputeModeProperties::setThreadArbitrationProperty(int32_t threadArbitrationPolicy,
                                                              const RootDeviceEnvironment &rootDeviceEnvironment) {
    bool setDefaultThreadArbitrationPolicy = (threadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent) &&
                                             (NEO::DebugManager.flags.ForceDefaultThreadArbitrationPolicyIfNotSpecified.get() ||
                                              (this->threadArbitrationPolicy.value == ThreadArbitrationPolicy::NotPresent));
    if (setDefaultThreadArbitrationPolicy) {
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
        threadArbitrationPolicy = gfxCoreHelper.getDefaultThreadArbitrationPolicy();
    }
    if (DebugManager.flags.OverrideThreadArbitrationPolicy.get() != -1) {
        threadArbitrationPolicy = DebugManager.flags.OverrideThreadArbitrationPolicy.get();
    }
    if (this->scmPropertiesSupport.threadArbitrationPolicy) {
        this->threadArbitrationPolicy.set(threadArbitrationPolicy);
    }
}

void StateComputeModeProperties::initSupport(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (this->propertiesSupportLoaded == false) {
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        productHelper.fillScmPropertiesSupportStructure(this->scmPropertiesSupport);
        this->propertiesSupportLoaded = true;
    }
}

void StateComputeModeProperties::setPropertiesCoherencyDevicePreemption(bool requiresCoherency, PreemptionMode devicePreemptionMode, const RootDeviceEnvironment &rootDeviceEnvironment, bool clearDirtyState) {
    initSupport(rootDeviceEnvironment);

    if (!clearDirtyState) {
        this->isCoherencyRequired.isDirty = false;
        this->devicePreemptionMode.isDirty = false;
        clearIsDirtyExtraPerContext();
    }
    setCoherencyProperty(requiresCoherency);
    setDevicePreemptionProperty(devicePreemptionMode);
    setPropertiesExtraPerContext();
    if (clearDirtyState) {
        this->isCoherencyRequired.isDirty = false;
        this->devicePreemptionMode.isDirty = false;
        clearIsDirtyExtraPerContext();
    }
}

void StateComputeModeProperties::setPropertiesGrfNumberThreadArbitration(uint32_t numGrfRequired, int32_t threadArbitrationPolicy, const RootDeviceEnvironment &rootDeviceEnvironment) {
    initSupport(rootDeviceEnvironment);

    this->threadArbitrationPolicy.isDirty = false;
    this->largeGrfMode.isDirty = false;
    clearIsDirtyExtraPerKernel();

    setGrfNumberProperty(numGrfRequired);
    setThreadArbitrationProperty(threadArbitrationPolicy, rootDeviceEnvironment);
    setPropertiesExtraPerKernel();
}

void FrontEndProperties::initSupport(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (this->propertiesSupportLoaded == false) {
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
        productHelper.fillFrontEndPropertiesSupportStructure(this->frontEndPropertiesSupport, hwInfo);
        this->propertiesSupportLoaded = true;
    }
}

void FrontEndProperties::setPropertiesAll(bool isCooperativeKernel, bool disableEuFusion, bool disableOverdispatch, int32_t engineInstancedDevice, const RootDeviceEnvironment &rootDeviceEnvironment) {
    initSupport(rootDeviceEnvironment);

    clearIsDirty();

    if (this->frontEndPropertiesSupport.computeDispatchAllWalker) {
        this->computeDispatchAllWalkerEnable.set(isCooperativeKernel);
    }

    if (this->frontEndPropertiesSupport.disableEuFusion) {
        this->disableEUFusion.set(disableEuFusion);
    }

    if (this->frontEndPropertiesSupport.disableOverdispatch) {
        this->disableOverdispatch.set(disableOverdispatch);
    }

    if (this->frontEndPropertiesSupport.singleSliceDispatchCcsMode) {
        this->singleSliceDispatchCcsMode.set(engineInstancedDevice);
    }
}

void FrontEndProperties::setPropertySingleSliceDispatchCcsMode(int32_t engineInstancedDevice, const RootDeviceEnvironment &rootDeviceEnvironment) {
    initSupport(rootDeviceEnvironment);

    this->singleSliceDispatchCcsMode.isDirty = false;
    if (this->frontEndPropertiesSupport.singleSliceDispatchCcsMode) {
        this->singleSliceDispatchCcsMode.set(engineInstancedDevice);
    }
}

void FrontEndProperties::setPropertiesDisableOverdispatchEngineInstanced(bool disableOverdispatch, int32_t engineInstancedDevice, const RootDeviceEnvironment &rootDeviceEnvironment, bool clearDirtyState) {
    initSupport(rootDeviceEnvironment);

    if (!clearDirtyState) {
        this->disableOverdispatch.isDirty = false;
        this->singleSliceDispatchCcsMode.isDirty = false;
    }

    if (this->frontEndPropertiesSupport.disableOverdispatch) {
        this->disableOverdispatch.set(disableOverdispatch);
    }
    if (this->frontEndPropertiesSupport.singleSliceDispatchCcsMode) {
        this->singleSliceDispatchCcsMode.set(engineInstancedDevice);
    }

    if (clearDirtyState) {
        this->disableOverdispatch.isDirty = false;
        this->singleSliceDispatchCcsMode.isDirty = false;
    }
}

void FrontEndProperties::setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(bool isCooperativeKernel, bool disableEuFusion, const RootDeviceEnvironment &rootDeviceEnvironment) {
    initSupport(rootDeviceEnvironment);

    this->computeDispatchAllWalkerEnable.isDirty = false;
    this->disableEUFusion.isDirty = false;

    if (this->frontEndPropertiesSupport.computeDispatchAllWalker) {
        this->computeDispatchAllWalkerEnable.set(isCooperativeKernel);
    }

    if (this->frontEndPropertiesSupport.disableEuFusion) {
        this->disableEUFusion.set(disableEuFusion);
    }
}

void FrontEndProperties::setProperties(const FrontEndProperties &properties) {
    clearIsDirty();

    disableOverdispatch.set(properties.disableOverdispatch.value);
    disableEUFusion.set(properties.disableEUFusion.value);
    singleSliceDispatchCcsMode.set(properties.singleSliceDispatchCcsMode.value);
    computeDispatchAllWalkerEnable.set(properties.computeDispatchAllWalkerEnable.value);
}

bool FrontEndProperties::isDirty() const {
    return disableOverdispatch.isDirty || disableEUFusion.isDirty || singleSliceDispatchCcsMode.isDirty ||
           computeDispatchAllWalkerEnable.isDirty;
}

void FrontEndProperties::clearIsDirty() {
    disableEUFusion.isDirty = false;
    disableOverdispatch.isDirty = false;
    singleSliceDispatchCcsMode.isDirty = false;
    computeDispatchAllWalkerEnable.isDirty = false;
}

void PipelineSelectProperties::initSupport(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (this->propertiesSupportLoaded == false) {
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

        productHelper.fillPipelineSelectPropertiesSupportStructure(this->pipelineSelectPropertiesSupport, *rootDeviceEnvironment.getHardwareInfo());
        this->propertiesSupportLoaded = true;
    }
}

void PipelineSelectProperties::setPropertiesAll(bool modeSelected, bool mediaSamplerDopClockGate, bool systolicMode, const RootDeviceEnvironment &rootDeviceEnvironment) {
    initSupport(rootDeviceEnvironment);
    clearIsDirty();

    if (this->pipelineSelectPropertiesSupport.modeSelected) {
        this->modeSelected.set(modeSelected);
    }

    if (this->pipelineSelectPropertiesSupport.mediaSamplerDopClockGate) {
        this->mediaSamplerDopClockGate.set(mediaSamplerDopClockGate);
    }

    if (this->pipelineSelectPropertiesSupport.systolicMode) {
        this->systolicMode.set(systolicMode);
    }
}

void PipelineSelectProperties::setPropertiesModeSelectedMediaSamplerClockGate(bool modeSelected, bool mediaSamplerDopClockGate, const RootDeviceEnvironment &rootDeviceEnvironment, bool clearDirtyState) {
    initSupport(rootDeviceEnvironment);

    if (!clearDirtyState) {
        this->modeSelected.isDirty = false;
        this->mediaSamplerDopClockGate.isDirty = false;
    }

    if (this->pipelineSelectPropertiesSupport.modeSelected) {
        this->modeSelected.set(modeSelected);
    }
    if (this->pipelineSelectPropertiesSupport.mediaSamplerDopClockGate) {
        this->mediaSamplerDopClockGate.set(mediaSamplerDopClockGate);
    }

    if (clearDirtyState) {
        this->modeSelected.isDirty = false;
        this->mediaSamplerDopClockGate.isDirty = false;
    }
}

void PipelineSelectProperties::setPropertySystolicMode(bool systolicMode, const RootDeviceEnvironment &rootDeviceEnvironment) {
    initSupport(rootDeviceEnvironment);

    this->systolicMode.isDirty = false;

    if (this->pipelineSelectPropertiesSupport.systolicMode) {
        this->systolicMode.set(systolicMode);
    }
}

void PipelineSelectProperties::setProperties(const PipelineSelectProperties &properties) {
    clearIsDirty();

    modeSelected.set(properties.modeSelected.value);
    mediaSamplerDopClockGate.set(properties.mediaSamplerDopClockGate.value);
    systolicMode.set(properties.systolicMode.value);
}

bool PipelineSelectProperties::isDirty() const {
    return modeSelected.isDirty || mediaSamplerDopClockGate.isDirty || systolicMode.isDirty;
}

void PipelineSelectProperties::clearIsDirty() {
    modeSelected.isDirty = false;
    mediaSamplerDopClockGate.isDirty = false;
    systolicMode.isDirty = false;
}

void StateBaseAddressProperties::initSupport(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (this->propertiesSupportLoaded == false) {
        auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
        productHelper.fillStateBaseAddressPropertiesSupportStructure(this->stateBaseAddressPropertiesSupport);
        this->propertiesSupportLoaded = true;
    }
}

void StateBaseAddressProperties::setPropertiesSurfaceState(int64_t bindingTablePoolBaseAddress, size_t bindingTablePoolSize,
                                                           int64_t surfaceStateBaseAddress, size_t surfaceStateSize, const RootDeviceEnvironment &rootDeviceEnvironment) {
    initSupport(rootDeviceEnvironment);

    this->bindingTablePoolBaseAddress.isDirty = false;
    this->bindingTablePoolSize.isDirty = false;
    this->surfaceStateBaseAddress.isDirty = false;
    this->surfaceStateSize.isDirty = false;

    if (this->stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress) {
        this->bindingTablePoolBaseAddress.set(bindingTablePoolBaseAddress);
        this->bindingTablePoolSize.set(bindingTablePoolSize);
    }
    this->surfaceStateBaseAddress.set(surfaceStateBaseAddress);
    this->surfaceStateSize.set(surfaceStateSize);
}

void StateBaseAddressProperties::setPropertiesDynamicState(int64_t dynamicStateBaseAddress, size_t dynamicStateSize) {
    this->dynamicStateBaseAddress.isDirty = false;
    this->dynamicStateSize.isDirty = false;
    this->dynamicStateBaseAddress.set(dynamicStateBaseAddress);
    this->dynamicStateSize.set(dynamicStateSize);
}

void StateBaseAddressProperties::setPropertiesIndirectState(int64_t indirectObjectBaseAddress, size_t indirectObjectSize) {
    this->indirectObjectBaseAddress.isDirty = false;
    this->indirectObjectSize.isDirty = false;
    this->indirectObjectBaseAddress.set(indirectObjectBaseAddress);
    this->indirectObjectSize.set(indirectObjectSize);
}

void StateBaseAddressProperties::setPropertyStatelessMocs(int32_t statelessMocs, const RootDeviceEnvironment &rootDeviceEnvironment) {
    initSupport(rootDeviceEnvironment);

    this->statelessMocs.isDirty = false;
    if (this->stateBaseAddressPropertiesSupport.statelessMocs) {
        this->statelessMocs.set(statelessMocs);
    }
}

void StateBaseAddressProperties::setPropertyGlobalAtomics(bool globalAtomics, const RootDeviceEnvironment &rootDeviceEnvironment, bool clearDirtyState) {
    initSupport(rootDeviceEnvironment);

    if (!clearDirtyState) {
        this->globalAtomics.isDirty = false;
    }
    if (this->stateBaseAddressPropertiesSupport.globalAtomics) {
        this->globalAtomics.set(globalAtomics);
    }
    if (clearDirtyState) {
        this->globalAtomics.isDirty = false;
    }
}

void StateBaseAddressProperties::setPropertiesAll(bool globalAtomics, int32_t statelessMocs,
                                                  int64_t bindingTablePoolBaseAddress, size_t bindingTablePoolSize,
                                                  int64_t surfaceStateBaseAddress, size_t surfaceStateSize,
                                                  int64_t dynamicStateBaseAddress, size_t dynamicStateSize,
                                                  int64_t indirectObjectBaseAddress, size_t indirectObjectSize, const RootDeviceEnvironment &rootDeviceEnvironment) {
    initSupport(rootDeviceEnvironment);
    clearIsDirty();

    if (this->stateBaseAddressPropertiesSupport.globalAtomics) {
        this->globalAtomics.set(globalAtomics);
    }

    if (this->stateBaseAddressPropertiesSupport.statelessMocs) {
        this->statelessMocs.set(statelessMocs);
    }

    if (this->stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress) {
        this->bindingTablePoolBaseAddress.set(bindingTablePoolBaseAddress);
        this->bindingTablePoolSize.set(bindingTablePoolSize);
    }

    this->surfaceStateBaseAddress.set(surfaceStateBaseAddress);
    this->surfaceStateSize.set(surfaceStateSize);
    this->dynamicStateBaseAddress.set(dynamicStateBaseAddress);
    this->dynamicStateSize.set(dynamicStateSize);
    this->indirectObjectBaseAddress.set(indirectObjectBaseAddress);
    this->indirectObjectSize.set(indirectObjectSize);
}

void StateBaseAddressProperties::setProperties(const StateBaseAddressProperties &properties) {
    clearIsDirty();

    globalAtomics.set(properties.globalAtomics.value);
    statelessMocs.set(properties.statelessMocs.value);
    bindingTablePoolBaseAddress.set(properties.bindingTablePoolBaseAddress.value);
    bindingTablePoolSize.set(properties.bindingTablePoolSize.value);

    surfaceStateBaseAddress.set(properties.surfaceStateBaseAddress.value);
    surfaceStateSize.set(properties.surfaceStateSize.value);
    dynamicStateBaseAddress.set(properties.dynamicStateBaseAddress.value);
    dynamicStateSize.set(properties.dynamicStateSize.value);
    indirectObjectBaseAddress.set(properties.indirectObjectBaseAddress.value);
    indirectObjectSize.set(properties.indirectObjectSize.value);
}

bool StateBaseAddressProperties::isDirty() const {
    return globalAtomics.isDirty || statelessMocs.isDirty ||
           bindingTablePoolBaseAddress.isDirty || bindingTablePoolSize.isDirty ||
           surfaceStateBaseAddress.isDirty || surfaceStateSize.isDirty ||
           dynamicStateBaseAddress.isDirty || dynamicStateSize.isDirty ||
           indirectObjectBaseAddress.isDirty || indirectObjectSize.isDirty;
}

void StateBaseAddressProperties::clearIsDirty() {
    globalAtomics.isDirty = false;
    statelessMocs.isDirty = false;
    bindingTablePoolBaseAddress.isDirty = false;
    bindingTablePoolSize.isDirty = false;
    surfaceStateBaseAddress.isDirty = false;
    surfaceStateSize.isDirty = false;
    dynamicStateBaseAddress.isDirty = false;
    dynamicStateSize.isDirty = false;
    indirectObjectBaseAddress.isDirty = false;
    indirectObjectSize.isDirty = false;
}
