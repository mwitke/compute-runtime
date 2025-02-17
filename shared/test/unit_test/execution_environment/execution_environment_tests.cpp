/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/destructor_counted.h"

using namespace NEO;

TEST(ExecutionEnvironment, givenDefaultConstructorWhenItIsCalledThenExecutionEnvironmentHasInitialRefCountZero) {
    ExecutionEnvironment environment;
    EXPECT_EQ(0, environment.getRefInternalCount());
    EXPECT_EQ(0, environment.getRefApiCount());
}

TEST(ExecutionEnvironment, WhenCreatingDevicesThenThoseDevicesAddRefcountsToExecutionEnvironment) {
    auto executionEnvironment = new ExecutionEnvironment();

    auto expectedRefCounts = executionEnvironment->getRefInternalCount();
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    EXPECT_LE(0u, devices[0]->getNumSubDevices());
    if (devices[0]->getNumSubDevices() > 1) {
        expectedRefCounts++;
    }
    expectedRefCounts += std::max(devices[0]->getNumSubDevices(), 1u);
    EXPECT_EQ(expectedRefCounts, executionEnvironment->getRefInternalCount());
}

TEST(ExecutionEnvironment, givenMemoryManagerIsNotInitializedInExecutionEnvironmentWhenCreatingDevicesThenEmptyDeviceVectorIsReturned) {
    class FailedInitializeMemoryManagerExecutionEnvironment : public MockExecutionEnvironment {
        bool initializeMemoryManager() override { return false; }
    };

    auto executionEnvironment = std::make_unique<FailedInitializeMemoryManagerExecutionEnvironment>();
    prepareDeviceEnvironments(*executionEnvironment);
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    EXPECT_TRUE(devices.empty());
}

TEST(ExecutionEnvironment, givenDeviceWhenItIsDestroyedThenMemoryManagerIsStillAvailable) {
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.incRefInternal();
    executionEnvironment.initializeMemoryManager();
    std::unique_ptr<Device> device(Device::create<RootDevice>(&executionEnvironment, 0u));
    device.reset(nullptr);
    EXPECT_NE(nullptr, executionEnvironment.memoryManager);
}

TEST(RootDeviceEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsReceivesCorrectInputParams) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->initAubCenter(true, "test.aub", CommandStreamReceiverType::CSR_AUB);
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
    EXPECT_TRUE(rootDeviceEnvironment->localMemoryEnabledReceived);
    EXPECT_STREQ(rootDeviceEnvironment->aubFileNameReceived.c_str(), "test.aub");
}

TEST(RootDeviceEnvironment, whenCreatingRootDeviceEnvironmentThenCreateOsAgnosticOsTime) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto profilingTimerResolution = defaultHwInfo->capabilityTable.defaultProfilingTimerResolution;

    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());

    EXPECT_EQ(nullptr, rootDeviceEnvironment->osTime.get());
    rootDeviceEnvironment->initOsTime();

    uint64_t ts = 123;
    EXPECT_TRUE(rootDeviceEnvironment->osTime->getCpuTime(&ts));
    EXPECT_EQ(0u, ts);

    EXPECT_EQ(0u, rootDeviceEnvironment->osTime->getHostTimerResolution());
    EXPECT_EQ(0u, rootDeviceEnvironment->osTime->getCpuRawTimestamp());

    TimeStampData tsData{1, 2};
    EXPECT_TRUE(rootDeviceEnvironment->osTime->getCpuGpuTime(&tsData));
    EXPECT_EQ(0u, tsData.CPUTimeinNS);
    EXPECT_EQ(0u, tsData.GPUTimeStamp);

    EXPECT_EQ(profilingTimerResolution, rootDeviceEnvironment->osTime->getDynamicDeviceTimerResolution(*defaultHwInfo));
    EXPECT_EQ(static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution(*defaultHwInfo)), rootDeviceEnvironment->osTime->getDynamicDeviceTimerClock(*defaultHwInfo));
}

TEST(RootDeviceEnvironment, givenUseAubStreamFalseWhenGetAubManagerIsCalledThenReturnNull) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.UseAubStream.set(false);

    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get(), false, 1u};
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initAubCenter(false, "", CommandStreamReceiverType::CSR_AUB);
    auto aubManager = rootDeviceEnvironment->aubCenter->getAubManager();
    EXPECT_EQ(nullptr, aubManager);
}

TEST(RootDeviceEnvironment, givenExecutionEnvironmentWhenInitializeAubCenterIsCalledThenItIsInitalizedOnce) {
    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get(), false, 1u};
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initAubCenter(false, "", CommandStreamReceiverType::CSR_AUB);
    auto currentAubCenter = rootDeviceEnvironment->aubCenter.get();
    EXPECT_NE(nullptr, currentAubCenter);
    auto currentAubStreamProvider = currentAubCenter->getStreamProvider();
    EXPECT_NE(nullptr, currentAubStreamProvider);
    auto currentAubFileStream = currentAubStreamProvider->getStream();
    EXPECT_NE(nullptr, currentAubFileStream);
    rootDeviceEnvironment->initAubCenter(false, "", CommandStreamReceiverType::CSR_AUB);
    EXPECT_EQ(currentAubCenter, rootDeviceEnvironment->aubCenter.get());
    EXPECT_EQ(currentAubStreamProvider, rootDeviceEnvironment->aubCenter->getStreamProvider());
    EXPECT_EQ(currentAubFileStream, rootDeviceEnvironment->aubCenter->getStreamProvider()->getStream());
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenLocalMemorySupportedInMemoryManagerHasCorrectValue) {
    const HardwareInfo *hwInfo = defaultHwInfo.get();
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(hwInfo));
    auto executionEnvironment = device->getExecutionEnvironment();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto enableLocalMemory = gfxCoreHelper.getEnableLocalMemory(*hwInfo);
    executionEnvironment->initializeMemoryManager();
    EXPECT_EQ(enableLocalMemory, executionEnvironment->memoryManager->isLocalMemorySupported(device->getRootDeviceIndex()));
}

TEST(ExecutionEnvironment, givenEnableDirectSubmissionControllerSetWhenInitializeDirectSubmissionControllerThenNotNull) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDirectSubmissionController.set(1);

    MockExecutionEnvironment executionEnvironment{};
    auto controller = executionEnvironment.initializeDirectSubmissionController();

    EXPECT_NE(controller, nullptr);
}

TEST(ExecutionEnvironment, givenSetCsrFlagSetWhenInitializeDirectSubmissionControllerThenNull) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SetCommandStreamReceiver.set(1);

    MockExecutionEnvironment executionEnvironment{};
    auto controller = executionEnvironment.initializeDirectSubmissionController();

    EXPECT_EQ(controller, nullptr);
}

TEST(ExecutionEnvironment, givenEnableDirectSubmissionControllerSetZeroWhenInitializeDirectSubmissionControllerThenNull) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableDirectSubmissionController.set(0);

    MockExecutionEnvironment executionEnvironment{};
    auto controller = executionEnvironment.initializeDirectSubmissionController();

    EXPECT_EQ(controller, nullptr);
}

TEST(ExecutionEnvironment, givenNeoCalEnabledWhenCreateExecutionEnvironmentThenSetDebugVariables) {
    const std::unordered_map<std::string, int32_t> config = {
        {"UseDrmVirtualEnginesForBcs", 0},
        {"EnableCmdQRoundRobindBcsEngineAssignLimit", 6},
        {"EnableCmdQRoundRobindBcsEngineAssign", 1},
        {"ForceBCSForInternalCopyEngine", 7},
        {"AssignBCSAtEnqueue", 0},
        {"EnableCopyEngineSelector", 1},
        {"SplitBcsCopy", 0},
    };

#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    EXPECT_EQ(defaultValue, DebugManager.flags.variableName.getRef());

#include "shared/source/debug_settings/release_variables.inl"

#include "debug_variables.inl"

#undef DECLARE_DEBUG_VARIABLE

    DebugManagerStateRestore restorer;
    DebugManager.flags.NEO_CAL_ENABLED.set(1);
    ExecutionEnvironment exeEnv;

#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)      \
    {                                                                                  \
        if constexpr (std::is_same_v<bool, dataType>) {                                \
            if (strcmp(#variableName, "NEO_CAL_ENABLED") == 0) {                       \
                EXPECT_TRUE(DebugManager.flags.variableName.getRef());                 \
            } else {                                                                   \
                EXPECT_EQ(defaultValue, DebugManager.flags.variableName.getRef());     \
            }                                                                          \
        } else {                                                                       \
            if constexpr (std::is_same_v<int32_t, dataType>) {                         \
                auto it = config.find(#variableName);                                  \
                if (it != config.end()) {                                              \
                    EXPECT_EQ(it->second, DebugManager.flags.variableName.getRef());   \
                } else {                                                               \
                    EXPECT_EQ(defaultValue, DebugManager.flags.variableName.getRef()); \
                }                                                                      \
            } else {                                                                   \
                EXPECT_EQ(defaultValue, DebugManager.flags.variableName.getRef());     \
            }                                                                          \
        }                                                                              \
    }

#include "shared/source/debug_settings/release_variables.inl"

#include "debug_variables.inl"

#undef DECLARE_DEBUG_VARIABLE
}

TEST(ExecutionEnvironment, givenEnvVarUsedInCalConfigAlsoSetByAppWhenCreateExecutionEnvironmentThenRespectAppSetting) {
    constexpr int32_t appCommandBufferAlignment = 12345;
    DebugManagerStateRestore restorer;
    DebugManager.flags.NEO_CAL_ENABLED.set(1);
    DebugManager.flags.ForceCommandBufferAlignment.set(appCommandBufferAlignment);
    ExecutionEnvironment exeEnv;

    EXPECT_EQ(DebugManager.flags.ForceCommandBufferAlignment.get(), appCommandBufferAlignment);
}

TEST(ExecutionEnvironment, givenExecutionEnvironmentWhenInitializeMemoryManagerIsCalledThenItIsInitalized) {
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment.memoryManager);
}
static_assert(sizeof(ExecutionEnvironment) == sizeof(std::unique_ptr<HardwareInfo>) +
                                                  sizeof(std::vector<RootDeviceEnvironment>) +
                                                  sizeof(std::unique_ptr<OsEnvironment>) +
                                                  sizeof(std::unique_ptr<DirectSubmissionController>) +
                                                  sizeof(std::unordered_map<uint32_t, uint32_t>) +
                                                  sizeof(bool) +
                                                  (is64bit ? 23 : 15),
              "New members detected in ExecutionEnvironment, please ensure that destruction sequence of objects is correct");

TEST(ExecutionEnvironment, givenExecutionEnvironmentWithVariousMembersWhenItIsDestroyedThenDeleteSequenceIsSpecified) {
    uint32_t destructorId = 0u;

    struct MemoryMangerMock : public DestructorCounted<MockMemoryManager, 8> {
        MemoryMangerMock(uint32_t &destructorId, ExecutionEnvironment &executionEnvironment) : DestructorCounted(destructorId, executionEnvironment) {
            callBaseAllocateGraphicsMemoryForNonSvmHostPtr = false;
            callBasePopulateOsHandles = false;
        }
    };
    struct DirectSubmissionControllerMock : public DestructorCounted<DirectSubmissionController, 7> {
        DirectSubmissionControllerMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct GmmHelperMock : public DestructorCounted<GmmHelper, 6> {
        GmmHelperMock(uint32_t &destructorId, const RootDeviceEnvironment &rootDeviceEnvironment) : DestructorCounted(destructorId, rootDeviceEnvironment) {}
    };
    struct OsInterfaceMock : public DestructorCounted<OSInterface, 5> {
        OsInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct MemoryOperationsHandlerMock : public DestructorCounted<MockMemoryOperationsHandler, 4> {
        MemoryOperationsHandlerMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct AubCenterMock : public DestructorCounted<AubCenter, 3> {
        AubCenterMock(uint32_t &destructorId, const RootDeviceEnvironment &rootDeviceEnvironment) : DestructorCounted(destructorId, rootDeviceEnvironment, false, "", CommandStreamReceiverType::CSR_AUB) {}
    };
    struct CompilerInterfaceMock : public DestructorCounted<CompilerInterface, 2> {
        CompilerInterfaceMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct BuiltinsMock : public DestructorCounted<BuiltIns, 1> {
        BuiltinsMock(uint32_t &destructorId) : DestructorCounted(destructorId) {}
    };
    struct SourceLevelDebuggerMock : public DestructorCounted<SourceLevelDebugger, 0> {
        SourceLevelDebuggerMock(uint32_t &destructorId) : DestructorCounted(destructorId, nullptr) {}
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->gmmHelper = std::make_unique<GmmHelperMock>(destructorId, *executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OsInterfaceMock>(destructorId);
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<MemoryOperationsHandlerMock>(destructorId);
    executionEnvironment->memoryManager = std::make_unique<MemoryMangerMock>(destructorId, *executionEnvironment);
    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::make_unique<AubCenterMock>(destructorId, *executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->builtins = std::make_unique<BuiltinsMock>(destructorId);
    executionEnvironment->rootDeviceEnvironments[0]->compilerInterface = std::make_unique<CompilerInterfaceMock>(destructorId);
    executionEnvironment->rootDeviceEnvironments[0]->debugger = std::make_unique<SourceLevelDebuggerMock>(destructorId);
    executionEnvironment->directSubmissionController = std::make_unique<DirectSubmissionControllerMock>(destructorId);

    executionEnvironment.reset(nullptr);
    EXPECT_EQ(9u, destructorId);
}

TEST(ExecutionEnvironment, givenMultipleRootDevicesWhenTheyAreCreatedThenReuseMemoryManager) {
    ExecutionEnvironment executionEnvironment{};
    executionEnvironment.incRefInternal();
    executionEnvironment.prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }
    std::unique_ptr<MockDevice> device(Device::create<MockDevice>(&executionEnvironment, 0u));
    auto &commandStreamReceiver = device->getGpgpuCommandStreamReceiver();
    auto memoryManager = device->getMemoryManager();

    std::unique_ptr<MockDevice> device2(Device::create<MockDevice>(&executionEnvironment, 1u));
    EXPECT_NE(&commandStreamReceiver, &device2->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(memoryManager, device2->getMemoryManager());
}

uint64_t isDriverAvailableCounter = 0u;

class DriverModelMock : public DriverModel {
  public:
    DriverModelMock(DriverModelType driverModelType) : DriverModel(driverModelType) {
    }

    bool isDriverAvailable() override {
        isDriverAvailableCounter++;
        return true;
    }
    void setGmmInputArgs(void *args) override {
    }

    uint32_t getDeviceHandle() const override {
        return 0;
    }

    PhysicalDevicePciBusInfo getPciBusInfo() const override {
        return {};
    }
    PhysicalDevicePciSpeedInfo getPciSpeedInfo() const override {
        return {};
    }

    bool skipResourceCleanup() const {
        return skipResourceCleanupVar;
    }

    bool isGpuHangDetected(OsContext &osContext) override {
        return false;
    }
};

class DefaultDriverModelMock : public DriverModel {
  public:
    DefaultDriverModelMock(DriverModelType driverModelType) : DriverModel(driverModelType) {
    }

    bool isDriverAvailable() override {
        return true;
    }
    void setGmmInputArgs(void *args) override {
    }

    uint32_t getDeviceHandle() const override {
        return 0;
    }

    PhysicalDevicePciBusInfo getPciBusInfo() const override {
        return {};
    }
    PhysicalDevicePciSpeedInfo getPciSpeedInfo() const override {
        return {};
    }

    bool skipResourceCleanup() const {
        return skipResourceCleanupVar;
    }

    bool isGpuHangDetected(OsContext &osContext) override {
        return false;
    }
};

TEST(ExecutionEnvironment, givenRootDeviceWhenPrepareForCleanupThenIsDriverAvailableIsCalled) {
    VariableBackup<uint64_t> varBackup = &isDriverAvailableCounter;
    ExecutionEnvironment executionEnvironment{};

    std::unique_ptr<OSInterface> osInterface = std::make_unique<OSInterface>();
    osInterface->setDriverModel(std::make_unique<DriverModelMock>(DriverModelType::UNKNOWN));

    executionEnvironment.prepareRootDeviceEnvironments(1);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::move(osInterface);

    executionEnvironment.prepareForCleanup();

    EXPECT_EQ(1u, isDriverAvailableCounter);

    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::make_unique<DefaultDriverModelMock>(DriverModelType::UNKNOWN));
}

TEST(ExecutionEnvironment, givenUnproperSetCsrFlagValueWhenInitializingMemoryManagerThenCreateDefaultMemoryManager) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.SetCommandStreamReceiver.set(10);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get());
    executionEnvironment->initializeMemoryManager();
    EXPECT_NE(nullptr, executionEnvironment->memoryManager);
}

TEST(ExecutionEnvironment, whenCalculateMaxOsContexCountThenGlobalVariableHasProperValue) {
    DebugManagerStateRestore restore;
    VariableBackup<uint32_t> osContextCountBackup(&MemoryManager::maxOsContextCount, 0);
    uint32_t numRootDevices = 17u;
    uint32_t expectedOsContextCount = 0u;
    uint32_t expectedOsContextCountForCcs = 0u;

    {
        DebugManager.flags.EngineInstancedSubDevices.set(false);
        MockExecutionEnvironment executionEnvironment(nullptr, true, numRootDevices);

        for (const auto &rootDeviceEnvironment : executionEnvironment.rootDeviceEnvironments) {
            auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
            auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
            auto osContextCount = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment).size();
            auto subDevicesCount = GfxCoreHelper::getSubDevicesCount(hwInfo);
            bool hasRootCsr = subDevicesCount > 1;
            auto ccsCount = hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

            expectedOsContextCount += static_cast<uint32_t>(osContextCount * subDevicesCount + hasRootCsr);

            if (ccsCount > 1) {
                expectedOsContextCountForCcs += ccsCount * subDevicesCount;
            }
        }

        EXPECT_EQ(expectedOsContextCount, MemoryManager::maxOsContextCount);
    }

    {
        DebugManager.flags.EngineInstancedSubDevices.set(true);
        MockExecutionEnvironment executionEnvironment(nullptr, true, numRootDevices);

        EXPECT_EQ(expectedOsContextCount + expectedOsContextCountForCcs, MemoryManager::maxOsContextCount);
    }
}
