// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
namespace voelund::test {
void failAllocationAfter(int successfulAllocations) noexcept;
void disableAllocationFailure() noexcept;
} // namespace voelund::test