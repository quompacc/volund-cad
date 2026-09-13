#pragma once
namespace voelund::test {
void failAllocationAfter(int successfulAllocations) noexcept;
void disableAllocationFailure() noexcept;
} // namespace voelund::test