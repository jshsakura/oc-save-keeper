#include "ui/saves/Sidebar.hpp"

#include "ui/saves/Runtime.hpp"
#include "utils/Language.hpp"

#include <algorithm>

namespace ui::saves {

SidebarEntryBase::SidebarEntryBase(std::string title, std::string info)
    : m_title(std::move(title))
    , m_info(std::move(info)) {}

SidebarEntryCallback::SidebarEntryCallback(const std::string& title, Callback callback, bool popOnClick, const std::string& info)
    : SidebarEntryBase(title, info)
    , m_callback(std::move(callback))
    , m_popOnClick(popOnClick) {
    setAction(Button::A, Action{utils::Language::instance().get("ui.confirm"), [this]() {
        activate();
    }});
}

void SidebarEntryCallback::activate() {
    if (!m_enabled) {
        return;
    }
    
    if (m_popOnClick) {
        setPop();
    }
    
    if (m_callback) {
        m_callback();
    }
}

Sidebar::Sidebar(const std::string& title, Side side)
    : m_title(title)
    , m_side(side) {
    const float width = 420.0f;
    const float x = side == Side::Right ? 1280.0f - width : 0.0f;
    setPos({x, 0.0f, width, 720.0f});
    m_list = std::make_unique<List>(1, 6, getPos(), Vec4{x + 24.0f, 150.0f, width - 48.0f, 58.0f}, Vec2{0.0f, 10.0f});
}

void Sidebar::update(const Controller& controller, const TouchInfo& touch) {
    if (m_items.empty() || !m_list) {
        setPop();
        return;
    }

    m_list->onUpdate(controller, touch, m_index, static_cast<int>(m_items.size()), [this](bool tapped, int index) {
        setIndex(index);
        if (tapped) {
            fireAction(Button::A);
        }
    });

    Widget::update(controller, touch);

    if (m_items[m_index]->shouldPop()) {
        setPop();
    }
}

void Sidebar::draw() {
    Widget::draw();
}

SidebarEntryBase* Sidebar::add(std::unique_ptr<SidebarEntryBase>&& entry) {
    auto* raw = entry.get();
    m_items.push_back(std::move(entry));
    const int targetIndex = Sidebar::resolveInitialIndex(m_initialIndex, m_items.size());
    if (m_items.size() == 1 || m_index != targetIndex) {
        m_index = targetIndex;
        syncActions();
    }
    return raw;
}

void Sidebar::setInitialIndex(int index) {
    m_initialIndex = std::max(0, index);
    if (!m_items.empty()) {
        m_index = Sidebar::resolveInitialIndex(m_initialIndex, m_items.size());
        syncActions();
    }
}

void Sidebar::setIndex(int index) {
    if (m_items.empty()) {
        m_index = 0;
        return;
    }
    m_index = index;
    syncActions();
}

void Sidebar::syncActions() {
    m_actions.clear();
    if (m_items.empty()) {
        return;
    }

    for (const auto& [button, action] : m_items[m_index]->actions()) {
        setAction(button, action);
    }
    setAction(Button::B, Action{utils::Language::instance().get("detail.back"), [this]() {
        setPop();
    }});
}

} // namespace ui::saves
