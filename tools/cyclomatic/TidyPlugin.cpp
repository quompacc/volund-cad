// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

// clang-tidy plugin with the check voelund-cyclomatic-complexity: the same McCabe measurement
// as voelund-cyclomatic, so CI parses every translation unit once for both the function-size
// rules and the cyclomatic limit (A04):
//   clang-tidy --load=libVoelundTidyPlugin.so --checks=-*,readability-function-size,voelund-*

#include "CyclomaticComplexity.hpp"

#include <clang-tidy/ClangTidyCheck.h>
#include <clang-tidy/ClangTidyModule.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

namespace voelund::quality {

namespace {

using namespace clang::ast_matchers;

class CyclomaticComplexityCheck : public clang::tidy::ClangTidyCheck {
  public:
    CyclomaticComplexityCheck(llvm::StringRef name, clang::tidy::ClangTidyContext* context)
        : ClangTidyCheck(name, context), threshold_(Options.get("Threshold", 10U)) {}

    void storeOptions(clang::tidy::ClangTidyOptions::OptionMap& options) override {
        Options.store(options, "Threshold", threshold_);
    }

    // Template patterns, not their instantiations; lambda bodies are their own functions.
    void registerMatchers(MatchFinder* finder) override {
        finder->addMatcher(functionDecl(isDefinition(), unless(isImplicit()), unless(isDefaulted()),
                                        unless(isTemplateInstantiation()))
                               .bind("function"),
                           this);
    }

    void check(const MatchFinder::MatchResult& result) override {
        const auto* function = result.Nodes.getNodeAs<clang::FunctionDecl>("function");
        if (function == nullptr || !function->hasBody())
            return;
        const auto value = cyclomaticComplexity(*function, *result.Context);
        if (value == 0)
            diag(function->getLocation(), "no CFG for '%0'") << functionName(*function);
        else if (value > threshold_)
            diag(function->getLocation(), "cyclomatic complexity %0 of '%1' exceeds limit %2")
                << value << functionName(*function) << threshold_;
    }

  private:
    unsigned threshold_;
};

class VoelundModule : public clang::tidy::ClangTidyModule {
  public:
    void addCheckFactories(clang::tidy::ClangTidyCheckFactories& factories) override {
        factories.registerCheck<CyclomaticComplexityCheck>("voelund-cyclomatic-complexity");
    }
};

} // namespace

} // namespace voelund::quality

static const clang::tidy::ClangTidyModuleRegistry::Add<voelund::quality::VoelundModule>
    registration("voelund-module", "Völund quality checks (A04)");
