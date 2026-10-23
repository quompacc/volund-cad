// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "NativeDocumentStore.hpp"
#include "BoundedNativeOutput.hpp"
#include "DocumentSerializationDetail.hpp"
#include "NativeContainerStructure.hpp"
#include "NativePrimitiveCodec.hpp"
#include <BinDrivers.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <TDocStd_Application.hxx>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <voelund/kernel/KernelError.hpp>

namespace voelund::document::detail {
namespace {
struct NativeSession final {
    Handle(TDocStd_Application) application = new TDocStd_Application;
    Handle(TDocStd_Document) document;
    NativeSession() { BinDrivers::DefineFormat(application); }
    ~NativeSession() {
        if (!document.IsNull()) {
            try {
                kernel::translateEveryOcctFailure(kernel::KernelOperation::nativeDocument,
                                                  [this] { application->Close(document); });
            } catch (const kernel::KernelError&) { /* Never mask the original failure. */
            }
        }
    }
};
} // namespace
std::string NativeDocumentStore::encode(const IdentifiedDocumentSnapshot& snapshot) {
    std::string bytes;
    try {
        kernel::translateEveryOcctFailure(kernel::KernelOperation::nativeDocument, [&] {
            NativeSession session;
            session.application->NewDocument("BinOcaf", session.document);
            session.document->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
            NativePrimitiveCodec::write(session.document, snapshot);
            BoundedNativeOutput buffer;
            std::ostream output(&buffer);
            if (session.application->SaveAs(session.document, output) != PCDM_SS_OK ||
                !output.good())
                throw std::runtime_error("Native document serialization failed");
            bytes = buffer.release();
        });
    } catch (const kernel::KernelError& error) {
        throw std::runtime_error("Native document serialization: " + error.diagnostic());
    }
    if (bytes.size() > maximumDocumentBytes)
        throw std::invalid_argument("Native document byte limit exceeded");
    validateNativeContainerStructure(bytes, true);
    return bytes;
}
IdentifiedDocumentSnapshot NativeDocumentStore::decode(std::string_view bytes) {
    // Check transport size before OCCT opens or copies the payload. This does not
    // bound OCCT's internal allocations caused by malicious container metadata.
    if (bytes.empty() || bytes.size() > maximumDocumentBytes)
        throw std::invalid_argument("Invalid native document byte count");
    validateNativeContainerStructure(bytes, true);
    std::optional<IdentifiedDocumentSnapshot> snapshot;
    try {
        kernel::translateEveryOcctFailure(kernel::KernelOperation::nativeDocument, [&] {
            NativeSession session;
            std::istringstream input(std::string(bytes), std::ios::binary);
            if (session.application->Open(input, session.document) != PCDM_RS_OK ||
                session.document.IsNull())
                throw std::invalid_argument("Native document container could not be read");
            // Application marker/schema, identities and all model fields are validated
            // before returning a value. No partially loaded ModelDocument is published.
            snapshot = NativePrimitiveCodec::read(session.document);
        });
    } catch (const kernel::KernelError& error) {
        throw std::invalid_argument("Native document read: " + error.diagnostic());
    }
    return std::move(*snapshot);
}
} // namespace voelund::document::detail
