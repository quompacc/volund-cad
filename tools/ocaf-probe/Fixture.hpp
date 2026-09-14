#pragma once
#include <TDocStd_Application.hxx>
#include <TDocStd_Document.hxx>
#include <filesystem>

namespace probe {
void require(bool value, const char* message);
Handle(TDocStd_Application) application();
Handle(TDocStd_Document) create(const Handle(TDocStd_Application) & app);
void check(const Handle(TDocStd_Document) & document, double width);
void change(const Handle(TDocStd_Document) & document, double width);
void verifyHistory(const Handle(TDocStd_Document) & document);
void save(const Handle(TDocStd_Application) & app, const Handle(TDocStd_Document) & document,
          const std::filesystem::path& path);
Handle(TDocStd_Document)
    read(const Handle(TDocStd_Application) & app, const std::filesystem::path& path);
} // namespace probe
