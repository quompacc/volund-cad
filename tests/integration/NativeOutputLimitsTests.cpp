// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/document/BoundedNativeOutput.hpp"
#include "../../src/document/NativeDocumentStore.hpp"
#include "../../src/document/NativePrimitiveCodec.hpp"
#include <BinDrivers.hxx>
#include <PCDM_StoreStatus.hxx>
#include <TDocStd_Application.hxx>
#include <iostream>
#include <limits>
#include <sstream>
#include <voelund/kernel/OcctCompatibility.hpp>

namespace {
using namespace voelund::document;
using namespace voelund::document::detail;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void streamChecks() {
    BoundedNativeOutput buffer(5);
    std::ostream out(&buffer);
    out.write("abcde", 5);
    require(out.good() && buffer.bytes() == "abcde", "Exact limit rejected");
    out.seekp(1);
    out.write("XY", 2);
    out.seekp(0, std::ios::end);
    require(out.good() && buffer.bytes() == "aXYde" && out.tellp() == 5, "Backpatch changed size");
    out.put('!');
    require(out.bad() && buffer.bytes() == "aXYde", "Single-byte overflow changed output");
    out.clear();
    out.seekp(-1, std::ios::end);
    out.write("XX", 2);
    require(out.bad() && buffer.bytes() == "aXYde", "Block overflow partially wrote");
    for (const auto position : {std::numeric_limits<std::streamoff>::min(), std::streamoff{6},
                                std::numeric_limits<std::streamoff>::max()}) {
        out.clear();
        out.seekp(position);
        require(out.fail() && buffer.bytes() == "aXYde", "Invalid seek changed output");
    }
    out.clear();
    out.seekp(0, std::ios::end);
    out.exceptions(std::ios::badbit);
    bool failed = false;
    try {
        out.put('!');
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed && buffer.bytes() == "aXYde", "Overflow exception mask ignored");
    require(buffer.release() == "aXYde" && buffer.bytes().empty(),
            "Release did not transfer bytes");
    BoundedNativeOutput empty(0);
    std::ostream zero(&empty);
    zero.write("", 0);
    zero.flush();
    require(zero.good(), "Empty write failed");
    zero.put('x');
    require(zero.bad() && empty.bytes().empty(), "Zero limit exceeded");
}
void nativeChecks() {
    auto model = ModelDocument::empty();
    (void)model.addBoxFeature("Output Ω", {3, 4, 5});
    const auto expected =
        IdentifiedDocumentSnapshot::restore(model.documentId(), DocumentSnapshot::capture(model));
    Handle(TDocStd_Application) app = new TDocStd_Application;
    BinDrivers::DefineFormat(app);
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    NativePrimitiveCodec::write(doc, expected);
    std::ostringstream reference(std::ios::binary);
    require(app->SaveAs(doc, reference) == PCDM_SS_OK, "Reference serialization failed");
    BoundedNativeOutput exact(reference.str().size());
    std::ostream exactOut(&exact);
    require(app->SaveAs(doc, exactOut) == PCDM_SS_OK && exactOut.good(),
            "Exact native limit failed");
    require(NativeDocumentStore::decode(exact.bytes()) == expected,
            "Backpatch corrupted native output");
    BoundedNativeOutput shortBuffer(reference.str().size() - 1);
    std::ostream shortOut(&shortBuffer);
    try {
        (void)app->SaveAs(doc, shortOut);
    } catch (const Standard_Failure&) {
    }
    require(!shortOut.good() && shortBuffer.bytes().size() <= reference.str().size() - 1,
            "Native writer exceeded its output budget");
    require(NativePrimitiveCodec::read(doc) == expected, "Failed native output changed document");
    app->Close(doc);
    require(NativeDocumentStore::decode(NativeDocumentStore::encode(expected)) == expected,
            "Store failed after budget rejection");
}
} // namespace
int main() {
    try {
        streamChecks();
        nativeChecks();
        std::cout << "PASS native output: byte caps, backpatch, seek errors, OCCT roundtrip and "
                     "failure\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << voelund::kernel::occtFailureDiagnostic(error) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
