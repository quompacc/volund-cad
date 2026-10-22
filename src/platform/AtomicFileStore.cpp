// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "AtomicFileStoreDetail.hpp"
#include <algorithm>
#include <atomic>
#include <memory>
#include <random>
#include <string>

namespace voelund::platform {
namespace {
const char* stageName(FileStoreStage stage) {
    switch (stage) {
    case FileStoreStage::create:
        return "atomic file create";
    case FileStoreStage::write:
        return "atomic file write";
    case FileStoreStage::flush:
        return "atomic file flush";
    case FileStoreStage::close:
        return "atomic file close";
    case FileStoreStage::replace:
        return "atomic file replace";
    }
    return "atomic file operation";
}

std::filesystem::path sibling(const std::filesystem::path& target) {
    static std::atomic<unsigned long long> sequence{0};
    std::random_device random;
    const auto name = ".voelund-save-" + std::to_string(random()) + "-" + std::to_string(random()) +
                      "-" + std::to_string(sequence++) + ".tmp";
    return target.parent_path() / name;
}

std::unique_ptr<detail::NativeTemp> createTemp(const std::filesystem::path& target) {
    for (int attempt = 0; attempt < 128; ++attempt) {
        try {
            return std::make_unique<detail::NativeTemp>(sibling(target));
        } catch (const FileStoreError& error) {
            if (error.code() != std::errc::file_exists)
                throw;
        }
    }
    throw FileStoreError(FileStoreStage::create, std::make_error_code(std::errc::file_exists));
}
} // namespace

FileStoreError::FileStoreError(FileStoreStage stage, std::error_code error)
    : std::system_error(error, stageName(stage)), stage_(stage) {}

void AtomicFileStore::write(const std::filesystem::path& target, std::string_view bytes) {
    detail::writeAtomic(target, bytes, {});
}

void detail::writeAtomic(const std::filesystem::path& target, std::string_view bytes,
                         const StageHook& beforeStage) {
    const auto boundary = [&](FileStoreStage stage) {
        if (beforeStage)
            beforeStage(stage);
    };
    boundary(FileStoreStage::create);
    if (target.empty() || target.filename().empty() ||
        target.native().find(typename std::filesystem::path::value_type{}) !=
            std::filesystem::path::string_type::npos)
        throw FileStoreError(FileStoreStage::create,
                             std::make_error_code(std::errc::invalid_argument));
    // Resolve relative paths once, so subsequent operations share one parent.
    const auto absolute = std::filesystem::absolute(target);
    auto temp = createTemp(absolute);
    while (!bytes.empty()) {
        boundary(FileStoreStage::write);
        const auto chunk = bytes.substr(0, std::min<std::size_t>(bytes.size(), 65536));
        temp->write(chunk);
        bytes.remove_prefix(chunk.size());
    }
    boundary(FileStoreStage::flush);
    temp->flush();
    boundary(FileStoreStage::close);
    temp->close();
    boundary(FileStoreStage::replace);
    temp->replace(absolute);
}
} // namespace voelund::platform
