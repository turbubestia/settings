#include <expected.hpp>

#include <format>

namespace turbubestia::settings {

auto error_stack::push(const error_frame &frame) -> void {
    m_frames.push_back(frame);
}

auto error_stack::frames() const -> const std::vector<error_frame> & {
    return m_frames;
}

auto error_stack::clear() -> void {
    m_frames.clear();
}

auto error_stack::to_string() const -> std::string {
    if (m_frames.empty()) {
        return "";
    }

    std::string result;
    for (const auto &frame : m_frames) {
        result += "Error " + std::to_string(frame.error_code) + ": " + frame.description + "\n";
        if (frame.location.file_name()) {
            result += std::format("  at {}:{}\n", frame.location.file_name(), frame.location.line());
        }
    }
    return result;
}

} // namespace turbubestia::settings
