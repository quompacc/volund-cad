// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CyclomaticComplexity.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/StmtCXX.h>
#include <clang/Analysis/CFG.h>

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

namespace voelund::quality {

namespace {

clang::CFG::BuildOptions buildOptions() {
    clang::CFG::BuildOptions options;
    // Constant conditions are still decisions in the source; keep both edges so the value
    // does not depend on what the compiler can fold.
    options.PruneTriviallyFalseEdges = false;
    // Implicit destructor and exception edges are not written control flow.
    options.AddImplicitDtors = false;
    options.AddTemporaryDtors = false;
    options.AddEHEdges = false;
    return options;
}

bool isTryDispatch(const clang::CFGBlock& block) {
    return llvm::isa_and_nonnull<clang::CXXTryStmt>(block.getTerminatorStmt());
}

// Without exception edges a try statement's dispatch block (whose successors are the catch
// handlers) has no predecessor. It counts as entered once; each handler is one branch, the
// "no handler matched" edge to the caller is not written control flow and does not count.
bool countsAsEdge(const clang::CFGBlock& from, const clang::CFGBlock& to) {
    if (!isTryDispatch(from))
        return true;
    return llvm::isa_and_nonnull<clang::CXXCatchStmt>(to.getLabel());
}

class Walk {
  public:
    void enter(const clang::CFGBlock* block) {
        if (seen_.insert(block).second)
            pending_.push_back(block);
    }

    void run() {
        while (!pending_.empty()) {
            const auto* block = pending_.back();
            pending_.pop_back();
            for (const auto& successor : block->succs()) {
                const auto* next = successor.getReachableBlock();
                if (next == nullptr || !countsAsEdge(*block, *next))
                    continue;
                ++edges_;
                enter(next);
            }
        }
    }

    void addEdge() { ++edges_; }
    long complexity() const { return edges_ - static_cast<long>(seen_.size()) + 2; }

  private:
    std::set<const clang::CFGBlock*> seen_;
    std::vector<const clang::CFGBlock*> pending_;
    long edges_{0};
};

unsigned measuredBody(const clang::FunctionDecl& function, clang::ASTContext& context);

// Clang builds no CFG for some uninstantiated patterns (a range-for over a dependent type has
// no begin/end yet). Such a template is measured on its instantiations, taking the highest.
unsigned measuredInstantiations(const clang::FunctionDecl& pattern, clang::ASTContext& context) {
    const auto* described = pattern.getDescribedFunctionTemplate();
    if (described == nullptr)
        return 0;
    unsigned value = 0;
    for (const auto* specialization : described->specializations())
        value = std::max(value, measuredBody(*specialization, context));
    return value;
}

unsigned measuredBody(const clang::FunctionDecl& function, clang::ASTContext& context) {
    auto* body = function.getBody();
    if (body == nullptr)
        return 0;
    const auto cfg = clang::CFG::buildCFG(&function, body, &context, buildOptions());
    if (!cfg)
        return 0;

    // Only blocks reachable from the entry count: code after a return is not a path.
    Walk walk;
    walk.enter(&cfg->getEntry());
    for (const auto* block : *cfg) {
        if (isTryDispatch(*block)) {
            walk.addEdge();
            walk.enter(block);
        }
    }
    walk.run();
    const long value = walk.complexity();
    return value < 1 ? 1U : static_cast<unsigned>(value);
}

} // namespace

unsigned cyclomaticComplexity(const clang::FunctionDecl& function, clang::ASTContext& context) {
    const auto value = measuredBody(function, context);
    return value != 0 ? value : measuredInstantiations(function, context);
}

std::string functionName(const clang::FunctionDecl& function) {
    const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(&function);
    if (method == nullptr || !method->getParent()->isLambda())
        return function.getQualifiedNameAsString();
    const auto* scope = llvm::dyn_cast_or_null<clang::NamedDecl>(
        clang::Decl::castFromDeclContext(method->getParent()->getDeclContext()));
    return (scope != nullptr ? scope->getQualifiedNameAsString() : std::string()) + "::(lambda)";
}

} // namespace voelund::quality
