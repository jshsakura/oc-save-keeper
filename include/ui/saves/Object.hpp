#pragma once

#include "ui/saves/Types.hpp"

namespace ui::saves {

enum class ObjectKind {
    Unknown,
    Widget,
    SaveMenuScreen,
    RevisionMenuScreen,
};

class Object {
public:
    virtual ~Object() = default;

    virtual void draw() = 0;
    virtual ObjectKind kind() const {
        return ObjectKind::Unknown;
    }

    Vec4 getPos() const {
        return m_pos;
    }

    void setPos(const Vec4& pos) {
        m_pos = pos;
    }

    bool isHidden() const {
        return m_hidden;
    }

    void setHidden(bool hidden = true) {
        m_hidden = hidden;
    }

protected:
    Vec4 m_pos{};
    bool m_hidden = false;
};

} // namespace ui::saves
