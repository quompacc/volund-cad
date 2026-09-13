#pragma once

#include <filesystem>
#include <string_view>
#include <system_error>

namespace voelund::platform {
enum class FileStoreStage { create, write, flush, close, replace };

class FileStoreError final : public std::system_error {
  public:
    FileStoreError(FileStoreStage stage, std::error_code error);
    [[nodiscard]] FileStoreStage stage() const noexcept { return stage_; }

  private:
    FileStoreStage stage_;
};

// Replaces one directory entry using a private sibling file. Throws before commit on error.
// The parent directory must be trusted. No durability promise across power loss or network FS.
class AtomicFileStore final {
  public:
    static void write(const std::filesystem::path& target, std::string_view bytes);
};
} // namespace voelund::platform
