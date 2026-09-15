#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <iostream>
#include <limits>

int main() {
    using namespace voelund;
    application::CadSession session;
    document::SketchParameters sketch{
        {0, 0, 20, 0, 0, 1, 1, 0, 0},
        {{{document::SketchElementId{1}, document::SketchLine{{0, 0}, {5, 0}}}},
         std::numeric_limits<std::uint64_t>::max() - 1}};
    document::validateSketchParameters(sketch);
    application::SketchEditorSession editor(session, document::FeatureId{3},
                                            document::FeatureId{1}, sketch);
    editor.setTool(application::SketchDrawingTool::rectangle);
    const auto before = editor.draftParameters();
    const bool preview = editor.setPreview({1, 1}, {4, 4});
    const auto confirmed = editor.confirmPreview();
    const auto after = editor.draftParameters();
    bool zero = false;
    for (const auto& element : after.elements.values)
        zero = zero || element.id.value() == 0;
    std::cout << "preview=" << preview << " confirmed=" << confirmed.has_value()
              << " changed=" << (before != after) << " zero_id=" << zero
              << " next_id=" << after.elements.nextId << '\n';
    return zero && before != after ? 0 : 1;
}
