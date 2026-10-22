// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <iostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>
#include <voelund/application/CommandTransaction.hpp>

namespace {
using namespace voelund;
using namespace application;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
// Test-only publication adapter: two scalar parameters stand for compound edits.
// This is not a replacement ModelDocument or a production history implementation.
struct State {
    std::vector<int> parameters{10};
    unsigned nextId{2};
    unsigned selection{1};
    features::EvaluationReport evaluation;
};
struct Published {
    State state;
    std::vector<State> undo;
    std::vector<State> redo;
    std::vector<int> saved{10};
};
struct MemoryStore {
    std::unique_ptr<Published> published{std::make_unique<Published>()};
    int commits{0};
    int rollbacks{0};
    bool failPrepare{false};
    bool dirty() const { return published->state.parameters != published->saved; }
    void save() { published->saved = published->state.parameters; }
    void undo() {
        auto next = std::make_unique<Published>(*published);
        next->redo.push_back(next->state);
        next->state = next->undo.back();
        next->undo.pop_back();
        published.swap(next);
    }
    void redo() {
        auto next = std::make_unique<Published>(*published);
        next->undo.push_back(next->state);
        next->state = next->redo.back();
        next->redo.pop_back();
        published.swap(next);
    }
};
class MemoryTransaction final : public CommandTransactionBackend {
  public:
    explicit MemoryTransaction(MemoryStore& store)
        : candidate(store.published->state), store_(store) {}
    State candidate;
    void prepareCommit(const features::EvaluationReport& evaluation) override {
        prepared_ = std::make_unique<Published>(*store_.published);
        prepared_->undo.push_back(prepared_->state);
        prepared_->redo.clear();
        prepared_->state = candidate;
        prepared_->state.evaluation = evaluation;
        if (store_.failPrepare)
            throw std::bad_alloc();
    }
    void commitPrepared() noexcept override {
        store_.published.swap(prepared_);
        ++store_.commits;
    }
    void rollback() noexcept override {
        prepared_.reset();
        ++store_.rollbacks;
    }

  private:
    MemoryStore& store_;
    std::unique_ptr<Published> prepared_;
};
features::EvaluationReport report(features::EvaluationStatus status) {
    return {{{document::FeatureId{2}, status, "fixture.evaluation", "Fixture message",
              "Fixture detail"}}};
}
void compoundFixture() {
    MemoryStore store;
    {
        auto backend = std::make_unique<MemoryTransaction>(store);
        backend->candidate.parameters.push_back(20);
        backend->candidate.parameters.push_back(30);
        backend->candidate.nextId = 4;
        backend->candidate.selection = 3;
        CommandTransaction transaction(CommandKind::newFeature, std::move(backend));
        require(store.published->state.parameters == std::vector<int>{10}, "preview leaked");
        auto result = transaction.finish(report(features::EvaluationStatus::valid));
        require(result.status() == CommandStatus::committed &&
                    result.reason() == RejectionReason::none,
                "valid compound command rejected");
        require(transaction.finish({}).reason() == RejectionReason::alreadyFinished &&
                    transaction.cancel().reason() == RejectionReason::alreadyFinished,
                "second commit allowed");
    }
    require(store.commits == 1 && store.rollbacks == 0 && store.published->undo.size() == 1 &&
                store.published->state.parameters == std::vector<int>({10, 20, 30}) &&
                store.dirty(),
            "compound command did not have one boundary");
    store.save();
    require(!store.dirty() && store.published->undo.size() == 1, "save changed history");
    store.undo();
    require(store.published->state.parameters == std::vector<int>{10} && store.dirty() &&
                store.published->state.nextId == 2 && store.published->state.selection == 1,
            "undo state");
    store.redo();
    require(!store.dirty() && store.published->state.nextId == 4 &&
                store.published->state.selection == 3,
            "redo save state");
    const auto* before = store.published.get();
    {
        auto backend = std::make_unique<MemoryTransaction>(store);
        backend->candidate.parameters.push_back(40);
        CommandTransaction transaction(CommandKind::editFeature, std::move(backend));
        require(transaction.cancel().reason() == RejectionReason::cancelled, "cancel reason");
    }
    {
        auto backend = std::make_unique<MemoryTransaction>(store);
        backend->candidate.selection = 99;
        CommandTransaction transaction(CommandKind::editFeature, std::move(backend));
        // Exception leaving a partially edited scope must discard its candidate.
    }
    require(store.published.get() == before && store.rollbacks == 2,
            "cancel/destructor published data");
    store.failPrepare = true;
    bool threw = false;
    {
        CommandTransaction transaction(CommandKind::editFeature,
                                       std::make_unique<MemoryTransaction>(store));
        try {
            static_cast<void>(transaction.finish({}));
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        require(transaction.finish({}).reason() == RejectionReason::alreadyFinished,
                "failed commit reusable");
    }
    require(threw && store.published.get() == before && store.commits == 1 && store.rollbacks == 3,
            "prepare exception changed publication");
}
void errorFixture() {
    for (const auto kind : {CommandKind::newFeature, CommandKind::editFeature}) {
        for (const auto disposition :
             {ErrorDisposition::reject, ErrorDisposition::keepEditableErrors}) {
            MemoryStore store;
            auto backend = std::make_unique<MemoryTransaction>(store);
            backend->candidate.parameters[0] = 99;
            CommandTransaction transaction(kind, std::move(backend));
            auto evaluation = report(features::EvaluationStatus::failed);
            evaluation.features.push_back({document::FeatureId{3},
                                           features::EvaluationStatus::blockedByDependency,
                                           "fixture.blocked", "Upstream failure", "feature=2"});
            auto result = transaction.finish(evaluation, disposition);
            const bool allowed = kind == CommandKind::editFeature &&
                                 disposition == ErrorDisposition::keepEditableErrors;
            require(result.status() ==
                        (allowed ? CommandStatus::committedWithErrors : CommandStatus::rejected),
                    "wrong explicit error policy");
            require(result.evaluation().features.size() == 2 &&
                        result.evaluation().features[0].code == "fixture.evaluation",
                    "diagnostics lost");
            require(store.commits == (allowed ? 1 : 0) && store.dirty() == allowed,
                    "rejection modified document");
            if (allowed) {
                require(store.published->state.evaluation.features[1].status ==
                            features::EvaluationStatus::blockedByDependency,
                        "evaluation not published with parameters");
                store.save();
                require(!store.dirty(), "editable error cannot become clean");
            }
        }
    }
    MemoryStore store;
    for (const auto reason : {RejectionReason::invalidInput, RejectionReason::noChange,
                              RejectionReason::incompleteEvaluation}) {
        CommandTransaction transaction(CommandKind::editFeature,
                                       std::make_unique<MemoryTransaction>(store));
        const auto result = reason == RejectionReason::invalidInput ? transaction.rejectInput({})
                            : reason == RejectionReason::noChange
                                ? transaction.noChange()
                                : transaction.finish(report(features::EvaluationStatus::dirty),
                                                     ErrorDisposition::keepEditableErrors);
        require(result.status() == CommandStatus::rejected && result.reason() == reason,
                "rejection reason");
    }
    require(store.commits == 0 && store.rollbacks == 3 && !store.dirty(),
            "noncommits changed state");
    CommandTransaction suppressed(CommandKind::editFeature,
                                  std::make_unique<MemoryTransaction>(store));
    require(suppressed.finish(report(features::EvaluationStatus::suppressed)).status() ==
                CommandStatus::committed,
            "suppressed feature treated as failed");
}
} // namespace
int main(int argc, char** argv) {
    static_assert(!std::is_convertible_v<CommandResult, bool>);
    static_assert(!std::is_copy_constructible_v<CommandTransaction>);
    try {
        require(argc == 2, "fixture name required");
        if (std::string_view(argv[1]) == "compound")
            compoundFixture();
        else if (std::string_view(argv[1]) == "errors")
            errorFixture();
        else
            throw std::runtime_error("unknown fixture");
        std::cout << "Command contract fixture passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}