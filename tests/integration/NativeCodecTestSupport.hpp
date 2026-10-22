// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

// OCAF document helpers of the native codec tests.

#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <TDF_Reference.hxx>
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <istream>
#include <ostream>
#include <stdexcept>

namespace voelund::test::native_codec {

inline void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Action> void rejected(Action action) {
    bool failed = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed, "Invalid native cylinder data accepted");
}
inline Handle(TDocStd_Document) fresh(const Handle(TDocStd_Application) & app) {
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
inline void save(const Handle(TDocStd_Application) & app, const Handle(TDocStd_Document) & doc,
                 std::ostream& output) {
    require(app->SaveAs(doc, output) == PCDM_SS_OK && output.good(), "Native save failed");
}
inline Handle(TDocStd_Document) open(const Handle(TDocStd_Application) & app, std::istream& input) {
    Handle(TDocStd_Document) doc;
    require(app->Open(input, doc) == PCDM_RS_OK && !doc.IsNull(), "Native open failed");
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
inline TDF_Label feature(const Handle(TDocStd_Document) & doc, int position) {
    Handle(TDF_Reference) ref;
    require(
        doc->Main().FindChild(20).FindChild(position).FindAttribute(TDF_Reference::GetID(), ref),
        "Missing order reference");
    return ref->Get();
}

} // namespace voelund::test::native_codec
