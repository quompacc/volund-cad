#include "AllocationFailure.hpp"
#include <cstdlib>
#include <new>

namespace {
thread_local int remaining = -1;
void* allocate(std::size_t size) {
    if (remaining == 0) {
        remaining = -1;
        throw std::bad_alloc();
    }
    if (remaining > 0)
        --remaining;
    if (void* pointer = std::malloc(size == 0 ? 1 : size))
        return pointer;
    throw std::bad_alloc();
}
} // namespace
namespace voelund::test {
void failAllocationAfter(int successfulAllocations) noexcept { remaining = successfulAllocations; }
void disableAllocationFailure() noexcept { remaining = -1; }
} // namespace voelund::test
// Linked only into dedicated allocation-test executables, never the CAD applications.
void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }