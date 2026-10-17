#pragma once

#include <filesystem>
#include <string_view>

namespace voelund::test {

inline std::filesystem::path temporaryFilePath(std::string_view fileName) {
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "voelund-cad-tests";
    std::filesystem::create_directories(directory);
    return directory / fileName;
}

} // namespace voelund::test
