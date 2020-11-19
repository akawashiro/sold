#include <c10/core/CompileTimeFunctionPointer.h>
#include <gtest/gtest.h>

namespace test_is_compile_time_function_pointer {
static_assert(!c10::is_compile_time_function_pointer<void()>::value, "");

void dummy() {}
static_assert(c10::is_compile_time_function_pointer<TORCH_FN_TYPE(dummy)>::value, "");
}  // namespace test_is_compile_time_function_pointer

namespace test_access_through_type {
void dummy() {}
using dummy_ptr = TORCH_FN_TYPE(dummy);
static_assert(c10::is_compile_time_function_pointer<dummy_ptr>::value, "");
static_assert(dummy_ptr::func_ptr() == &dummy, "");
static_assert(std::is_same<void(), dummy_ptr::FuncType>::value, "");
}  // namespace test_access_through_type

namespace test_access_through_value {
void dummy() {}
constexpr auto dummy_ptr = TORCH_FN(dummy);
static_assert(dummy_ptr.func_ptr() == &dummy, "");
static_assert(std::is_same<void(), decltype(dummy_ptr)::FuncType>::value, "");
}  // namespace test_access_through_value

namespace test_access_through_type_also_works_if_specified_as_pointer {
void dummy() {}
using dummy_ptr = TORCH_FN_TYPE(&dummy);
static_assert(c10::is_compile_time_function_pointer<dummy_ptr>::value, "");
static_assert(dummy_ptr::func_ptr() == &dummy, "");
static_assert(std::is_same<void(), dummy_ptr::FuncType>::value, "");
}  // namespace test_access_through_type_also_works_if_specified_as_pointer

namespace test_access_through_value_also_works_if_specified_as_pointer {
void dummy() {}
constexpr auto dummy_ptr = TORCH_FN(&dummy);
static_assert(dummy_ptr.func_ptr() == &dummy, "");
static_assert(std::is_same<void(), decltype(dummy_ptr)::FuncType>::value, "");
}  // namespace test_access_through_value_also_works_if_specified_as_pointer

namespace test_run_through_type {
int add(int a, int b) {
    return a + b;
}
using Add = TORCH_FN_TYPE(add);
template <class Func>
struct Executor {
    int execute(int a, int b) { return Func::func_ptr()(a, b); }
};

TEST(CompileTimeFunctionPointerTest, runFunctionThroughType) {
    Executor<Add> executor;
    EXPECT_EQ(3, executor.execute(1, 2));
}
}  // namespace test_run_through_type

namespace test_run_through_value {
int add(int a, int b) {
    return a + b;
}
template <class Func>
int execute(Func, int a, int b) {
    return Func::func_ptr()(a, b);
}

TEST(CompileTimeFunctionPointerTest, runFunctionThroughValue) {
    EXPECT_EQ(3, execute(TORCH_FN(add), 1, 2));
}
}  // namespace test_run_through_value

#include <c10/core/DeviceGuard.h>
#include <c10/core/impl/FakeGuardImpl.h>

using namespace c10;
using namespace c10::impl;

// The tests here are mostly covered by InlineDeviceGuard_test, but there
// is some DeviceGuard specific functionality we must test.

// -- DeviceGuard -------------------------------------------------------

TEST(DeviceGuard, ResetDeviceDifferentDeviceType) {
    FakeGuardImpl<DeviceType::CUDA> cuda_impl;
    FakeGuardImpl<DeviceType::HIP> hip_impl;
    FakeGuardImpl<DeviceType::CUDA>::setDeviceIndex(0);
    FakeGuardImpl<DeviceType::HIP>::setDeviceIndex(0);
    DeviceGuard g(Device(DeviceType::CUDA, 1), &cuda_impl);
    g.reset_device(Device(DeviceType::HIP, 2), &hip_impl);
    ASSERT_EQ(FakeGuardImpl<DeviceType::CUDA>::getDeviceIndex(), 0);
    ASSERT_EQ(FakeGuardImpl<DeviceType::HIP>::getDeviceIndex(), 2);
    ASSERT_EQ(g.current_device(), Device(DeviceType::HIP, 2));
    ASSERT_EQ(g.original_device(), Device(DeviceType::HIP, 0));
}

// -- OptionalDeviceGuard -----------------------------------------------

TEST(OptionalDeviceGuard, ResetDeviceDifferentDeviceType) {
    FakeGuardImpl<DeviceType::CUDA> cuda_impl;
    FakeGuardImpl<DeviceType::HIP> hip_impl;
    FakeGuardImpl<DeviceType::CUDA>::setDeviceIndex(0);
    FakeGuardImpl<DeviceType::HIP>::setDeviceIndex(0);
    OptionalDeviceGuard g;
    g.reset_device(Device(DeviceType::CUDA, 1), &cuda_impl);
    g.reset_device(Device(DeviceType::HIP, 2), &hip_impl);
    ASSERT_EQ(FakeGuardImpl<DeviceType::CUDA>::getDeviceIndex(), 0);
    ASSERT_EQ(FakeGuardImpl<DeviceType::HIP>::getDeviceIndex(), 2);
    ASSERT_EQ(g.current_device(), make_optional(Device(DeviceType::HIP, 2)));
    ASSERT_EQ(g.original_device(), make_optional(Device(DeviceType::HIP, 0)));
}

#include <unordered_set>

#include <c10/core/DispatchKeySet.h>

using namespace c10;

TEST(DispatchKeySet, Empty) {
    DispatchKeySet empty_set;
    for (uint8_t i = 1; i < static_cast<uint8_t>(DispatchKey::NumDispatchKeys); i++) {
        auto tid = static_cast<DispatchKey>(i);
        ASSERT_FALSE(empty_set.has(tid));
    }
    ASSERT_TRUE(empty_set.empty());
    DispatchKeySet empty_set2;
    ASSERT_TRUE(empty_set == empty_set2);
    ASSERT_EQ(empty_set.highestPriorityTypeId(), DispatchKey::Undefined);
}

TEST(DispatchKeySet, Singleton) {
    for (uint8_t i = 1; i < static_cast<uint8_t>(DispatchKey::NumDispatchKeys); i++) {
        auto tid = static_cast<DispatchKey>(i);
        DispatchKeySet sing(tid);
        ASSERT_EQ(sing, sing);
        ASSERT_EQ(sing, DispatchKeySet().add(tid));
        ASSERT_EQ(sing, sing.add(tid));
        ASSERT_EQ(sing, sing | sing);
        ASSERT_FALSE(sing.empty());
        ASSERT_TRUE(sing.has(tid));
        ASSERT_EQ(sing.highestPriorityTypeId(), tid);
        ASSERT_EQ(sing.remove(tid), DispatchKeySet());
    }
}

TEST(DispatchKeySet, Doubleton) {
    for (uint8_t i = 1; i < static_cast<uint8_t>(DispatchKey::NumDispatchKeys); i++) {
        for (uint8_t j = i + 1; j < static_cast<uint8_t>(DispatchKey::NumDispatchKeys); j++) {
            ASSERT_LT(i, j);
            auto tid1 = static_cast<DispatchKey>(i);
            auto tid2 = static_cast<DispatchKey>(j);
            auto doub = DispatchKeySet(tid1).add(tid2);
            ASSERT_EQ(doub, DispatchKeySet(tid1) | DispatchKeySet(tid2));
            ASSERT_TRUE(doub.has(tid1));
            ASSERT_TRUE(doub.has(tid2));
            ASSERT_EQ(doub.highestPriorityTypeId(), tid2);  // relies on i < j
        }
    }
}

TEST(DispatchKeySet, Full) {
    DispatchKeySet full(DispatchKeySet::FULL);
    for (uint8_t i = 1; i < static_cast<uint8_t>(DispatchKey::NumDispatchKeys); i++) {
        auto tid = static_cast<DispatchKey>(i);
        ASSERT_TRUE(full.has(tid));
    }
}

TEST(DispatchKeySet, IteratorBasicOps) {
    DispatchKeySet empty_set;
    DispatchKeySet full_set(DispatchKeySet::FULL);
    DispatchKeySet mutated_set = empty_set.add(static_cast<DispatchKey>(1));

    // Constructor + Comparison
    ASSERT_EQ(*empty_set.begin(), DispatchKey::NumDispatchKeys);
    ASSERT_EQ(*empty_set.end(), DispatchKey::NumDispatchKeys);
    ASSERT_EQ(*mutated_set.begin(), static_cast<DispatchKey>(1));

    ASSERT_TRUE(empty_set.begin() == empty_set.end());
    ASSERT_TRUE(full_set.begin() != full_set.end());

    // Increment Ops
    ASSERT_TRUE(full_set.begin() == full_set.begin()++);
    ASSERT_TRUE(full_set.begin() != ++full_set.begin());
}

TEST(DispatchKeySet, IteratorEmpty) {
    DispatchKeySet empty_set;
    uint8_t i = 0;

    for (auto it = empty_set.begin(); it != empty_set.end(); ++it) {
        i++;
    }
    ASSERT_EQ(i, 0);
}

TEST(DispatchKeySet, IteratorFull) {
    DispatchKeySet full_set(DispatchKeySet::FULL);
    uint8_t i = 0;

    for (auto it = full_set.begin(); it != full_set.end(); ++it) {
        i++;
        ASSERT_TRUE(*it == static_cast<DispatchKey>(i));
        ASSERT_TRUE(*it != DispatchKey::NumDispatchKeys);
    }
    ASSERT_EQ(i, static_cast<uint8_t>(DispatchKey::NumDispatchKeys) - 1);
}

TEST(DispatchKeySet, IteratorRangeFull) {
    DispatchKeySet full_set(DispatchKeySet::FULL);
    uint8_t i = 0;

    for (DispatchKey dispatch_key : full_set) {
        i++;
        ASSERT_TRUE(dispatch_key == static_cast<DispatchKey>(i));
    }

    ASSERT_EQ(i, static_cast<uint8_t>(DispatchKey::NumDispatchKeys) - 1);
}

TEST(DispatchKeySet, SpecificKeys) {
    DispatchKeySet keyset({
        static_cast<DispatchKey>(0),  // Undefined should be ignored
        static_cast<DispatchKey>(4),
        static_cast<DispatchKey>(10),
        static_cast<DispatchKey>(15),
    });
    std::unordered_set<DispatchKey> visited_keys;

    for (DispatchKey key : keyset) {
        visited_keys.insert(key);
    }

    ASSERT_EQ(visited_keys.size(), 3);
    ASSERT_TRUE(visited_keys.find(static_cast<DispatchKey>(4)) != visited_keys.end());
    ASSERT_TRUE(visited_keys.find(static_cast<DispatchKey>(10)) != visited_keys.end());
    ASSERT_TRUE(visited_keys.find(static_cast<DispatchKey>(15)) != visited_keys.end());
}

TEST(DispatchKeySet, FailAtEndIterator) {
    DispatchKeySet full_set(DispatchKeySet::FULL);
    uint64_t raw_repr = full_set.raw_repr();

    EXPECT_THROW(DispatchKeySet::iterator(&raw_repr, static_cast<uint8_t>(DispatchKey::NumDispatchKeys) + 1), c10::Error);
}

#include <c10/core/StreamGuard.h>

// At the moment, just make sure it compiles
