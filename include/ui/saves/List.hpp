#pragma once

#include "ui/saves/Object.hpp"

#include <functional>

namespace ui::saves {

class List final : public Object {
public:
    enum class Layout {
        Home,
        Grid,
    };

    using DrawCallback = std::function<void(const Vec4& rect, int index)>;
    using TouchCallback = std::function<void(bool touch, int index)>;

    List(int row, int page, const Vec4& pos, const Vec4& itemRect, const Vec2& pad = {});

    void onUpdate(const Controller& controller, const TouchInfo& touch, int index, int count, const TouchCallback& callback);
    void drawItems(int count, const DrawCallback& callback) const;
    void draw() override {}

    bool scrollDown(int& index, int step, int count);
    bool scrollUp(int& index, int step, int count);

    void setLayout(Layout layout) {
        m_layout = layout;
    }

    Layout layout() const {
        return m_layout;
    }

    int row() const {
        return m_row;
    }

    int page() const {
        return m_page;
    }

    float yoff() const {
        return m_yoff;
    }

    void setYoff(float yoff) {
        m_yoff = yoff;
    }

    float maxY() const {
        return m_itemRect.h + m_pad.y;
    }

private:
    int m_row = 1;
    int m_page = 1;
    Vec4 m_itemRect{};
    Vec2 m_pad{};
    float m_yoff = 0.0f;
    Layout m_layout = Layout::Grid;
};

} // namespace ui::saves
