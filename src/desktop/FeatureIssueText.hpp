#pragma once

#include <QString>

#include <string>

namespace voelund::desktop {

// U15: what a failed feature means, in the user's words, by its stable code
// (features::EvaluationIssue::code); empty for codes without a text of their own. The
// evaluation's message stays technical English for logs, MCP and the assistant.
[[nodiscard]] QString issueText(const std::string& code);

// The text for the shell: the explanation of the code, or a general sentence, followed by the
// technical message in parentheses.
[[nodiscard]] QString issueExplanation(const std::string& code, const std::string& technical);

} // namespace voelund::desktop
