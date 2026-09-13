#pragma once
#include <functional>
#include <voelund/platform/AtomicFileStore.hpp>

namespace voelund::platform::detail {
// Internal fault boundary; production always supplies an empty hook.
using StageHook = std::function<void(FileStoreStage)>;
void writeAtomic(const std::filesystem::path& target, std::string_view bytes,
                 const StageHook& beforeStage);

class NativeTemp final {
  public:
    explicit NativeTemp(std::filesystem::path path);
    ~NativeTemp();
    NativeTemp(const NativeTemp&) = delete;
    NativeTemp& operator=(const NativeTemp&) = delete;
    void write(std::string_view bytes);
    void flush();
    void close();
    void replace(const std::filesystem::path& target);

  private:
    std::filesystem::path path_;
#ifdef _WIN32
    void* handle_{nullptr};
#else
    int handle_{-1};
#endif
    bool committed_{false};
};
} // namespace voelund::platform::detail
