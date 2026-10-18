#pragma once

#include <optional>

namespace voelund::apps {

// L03: command-line checks that need no window, handled before the shell starts.
//   --version                                    prints "Völund CAD <version> (<revision>)"
//   --check-document <file> [--resave <target>]  opens a document with the product's reader,
//       optionally saves it as <target> and opens that again
// Returns the exit code when one of them was given: 0 success, 2 the document was refused
// (with the reason in the user's words on stderr), 3 saving or reopening failed, 64 usage.
[[nodiscard]] std::optional<int> runCommandLine(int argc, char* argv[]);

} // namespace voelund::apps
