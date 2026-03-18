#include "ui/saves/Sidebar.hpp"

#include "ui/saves/Runtime.hpp"
#include "utils/Language.hpp"

#include <chrono>

namespace ui::saves {

SidebarEntryBase::SidebarEntryBase(std::string title, std::string info)
    : m_title(std::move(title))
    , m_info(std::move(info)) {}

SidebarEntryCallback::SidebarEntryCallback(const std::string& title, Callback callback, bool popOnClick, const std::string& info, bool holdRequired)
    : SidebarEntryBase(title, info)
    , m_callback(std::move(callback))
    , m_popOnClick(popOnClick)
    , m_holdRequired(holdRequired) {
    setAction(Button::A, Action{utils::Language::instance().get("ui.confirm"), [this]() {
        activate();
    }});
}

void SidebarEntryCallback::activate() {
    if (!m_enabled) {
        return;
    }
    
    // If hold is required, activation happens via updateHold when progress reaches 1.0
    if (m_holdRequired) {
        return;
    }
    
    if (m_callback) {
        m_callback();
    }
    if (m_popOnClick) {
        setPop();
    }
}

void SidebarEntryCallback::updateHoldState(bool pressed, bool held, bool released) {
    if (!m_enabled) {
        return;
    }
    
    m_triggerGuard = m_triggerGuard || (pressed && !m_triggerGuard);
    
    const bool noHoldTrigger = m_triggerGuard && pressed && !m_holdRequired;
    const bool holdTriggered = m_triggerGuard && pressed && m_holdRequired;
    const bool holdSustained = m_triggerGuard && held && m_holdRequired;
    
    if (noHoldTrigger) {
        if (m_callback) {
            m_callback();
        }
        if (m_popOnClick) {
            setPop();
        }
    } else if (holdTriggered) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        m_holdStartTime = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count()
        );
        m_holdProgress = 0.0f;
    } else if (holdSustained) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        
        const uint64_t elapsed = static_cast<uint64_t>(nowMs) - m_holdStartTime;
        constexpr uint64_t holdDuration = 3000;
        m_holdProgress = std::min(static_cast<float>(elapsed) / static_cast<float>(holdDuration), 1.0f);
        
        if (elapsed >= holdDuration) {
            if (m_callback) {
                m_callback();
            }
            if (m_popOnClick) {
                setPop();
            }
            resetHold();
        }
    } else if (released) {
        resetHold();
    }
}

void SidebarEntryCallback::resetHold() {
    m_holdProgress = 0.0f;
    m_holdStartTime = 0;
    m_triggerGuard = false;
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

    auto* currentEntry = m_items[m_index].get();
    const bool isHoldable = currentEntry && currentEntry->isHoldable();
    const bool aPressed = controller.gotDown(Button::A);
    const bool aHeld = controller.gotHeld(Button::A);
    const bool aReleased = controller.gotUp(Button::A);

    if (isHoldable) {
        currentEntry->updateHoldState(aPressed, aHeld, aReleased);
    }

    m_list->onUpdate(controller, touch, m_index, static_cast<int>(m_items.size()), [this, isHoldable](bool tapped, int index) {
        setIndex(index);
        if (tapped && !isHoldable) {
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
    if (m_items.size() == 1) {
        syncActions();
    }
    return raw;
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
