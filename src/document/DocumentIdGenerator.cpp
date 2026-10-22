// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "DocumentIdGeneration.hpp"
#include <voelund/platform/RandomBytes.hpp>

namespace voelund::document {
DocumentId generateDocumentId() {
    return detail::generateDocumentIdUsing(platform::randomBytes128);
}
} // namespace voelund::document
