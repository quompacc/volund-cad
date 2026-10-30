// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/StepExchange.hpp>

#include <QMimeData>
#include <QString>
#include <QWidget>

#include <filesystem>

namespace voelund::desktop {

// I02: reads a STEP file on another thread. The window stays alive and shows how far it is, with a
// button to stop; a small file is done before anything shows.
[[nodiscard]] kernel::StepImport readStepFile(QWidget& parent, const std::filesystem::path& path);

// A file of the dropped data that Völund opens or imports; empty when there is none.
[[nodiscard]] QString droppedFile(const QMimeData& data);

} // namespace voelund::desktop
