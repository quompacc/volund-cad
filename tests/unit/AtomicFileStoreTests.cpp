#include "../../src/platform/AtomicFileStoreDetail.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;
using namespace voelund::platform;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
struct Fixture {
    fs::path root;
    explicit Fixture(const fs::path& parent) {
        for (int attempt = 0; attempt < 128; ++attempt) {
            const auto candidate = parent / ("atomic-" + std::to_string(std::random_device{}()));
            if (fs::create_directory(candidate)) {
                root = candidate;
                return;
            }
        }
        throw std::runtime_error("Cannot create isolated fixture");
    }
    ~Fixture() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
};
std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "Cannot read fixture");
    return {std::istreambuf_iterator<char>(input), {}};
}
void noTemps(const fs::path& root) {
    for (const auto& entry : fs::directory_iterator(root))
        require(!entry.path().filename().u8string().starts_with(u8".voelund-save-"),
                "Owned temporary file leaked");
}
void roundtrip(const fs::path& root) {
    const auto directory = root / fs::path(u8"Ordner-ä-東京");
    fs::create_directory(directory);
    const auto target = directory / fs::path(u8"Modell-ä-東京.volund");
    const std::string binary("new\0bytes\r\n", 11);
    AtomicFileStore::write(target, "old");
    AtomicFileStore::write(target, binary);
    require(read(target) == binary, "Binary/Unicode replacement changed bytes");
    AtomicFileStore::write(target, {});
    require(read(target).empty(), "Empty replacement failed");
    noTemps(directory);
    noTemps(root);
}
void failures(const fs::path& root) {
    const auto target = root / "failure.volund";
    AtomicFileStore::write(target, "original");
    const std::string payload(150000, 'x');
    for (const auto stage : {FileStoreStage::create, FileStoreStage::write, FileStoreStage::flush,
                             FileStoreStage::close, FileStoreStage::replace}) {
        int writes = 0;
        bool failed = false;
        try {
            detail::writeAtomic(target, payload, [&](FileStoreStage current) {
                if (current == FileStoreStage::write && ++writes == 1)
                    return; // Fail after an actual partial write, not before all I/O.
                if (current == stage)
                    throw FileStoreError(stage, std::make_error_code(std::errc::io_error));
            });
        } catch (const FileStoreError& error) {
            failed = error.stage() == stage;
        }
        require(failed && read(target) == "original", "Failure damaged original or wrong stage");
        noTemps(root);
    }
    const auto collision = root / ".voelund-save-foreign.tmp";
    AtomicFileStore::write(collision, "foreign");
    bool rejected = false;
    try {
        detail::NativeTemp temp(collision);
    } catch (const FileStoreError&) {
        rejected = true;
    }
    require(rejected && read(collision) == "foreign", "Exclusive creation overwrote foreign temp");
    AtomicFileStore::write(target, "replacement");
    require(read(collision) == "foreign", "Cleanup removed foreign file");
    fs::remove(collision);
    const auto directory = root / "directory.volund";
    fs::create_directory(directory);
    AtomicFileStore::write(directory / "child", "keep");
    rejected = false;
    try {
        AtomicFileStore::write(directory, "invalid");
    } catch (const FileStoreError& error) {
        rejected = error.stage() == FileStoreStage::replace;
    }
    require(rejected && read(directory / "child") == "keep", "Directory replaced or damaged");
    noTemps(root);
}
void nativeRestrictions(const fs::path& root) {
    const auto target = root / "locked.volund";
    AtomicFileStore::write(target, "locked original");
    bool rejected = false;
#ifdef _WIN32
    const HANDLE lock = CreateFileW(target.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(lock != INVALID_HANDLE_VALUE, "Cannot acquire Windows sharing lock");
    try {
        AtomicFileStore::write(target, "forbidden");
    } catch (const FileStoreError& error) {
        rejected = error.stage() == FileStoreStage::replace;
    }
    CloseHandle(lock);
    require(rejected && read(target) == "locked original", "Sharing lock not respected");
    require(SetFileAttributesW(target.c_str(), FILE_ATTRIBUTE_READONLY), "Cannot set read-only");
    rejected = false;
    try {
        AtomicFileStore::write(target, "forbidden");
    } catch (const FileStoreError&) {
        rejected = true;
    }
    SetFileAttributesW(target.c_str(), FILE_ATTRIBUTE_NORMAL);
    require(rejected && read(target) == "locked original", "Read-only target not respected");
#else
    // POSIX readers retain the original inode; an open reader is not a mandatory rename lock.
    std::ifstream reader(target);
    AtomicFileStore::write(target, "new inode");
    const std::string oldBytes{std::istreambuf_iterator<char>(reader), {}};
    require(oldBytes == "locked original" && read(target) == "new inode", "POSIX inode contract");
    require(geteuid() != 0, "Permission regression must run as unprivileged user");
    require(::chmod(root.c_str(), 0500) == 0, "Cannot remove directory write permission");
    try {
        AtomicFileStore::write(target, "forbidden");
    } catch (const FileStoreError& error) {
        rejected = error.stage() == FileStoreStage::create;
    }
    const int restored = ::chmod(root.c_str(), 0700);
    require(restored == 0 && rejected && read(target) == "new inode",
            "Directory permission ignored");
#endif
    noTemps(root);
}
void concurrent(const fs::path& root) {
    const auto target = root / "concurrent.volund";
    std::vector<std::string> payloads;
    for (int i = 0; i < 8; ++i)
        payloads.emplace_back(200000, static_cast<char>('A' + i));
    std::vector<std::exception_ptr> errors(8);
    std::vector<std::thread> writers;
    for (std::size_t i = 0; i < payloads.size(); ++i)
        writers.emplace_back([&, i] {
            try {
                AtomicFileStore::write(target, payloads[i]);
            } catch (...) {
                errors[i] = std::current_exception();
            }
        });
    for (auto& writer : writers)
        writer.join();
    for (const auto& error : errors)
        if (error)
            std::rethrow_exception(error);
    const auto actual = read(target);
    bool whole = false;
    for (const auto& payload : payloads)
        whole = whole || actual == payload;
    require(whole, "Concurrent saves produced mixed or truncated payload");
    noTemps(root);
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Pass an existing fixture parent directory");
        Fixture fixture(fs::absolute(argv[1]));
        std::cout << "CASE roundtrip" << std::endl;
        roundtrip(fixture.root);
        std::cout << "CASE failures" << std::endl;
        failures(fixture.root);
        std::cout << "CASE nativeRestrictions" << std::endl;
        nativeRestrictions(fixture.root);
        std::cout << "CASE concurrent" << std::endl;
        concurrent(fixture.root);
        std::cout
            << "PASS atomic replacement, Unicode/binary/empty, all failure stages, partial "
               "write, exclusive collision, permissions/locks, concurrent saves and cleanup\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
