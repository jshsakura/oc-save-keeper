#pragma once

#include "ui/saves/Types.hpp"

#include <string>

namespace ui::saves {

class ScrollingText {
public:
    void reset(const std::string& value = "");
    std::string clip(const std::string& value, std::size_t maxChars) const;

private:
    std::string m_value{};
};

} // namespace ui::saves
