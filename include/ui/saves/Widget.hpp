#pragma once

#include "ui/saves/Object.hpp"

#include <map>

namespace ui::saves {

class Widget : public Object {
public:
    using Actions = std::map<Button, Action>;

    virtual ~Widget() = default;

    virtual void update(const Controller& controller, const TouchInfo& touch);
    void draw() override;
    ObjectKind kind() const override {
        return ObjectKind::Widget;
    }

    bool hasAction(Button button) const;
    void setAction(Button button, const Action& action);
    bool fireAction(Button button, u8 type = ActionTypeDown);
    const Actions& actions() const {
        return m_actions;
    }

    bool shouldPop() const {
        return m_shouldPop;
    }

    void setPop(bool shouldPop = true) {
        m_shouldPop = shouldPop;
    }

protected:
    Actions m_actions{};
    bool m_shouldPop = false;
};

} // namespace ui::saves
