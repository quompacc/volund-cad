// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

#include <stdexcept>

namespace voelund::test {

// U12a: desktop tests never share persisted settings. Each process writes its own INI
// files below a temporary directory, so an aborted run cannot leak state (for example
// ui/toolRail) into later tests or into the product's settings.
inline void isolateSettings() {
    static QTemporaryDir directory;
    if (!directory.isValid())
        throw std::runtime_error("No temporary settings directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, directory.path());
    QCoreApplication::setOrganizationName(QStringLiteral("VoelundTest"));
    // L05: autosave of a test window never writes into the user's recovery directory.
    if (qEnvironmentVariableIsEmpty("VOELUND_RECOVERY_DIR"))
        qputenv("VOELUND_RECOVERY_DIR", (directory.path() + QStringLiteral("/recovery")).toUtf8());
}

} // namespace voelund::test
