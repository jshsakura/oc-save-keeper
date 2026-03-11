#include "ui/saves/Widget.hpp"

#include "ui/saves/Runtime.hpp"

namespace ui::saves {

namespace {

void invoke(const Action& action, bool down) {
    if (const auto* cb = std::get_if<std::function<void()>>(&action.callback)) {
        if (*cb) {
            (*cb)();
        }
        return;
    }

    if (const auto* cb = std::get_if<std::function<void(bool)>>(&action.callback)) {
        if (*cb) {
            (*cb)(down);
        }
    }
}

} // namespace

void Widget::update(const Controller& controller, const TouchInfo& touch) {
    for (const auto& [button, action] : m_actions) {
        if ((action.type & ActionTypeDown) && controller.gotDown(button)) {
            Runtime::instance().playSound(SoundEffect::Focus);
            invoke(action, true);
            return;
        }

        if ((action.type & ActionTypeUp) && controller.gotUp(button)) {
            invoke(action, false);
            return;
        }

        if ((action.type & ActionTypeHeld) && controller.gotHeld(button)) {
            invoke(action, true);
            return;
        }
    }

    if (!touch.isClicked) {
        return;
    }
}

void Widget::draw() {}

bool Widget::hasAction(Button button) const {
    return m_actions.find(button) != m_actions.end();
}

void Widget::setAction(Button button, const Action& action) {
    m_actions.insert_or_assign(button, action);
}

bool Widget::fireAction(Button button, u8 type) {
    const auto it = m_actions.find(button);
    if (it == m_actions.end() || (it->second.type & type) == 0) {
        return false;
    }

    Runtime::instance().playSound(SoundEffect::Focus);
    invoke(it->second, true);
    return true;
}

} // namespace ui::saves
