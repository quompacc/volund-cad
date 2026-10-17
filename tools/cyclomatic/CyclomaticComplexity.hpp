#pragma once

namespace clang {
class ASTContext;
class FunctionDecl;
} // namespace clang

namespace voelund::quality {

// McCabe's cyclomatic complexity of one function body, measured on Clang's CFG as
// M = E - N + 2 over the blocks reachable from the entry block. A template whose pattern has
// no CFG counts its highest instantiation. Returns 0 when there is still no CFG (an analysis
// error, not a measurement).
unsigned cyclomaticComplexity(const clang::FunctionDecl& function, clang::ASTContext& context);

} // namespace voelund::quality
