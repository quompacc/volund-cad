#include "../../src/desktop/FeatureIssueText.hpp"

#include <QCoreApplication>
#include <iostream>
#include <string>
#include <vector>

// U15: every code the evaluation produces today has German text; an unknown code gets the
// general sentence, and the technical message always follows in parentheses.
namespace {
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    try {
        const std::vector<std::string> codes{
            // kernel::booleanFailureCode
            "join-disjoint", "cut-no-material", "cut-whole-body", "intersect-empty",
            "fillet-failed", "chamfer-failed", "shell-failed",
            // features
            "fillet-edge-missing", "chamfer-edge-missing", "hole-face-missing",
            "shell-face-missing", "revolve-axis-missing", "helix-one-region",
            "datum-reference-missing", "datum-axis-edge", "combine-body-missing",
            "mirror-plane-missing", "mirror-body-missing", "move-body-missing",
            "pattern-axis-missing", "pattern-body-missing", "draft-face-missing",
            "draft-neutral-missing", "draft-face-refused", "loft-section-missing",
            "loft-section-hole", "loft-coplanar", "sweep-path-missing", "sweep-path-disconnected",
            "import-part-missing", "region-needs-repair", "region-open", "region-overlap",
            "profile-invalid", "sketch-unsolved", "sketch-support-missing",
            "sketch-support-unsolved", "sketch-plane-missing", "sketch-needs-repair",
            "end-face-missing", "end-face-not-parallel", "end-face-behind", "profile-face-missing",
            "body-missing", "base-body-exists", "extrusion-needs-target", "blocked-by-dependency",
            "rolled-back", "stale-input"};
        for (const auto& code : codes)
            require(!voelund::desktop::issueText(code).isEmpty(), "German text for " + code);

        const auto known = voelund::desktop::issueExplanation("sketch-unsolved",
                                                              "Sketch parameters cannot be solved");
        require(known.startsWith(QStringLiteral("Skizze:")) &&
                    known.endsWith(QStringLiteral("(Sketch parameters cannot be solved)")),
                "A known code: German text, technical message after it");
        const auto unknown =
            voelund::desktop::issueExplanation("feature-evaluation-failed", "Contour builder");
        require(unknown == QStringLiteral("Dieser Schritt lässt sich nicht berechnen. "
                                          "(Contour builder)"),
                "An unknown code: the general sentence");
        require(voelund::desktop::issueExplanation("sketch-unsolved", "").startsWith("Skizze:"),
                "Without a technical message only the explanation");
        std::cout << "PASS feature issue texts: " << codes.size() << " codes in German\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
