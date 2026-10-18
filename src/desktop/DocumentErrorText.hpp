#pragma once

#include <QString>

#include <string>

namespace voelund::desktop {

// L03: why a document could not be opened, in the user's words, with the technical message of
// the reader in parentheses. A file of a newer Völund version says so instead of only
// "unsupported schema".
[[nodiscard]] QString documentOpenErrorText(const std::string& technical);

} // namespace voelund::desktop
