#include "Fixture.hpp"
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <TDataStd_Integer.hxx>
#include <fstream>
#include <iostream>
#include <string>

namespace {
void negative(const std::filesystem::path& directory) {
    const auto corrupt = directory / "corrupt.volund";
    {
        std::ofstream file(corrupt, std::ios::binary);
        file << "not an OCAF document";
    }
    auto app = probe::application();
    bool rejected = false;
    try {
        static_cast<void>(probe::read(app, corrupt));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    probe::require(rejected, "Corrupt file accepted");
    const auto future = directory / "future.volund";
    auto doc = probe::create(app);
    TDataStd_Integer::Set(doc->Main(), 999);
    probe::save(app, doc, future);
    app->Close(doc);
    rejected = false;
    try {
        static_cast<void>(probe::read(app, future));
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()) == "Unsupported probe schema";
    }
    probe::require(rejected, "Future schema accepted");
}
} // namespace
int main(int argc, char** argv) {
    try {
        probe::require(argc >= 3, "Usage: create FILE | read FILE [REWRITE] | negative DIRECTORY");
        const std::string mode = argv[1];
        if (mode == "negative")
            negative(argv[2]);
        else {
            auto app = probe::application();
            if (mode == "create") {
                auto doc = probe::create(app);
                probe::save(app, doc, argv[2]);
                app->Close(doc);
            } else {
                probe::require(mode == "read", "Unknown mode");
                auto doc = probe::read(app, argv[2]);
                probe::verifyHistory(doc);
                if (argc == 4)
                    probe::save(app, doc, argv[3]);
                app->Close(doc);
            }
        }
        std::cout << "{\"schemaVersion\":1,\"occt\":\"" << OCC_VERSION_COMPLETE
                  << "\",\"writerFormat\":12,\"mode\":\"" << mode
                  << "\",\"status\":\"passed\",\"checks\":\"schema,ids,unicode,reference,parameter,"
                     "valid-brep,volume,bounds,topology,naming-history,undo-redo\"}\n";
        return 0;
    } catch (const Standard_Failure& error) {
#if OCC_VERSION_MAJOR >= 8
        std::cerr << "OCCT failure: " << error.what() << '\n';
#else
        std::cerr << "OCCT failure: "
                  << (error.GetMessageString() ? error.GetMessageString() : "no diagnostic")
                  << '\n';
#endif
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
