#include "NativeDocumentStore.hpp"
#include "NativePrimitiveCodec.hpp"
#include "NativeContainerHeader.hpp"
#include "DocumentSerializationDetail.hpp"
#include <voelund/kernel/OcctCompatibility.hpp>
#include <BinDrivers.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <TDocStd_Application.hxx>
#include <sstream>
#include <stdexcept>

namespace voelund::document::detail {
namespace {
struct NativeSession final {
    Handle(TDocStd_Application) application = new TDocStd_Application;
    Handle(TDocStd_Document) document;
    NativeSession() { BinDrivers::DefineFormat(application); }
    ~NativeSession() {
        if (!document.IsNull()) {
            try { application->Close(document); }
            catch (const Standard_Failure&) { /* Never mask the original failure during cleanup. */ }
        }
    }
};
} // namespace
std::string NativeDocumentStore::encode(const IdentifiedDocumentSnapshot& snapshot) {
    try {
        NativeSession session;
        session.application->NewDocument("BinOcaf", session.document);
        session.document->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
        NativePrimitiveCodec::write(session.document, snapshot);
        std::ostringstream output(std::ios::binary);
        if (session.application->SaveAs(session.document, output) != PCDM_SS_OK || !output.good())
            throw std::runtime_error("Native document serialization failed");
        auto bytes = std::move(output).str();
        if (bytes.size() > maximumDocumentBytes)
            throw std::invalid_argument("Native document byte limit exceeded");
        (void)validateNativeContainerHeader(bytes);
        return bytes;
    } catch (const Standard_Failure& error) {
        throw std::runtime_error("Native document serialization: " + kernel::occtFailureDiagnostic(error));
    }
}
IdentifiedDocumentSnapshot NativeDocumentStore::decode(std::string_view bytes) {
    // Check transport size before OCCT opens or copies the payload. This does not
    // bound OCCT's internal allocations caused by malicious container metadata.
    if (bytes.empty() || bytes.size() > maximumDocumentBytes)
        throw std::invalid_argument("Invalid native document byte count");
    (void)validateNativeContainerHeader(bytes);
    try {
        NativeSession session;
        std::istringstream input(std::string(bytes), std::ios::binary);
        if (session.application->Open(input, session.document) != PCDM_RS_OK || session.document.IsNull())
            throw std::invalid_argument("Native document container could not be read");
        // Application marker/schema, identities and all model fields are validated
        // before returning a value. No partially loaded ModelDocument is published.
        return NativePrimitiveCodec::read(session.document);
    } catch (const Standard_Failure& error) {
        throw std::invalid_argument("Native document read: " + kernel::occtFailureDiagnostic(error));
    }
}
} // namespace voelund::document::detail
