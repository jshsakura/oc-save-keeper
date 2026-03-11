#include "ui/saves/ScrollingText.hpp"

namespace ui::saves {

void ScrollingText::reset(const std::string& value) {
    m_value = value;
}

std::string ScrollingText::clip(const std::string& value, std::size_t maxChars) const {
    if (value.size() <= maxChars) {
        return value;
    }
    if (maxChars <= 3) {
        return value.substr(0, maxChars);
    }
    return value.substr(0, maxChars - 3) + "...";
}

} // namespace ui::saves
