// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "AtomicFileStoreDetail.hpp"
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <mutex>
#include <windows.h>
#else
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace voelund::platform::detail {
namespace {
[[noreturn]] void fail(FileStoreStage stage) {
#ifdef _WIN32
    throw FileStoreError(stage, {static_cast<int>(GetLastError()), std::system_category()});
#else
    throw FileStoreError(stage, {errno, std::generic_category()});
#endif
}
} // namespace

NativeTemp::NativeTemp(std::filesystem::path path) : path_(std::move(path)) {
#ifdef _WIN32
    handle_ = CreateFileW(path_.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE)
        fail(FileStoreStage::create);
#else
    handle_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (handle_ == -1)
        fail(FileStoreStage::create);
#endif
}

NativeTemp::~NativeTemp() {
#ifdef _WIN32
    if (handle_ != nullptr)
        CloseHandle(handle_);
    if (!committed_)
        DeleteFileW(path_.c_str());
#else
    if (handle_ != -1)
        ::close(handle_);
    if (!committed_)
        ::unlink(path_.c_str());
#endif
}

void NativeTemp::write(std::string_view bytes) {
    while (!bytes.empty()) {
#ifdef _WIN32
        DWORD written = 0;
        if (!WriteFile(handle_, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr))
            fail(FileStoreStage::write);
#else
        const auto written = ::write(handle_, bytes.data(), bytes.size());
        if (written < 0) {
            if (errno == EINTR)
                continue;
            fail(FileStoreStage::write);
        }
#endif
        if (written == 0)
            throw FileStoreError(FileStoreStage::write, std::make_error_code(std::errc::io_error));
        bytes.remove_prefix(static_cast<std::size_t>(written));
    }
}

void NativeTemp::flush() {
#ifdef _WIN32
    if (!FlushFileBuffers(handle_))
        fail(FileStoreStage::flush);
#else
    while (::fsync(handle_) != 0) {
        if (errno != EINTR)
            fail(FileStoreStage::flush);
    }
#endif
}

void NativeTemp::close() {
#ifdef _WIN32
    if (!CloseHandle(handle_))
        fail(FileStoreStage::close);
    handle_ = nullptr;
#else
    // Do not retry close: the descriptor may already have been released and reused.
    const int descriptor = std::exchange(handle_, -1);
    if (::close(descriptor) != 0)
        fail(FileStoreStage::close);
#endif
}

void NativeTemp::replace(const std::filesystem::path& target) {
#ifdef _WIN32
    // Concurrent MoveFileEx calls may deny each other's delete access. Serialize only
    // the commit syscall; independent serialization and file writes remain concurrent.
    static std::mutex commitMutex;
    const std::lock_guard lock(commitMutex);
    if (!MoveFileExW(path_.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        fail(FileStoreStage::replace);
#else
    if (::rename(path_.c_str(), target.c_str()) != 0)
        fail(FileStoreStage::replace);
#endif
    committed_ = true;
}
} // namespace voelund::platform::detail
