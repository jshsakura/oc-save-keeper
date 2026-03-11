#include "ui/saves/List.hpp"

#include <algorithm>

namespace ui::saves {

List::List(int row, int page, const Vec4& pos, const Vec4& itemRect, const Vec2& pad)
    : m_row(row)
    , m_page(page)
    , m_itemRect(itemRect)
    , m_pad(pad) {
    setPos(pos);
}

void List::onUpdate(const Controller& controller, const TouchInfo& touch, int index, int count, const TouchCallback& callback) {
    (void)touch;
    int nextIndex = index;

    if (controller.gotDown(Button::Down)) {
        scrollDown(nextIndex, m_row, count);
    } else if (controller.gotDown(Button::Up)) {
        scrollUp(nextIndex, m_row, count);
    } else if (m_row > 1 && controller.gotDown(Button::Right)) {
        nextIndex = std::min(index + 1, std::max(0, count - 1));
    } else if (m_row > 1 && controller.gotDown(Button::Left)) {
        nextIndex = std::max(0, index - 1);
    }

    if (nextIndex != index) {
        callback(false, nextIndex);
    }
}

void List::drawItems(int count, const DrawCallback& callback) const {
    Vec4 rect = m_itemRect;
    rect.y -= m_yoff;

    for (int i = 0; i < count; ++i) {
        if (m_layout == Layout::Home) {
            rect.x = m_itemRect.x + i * (m_itemRect.w + m_pad.x);
            rect.y = m_itemRect.y;
        } else {
            const int rowIndex = i / m_row;
            const int colIndex = i % m_row;
            rect.x = m_itemRect.x + colIndex * (m_itemRect.w + m_pad.x);
            rect.y = m_itemRect.y + rowIndex * (m_itemRect.h + m_pad.y) - m_yoff;
        }
        callback(rect, i);
    }
}

bool List::scrollDown(int& index, int step, int count) {
    if (count <= 0) {
        return false;
    }
    const int old = index;
    index = std::min(index + step, count - 1);
    if (index != old) {
        const int pageStart = static_cast<int>(m_yoff / maxY()) * m_row;
        if (index >= pageStart + m_page) {
            m_yoff += maxY();
        }
        return true;
    }
    return false;
}

bool List::scrollUp(int& index, int step, int count) {
    (void)count;
    const int old = index;
    index = std::max(0, index - step);
    if (index != old) {
        const int pageStart = static_cast<int>(m_yoff / maxY()) * m_row;
        if (index < pageStart) {
            m_yoff = std::max(0.0f, m_yoff - maxY());
        }
        return true;
    }
    return false;
}

} // namespace ui::saves
