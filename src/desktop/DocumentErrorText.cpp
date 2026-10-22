// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "DocumentErrorText.hpp"

#include <QCoreApplication>

#include <array>
#include <string_view>

namespace voelund::desktop {

namespace {

struct OpenErrorText {
    std::string_view marker; // part of the reader's message
    const char* text;
};

// The readers' messages for a newer schema, an unknown encoding and damaged native data.
constexpr std::array openErrorTexts{
    OpenErrorText{"unsupported legacy schema version",
                  QT_TRANSLATE_NOOP("DocumentError", "Die Datei stammt aus einer neueren "
                                                     "Völund-Version und lässt sich mit dieser "
                                                     "Version nicht öffnen.")},
    OpenErrorText{"Unsupported native application schema",
                  QT_TRANSLATE_NOOP("DocumentError", "Die Datei stammt aus einer neueren "
                                                     "Völund-Version und lässt sich mit dieser "
                                                     "Version nicht öffnen.")},
    OpenErrorText{"unknown document encoding",
                  QT_TRANSLATE_NOOP("DocumentError",
                                    "Das ist keine Völund-Datei oder ein unbekanntes Format.")},
    OpenErrorText{
        "Invalid or unsupported native",
        QT_TRANSLATE_NOOP("DocumentError", "Die Datei ist beschädigt oder unvollständig.")},
};

} // namespace

QString documentOpenErrorText(const std::string& technical) {
    const char* text = QT_TRANSLATE_NOOP("DocumentError", "Die Datei lässt sich nicht öffnen.");
    for (const auto& candidate : openErrorTexts)
        if (technical.find(candidate.marker) != std::string::npos) {
            text = candidate.text;
            break;
        }
    return QCoreApplication::translate("DocumentError", text) + QStringLiteral(" (") +
           QString::fromStdString(technical) + QStringLiteral(")");
}

} // namespace voelund::desktop
