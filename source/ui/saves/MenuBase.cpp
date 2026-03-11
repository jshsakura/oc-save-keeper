#include "ui/saves/MenuBase.hpp"

namespace ui::saves {

MenuBase::MenuBase(const std::string& title, unsigned int flags)
    : m_title(title)
    , m_flags(flags) {}

void MenuBase::update(const Controller& controller, const TouchInfo& touch) {
    Widget::update(controller, touch);
}

void MenuBase::draw() {
    Widget::draw();
}

PolledData MenuBase::getPolledData(bool forceRefresh) {
    (void)forceRefresh;
    return {};
}

} // namespace ui::saves
