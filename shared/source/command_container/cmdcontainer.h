/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace NEO {
class AllocationsList;
class CommandStreamReceiver;
class Device;
class GraphicsAllocation;
class HeapHelper;
class IndirectHeap;
class LinearStream;
class ReservedIndirectHeap;

using ResidencyContainer = std::vector<GraphicsAllocation *>;
using CmdBufferContainer = std::vector<GraphicsAllocation *>;
using HeapContainer = std::vector<GraphicsAllocation *>;
using HeapType = IndirectHeapType;

namespace CSRequirements {
// cleanup section usually contains 1-2 pipeControls BB end and place for BB start
// that makes 16 * 2 + 4 + 8 = 40 bytes
// then command buffer is aligned to cacheline that can take up to 63 bytes
// to be sure everything fits minimal size is at 2 x cacheline.

inline constexpr auto minCommandQueueCommandStreamSize = 2 * MemoryConstants::cacheLineSize;
inline constexpr auto csOverfetchSize = MemoryConstants::pageSize;
} // namespace CSRequirements

struct HeapReserveData {
    HeapReserveData();
    virtual ~HeapReserveData();
    ReservedIndirectHeap *indirectHeapReservation = nullptr;

  protected:
    std::unique_ptr<ReservedIndirectHeap> object;
};

struct HeapReserveArguments {
    ReservedIndirectHeap *indirectHeapReservation = nullptr;
    size_t size = 0;
    size_t alignment = 0;
};

class CommandContainer : public NonCopyableOrMovableClass {
  public:
    enum class ErrorCode {
        SUCCESS = 0,
        OUT_OF_DEVICE_MEMORY = 1
    };

    static constexpr size_t defaultListCmdBufferSize = 1u * MemoryConstants ::megaByte;
    static constexpr size_t cmdBufferReservedSize = MemoryConstants::cacheLineSize +
                                                    CSRequirements::csOverfetchSize;
    static constexpr size_t totalCmdBufferSize = defaultListCmdBufferSize + cmdBufferReservedSize;
    static constexpr size_t startingResidencyContainerSize = 128;

    CommandContainer();

    CommandContainer(uint32_t maxNumAggregatedIdds);

    CmdBufferContainer &getCmdBufferAllocations() { return cmdBufferAllocations; }

    ResidencyContainer &getResidencyContainer() { return residencyContainer; }

    std::vector<GraphicsAllocation *> &getDeallocationContainer() { return deallocationContainer; }

    void addToResidencyContainer(GraphicsAllocation *alloc);
    void removeDuplicatesFromResidencyContainer();

    LinearStream *getCommandStream() { return commandStream.get(); }

    IndirectHeap *getIndirectHeap(HeapType heapType);

    HeapHelper *getHeapHelper() { return heapHelper.get(); }

    GraphicsAllocation *getIndirectHeapAllocation(HeapType heapType) { return allocationIndirectHeaps[heapType]; }

    void setIndirectHeapAllocation(HeapType heapType, GraphicsAllocation *allocation) { allocationIndirectHeaps[heapType] = allocation; }

    uint64_t getInstructionHeapBaseAddress() const { return instructionHeapBaseAddress; }

    uint64_t getIndirectObjectHeapBaseAddress() const { return indirectObjectHeapBaseAddress; }

    void *getHeapSpaceAllowGrow(HeapType heapType, size_t size);

    ErrorCode initialize(Device *device, AllocationsList *reusableAllocationList, bool requireHeaps);

    void prepareBindfulSsh();

    virtual ~CommandContainer();

    Device *getDevice() const { return device; }

    MOCKABLE_VIRTUAL IndirectHeap *getHeapWithRequiredSizeAndAlignment(HeapType heapType, size_t sizeRequired, size_t alignment);
    void allocateNextCommandBuffer();
    void closeAndAllocateNextCommandBuffer();

    void handleCmdBufferAllocations(size_t startIndex);
    GraphicsAllocation *obtainNextCommandBufferAllocation();

    void reset();

    bool isHeapDirty(HeapType heapType) const { return (dirtyHeaps & (1u << heapType)); }
    bool isAnyHeapDirty() const { return dirtyHeaps != 0; }
    void setHeapDirty(HeapType heapType) { dirtyHeaps |= (1u << heapType); }
    void setDirtyStateForAllHeaps(bool dirty) { dirtyHeaps = dirty ? std::numeric_limits<uint32_t>::max() : 0; }
    void setIddBlock(void *iddBlock) { this->iddBlock = iddBlock; }
    void *getIddBlock() { return iddBlock; }
    uint32_t getNumIddPerBlock() const { return numIddsPerBlock; }
    void setNumIddPerBlock(uint32_t value) { numIddsPerBlock = value; }
    void setReservedSshSize(size_t reserveSize) {
        reservedSshSize = reserveSize;
    }

    bool getFlushTaskUsedForImmediate() const { return isFlushTaskUsedForImmediate; }
    void setFlushTaskUsedForImmediate(bool flushTaskUsedForImmediate) { isFlushTaskUsedForImmediate = flushTaskUsedForImmediate; }
    void setImmediateCmdListCsr(CommandStreamReceiver *newValue) {
        this->immediateCmdListCsr = newValue;
    }
    void enableHeapSharing() { heapSharingEnabled = true; }
    bool immediateCmdListSharedHeap(HeapType heapType) {
        return (heapSharingEnabled && (heapType == HeapType::DYNAMIC_STATE || heapType == HeapType::SURFACE_STATE));
    }

    void reserveSpaceForDispatch(HeapReserveArguments &sshReserveArg, HeapReserveArguments &dshReserveArg, bool getDsh);

    GraphicsAllocation *reuseExistingCmdBuffer();
    GraphicsAllocation *allocateCommandBuffer();
    void setCmdBuffer(GraphicsAllocation *cmdBuffer);
    void addCurrentCommandBufferToReusableAllocationList();

    void fillReusableAllocationLists();
    void storeAllocationAndFlushTagUpdate(GraphicsAllocation *allocation);

    HeapReserveData &getSurfaceStateHeapReserve() {
        return surfaceStateHeapReserveData;
    }
    HeapReserveData &getDynamicStateHeapReserve() {
        return dynamicStateHeapReserveData;
    }

    HeapContainer sshAllocations;
    uint64_t currentLinearStreamStartOffset = 0u;
    uint32_t slmSize = std::numeric_limits<uint32_t>::max();
    uint32_t nextIddInBlock = 0;
    bool lastPipelineSelectModeRequired = false;
    bool lastSentUseGlobalAtomics = false;
    bool systolicModeSupport = false;

  protected:
    size_t getTotalCmdBufferSize();
    IndirectHeap *getHeapWithRequiredSize(HeapType heapType, size_t sizeRequired, size_t alignment, bool allowGrow);
    void createAndAssignNewHeap(HeapType heapType, size_t size);
    IndirectHeap *initIndirectHeapReservation(ReservedIndirectHeap *indirectHeapReservation, size_t size, size_t alignment, HeapType heapType);
    GraphicsAllocation *allocationIndirectHeaps[HeapType::NUM_TYPES] = {};
    std::unique_ptr<IndirectHeap> indirectHeaps[HeapType::NUM_TYPES];
    HeapReserveData dynamicStateHeapReserveData;
    HeapReserveData surfaceStateHeapReserveData;

    CmdBufferContainer cmdBufferAllocations;
    ResidencyContainer residencyContainer;
    std::vector<GraphicsAllocation *> deallocationContainer;

    std::unique_ptr<HeapHelper> heapHelper;
    std::unique_ptr<LinearStream> commandStream;

    uint64_t instructionHeapBaseAddress = 0u;
    uint64_t indirectObjectHeapBaseAddress = 0u;

    void *iddBlock = nullptr;
    Device *device = nullptr;
    AllocationsList *reusableAllocationList = nullptr;
    std::unique_ptr<AllocationsList> immediateReusableAllocationList;
    size_t reservedSshSize = 0;
    CommandStreamReceiver *immediateCmdListCsr = nullptr;
    IndirectHeap *sharedSshCsrHeap = nullptr;
    IndirectHeap *sharedDshCsrHeap = nullptr;

    uint32_t dirtyHeaps = std::numeric_limits<uint32_t>::max();
    uint32_t numIddsPerBlock = 64;

    bool isFlushTaskUsedForImmediate = false;
    bool isHandleFenceCompletionRequired = false;
    bool heapSharingEnabled = false;
};

} // namespace NEO
