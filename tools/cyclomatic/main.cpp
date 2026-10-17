// voelund-cyclomatic: measures McCabe's cyclomatic complexity of every own function on
// Clang's CFG and fails when one exceeds the limit (docs/quality-tooling.md).
//
// Exit codes: 0 every function within the limit, 1 at least one function over the limit,
// 2 analysis error (a translation unit did not parse or a CFG could not be built).

#include "CyclomaticComplexity.hpp"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

llvm::cl::OptionCategory category("voelund-cyclomatic options");
llvm::cl::opt<unsigned> limit("limit", llvm::cl::desc("Highest allowed complexity"),
                              llvm::cl::init(10), llvm::cl::cat(category));
llvm::cl::list<std::string>
    roots("root",
          llvm::cl::desc("Measure functions declared under this directory (repeatable); "
                         "without any root only functions of the main files count"),
          llvm::cl::cat(category));
llvm::cl::list<std::string> excludes("exclude",
                                     llvm::cl::desc("Skip functions under this directory"),
                                     llvm::cl::cat(category));
llvm::cl::opt<bool> reportAll("report-all", llvm::cl::desc("Print the value of every function"),
                              llvm::cl::cat(category));

struct Measurement {
    std::string location;
    std::string name;
    unsigned value{0};
};

struct Collector {
    std::vector<std::string> roots;
    std::vector<std::string> excludes;
    // Keyed by location, so a function in a header counts once across translation units.
    std::map<std::string, Measurement> measurements;
};

std::string realPath(llvm::StringRef path) {
    llvm::SmallString<256> result;
    if (llvm::sys::fs::real_path(path, result))
        return path.str();
    return std::string(result);
}

bool underAny(const std::string& path, const std::vector<std::string>& directories) {
    for (const auto& directory : directories) {
        if (path.size() > directory.size() && path.compare(0, directory.size(), directory) == 0 &&
            path[directory.size()] == '/')
            return true;
    }
    return false;
}

class Visitor : public clang::RecursiveASTVisitor<Visitor> {
  public:
    Visitor(clang::ASTContext& context, Collector& collector)
        : context_(context), collector_(collector) {}

    bool VisitFunctionDecl(clang::FunctionDecl* function) {
        if (function->isImplicit() || function->isDefaulted() ||
            !function->doesThisDeclarationHaveABody())
            return true;
        measure(*function, voelund::quality::functionName(*function));
        return true;
    }

    // A lambda's body is its own function; the enclosing function's CFG skips it.
    bool VisitLambdaExpr(clang::LambdaExpr* lambda) {
        const auto* call = lambda->getCallOperator();
        measure(*call, voelund::quality::functionName(*call));
        return true;
    }

  private:
    bool owned(clang::SourceLocation location, const std::string& path) const {
        const auto& sources = context_.getSourceManager();
        if (sources.isInSystemHeader(location))
            return false;
        if (collector_.roots.empty())
            return sources.isInMainFile(location);
        return underAny(path, collector_.roots) && !underAny(path, collector_.excludes);
    }

    void measure(const clang::FunctionDecl& function, std::string name) {
        const auto& sources = context_.getSourceManager();
        const auto location = sources.getExpansionLoc(function.getLocation());
        const auto path = realPath(sources.getFilename(location));
        if (path.empty() || !owned(location, path))
            return;
        auto key = path + ":" + std::to_string(sources.getExpansionLineNumber(location)) + ":" +
                   std::to_string(sources.getExpansionColumnNumber(location));
        // A header template without an instantiation here may have one in a later unit.
        const auto known = collector_.measurements.find(key);
        if (known != collector_.measurements.end() && known->second.value != 0)
            return;
        const auto value = voelund::quality::cyclomaticComplexity(function, context_);
        collector_.measurements.insert_or_assign(key, Measurement{key, std::move(name), value});
    }

    clang::ASTContext& context_;
    Collector& collector_;
};

class Consumer : public clang::ASTConsumer {
  public:
    explicit Consumer(Collector& collector) : collector_(collector) {}
    void HandleTranslationUnit(clang::ASTContext& context) override {
        Visitor(context, collector_).TraverseDecl(context.getTranslationUnitDecl());
    }

  private:
    Collector& collector_;
};

class Action : public clang::ASTFrontendAction {
  public:
    explicit Action(Collector& collector) : collector_(collector) {}
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance&,
                                                          llvm::StringRef) override {
        return std::make_unique<Consumer>(collector_);
    }

  private:
    Collector& collector_;
};

class Factory : public clang::tooling::FrontendActionFactory {
  public:
    explicit Factory(Collector& collector) : collector_(collector) {}
    std::unique_ptr<clang::FrontendAction> create() override {
        return std::make_unique<Action>(collector_);
    }

  private:
    Collector& collector_;
};

std::vector<std::string> realPaths(const std::vector<std::string>& paths) {
    std::vector<std::string> result;
    for (const auto& path : paths)
        result.push_back(realPath(path));
    return result;
}

int report(const Collector& collector) {
    unsigned over = 0;
    unsigned failed = 0;
    for (const auto& [key, measurement] : collector.measurements) {
        if (reportAll)
            llvm::outs() << measurement.location << ": " << measurement.name << ": "
                         << measurement.value << "\n";
        if (measurement.value == 0) {
            ++failed;
            llvm::errs() << measurement.location << ": error: no CFG for '" << measurement.name
                         << "'\n";
        } else if (measurement.value > limit) {
            ++over;
            llvm::errs() << measurement.location << ": error: cyclomatic complexity "
                         << measurement.value << " of '" << measurement.name << "' exceeds limit "
                         << limit << "\n";
        }
    }
    llvm::errs() << "voelund-cyclomatic: " << collector.measurements.size()
                 << " functions measured, " << over << " over limit " << limit << ", " << failed
                 << " analysis errors\n";
    if (failed != 0)
        return 2;
    return over != 0 ? 1 : 0;
}

} // namespace

int main(int argc, const char** argv) {
    auto options = clang::tooling::CommonOptionsParser::create(argc, argv, category);
    if (!options) {
        llvm::errs() << llvm::toString(options.takeError()) << "\n";
        return 2;
    }
    clang::tooling::ClangTool tool(options->getCompilations(), options->getSourcePathList());
    // GCC's C++20 module scanning flags (CMake adds them to GCC compile commands).
    tool.appendArgumentsAdjuster(
        [](const clang::tooling::CommandLineArguments& arguments, llvm::StringRef) {
            clang::tooling::CommandLineArguments kept;
            for (const auto& argument : arguments) {
                const llvm::StringRef text(argument);
                if (text != "-fmodules-ts" && !text.starts_with("-fmodule-mapper=") &&
                    !text.starts_with("-fdeps-"))
                    kept.push_back(argument);
            }
            return kept;
        });
    // Built-in headers of the Clang this tool links against; warnings are not the metric.
    tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
        {"-resource-dir=" VOELUND_CLANG_RESOURCE_DIR, "-w"},
        clang::tooling::ArgumentInsertPosition::END));

    Collector collector{realPaths(roots), realPaths(excludes), {}};
    Factory factory(collector);
    if (tool.run(&factory) != 0) {
        llvm::errs() << "voelund-cyclomatic: a translation unit could not be analysed\n";
        report(collector);
        return 2;
    }
    return report(collector);
}
