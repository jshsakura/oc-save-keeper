#pragma once

#include "ui/saves/List.hpp"
#include "ui/saves/Widget.hpp"

#include <functional>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ui::saves {

// Action style for visual differentiation in sidebar entries
enum class ActionStyle {
    Default,    // Neutral/default styling
    Destructive, // Red - for delete/dangerous actions
    Positive,    // Yellow/gold - for favorite/positive actions
    Primary      // Blue - for restore/primary actions
};

class SidebarEntryBase : public Widget {
public:
    SidebarEntryBase(std::string title, std::string info = "");
    virtual ~SidebarEntryBase() = default;

    const std::string& title() const {
        return m_title;
    }

    const std::string& info() const {
        return m_info;
    }

    void setInfo(const std::string& info) {
        m_info = info;
    }

    bool isEnabled() const {
        return m_enabled;
    }

    void setEnabled(bool enabled) {
        m_enabled = enabled;
    }

    ActionStyle actionStyle() const {
        return m_actionStyle;
    }

    void setActionStyle(ActionStyle style) {
        m_actionStyle = style;
    }

protected:
    std::string m_title;
    std::string m_info;
    bool m_enabled = true;
    ActionStyle m_actionStyle = ActionStyle::Default;
};

class SidebarEntryCallback final : public SidebarEntryBase {
public:
    using Callback = std::function<void()>;

    SidebarEntryCallback(const std::string& title, Callback callback, bool popOnClick = false, const std::string& info = "", ActionStyle style = ActionStyle::Default);

    void activate();

private:
    Callback m_callback;
    bool m_popOnClick = false;
};

class Sidebar final : public Widget {
public:
    enum class Side {
        Left,
        Right,
    };

    Sidebar(const std::string& title, Side side);

    void update(const Controller& controller, const TouchInfo& touch) override;
    void draw() override;

    SidebarEntryBase* add(std::unique_ptr<SidebarEntryBase>&& entry);

    template <typename T, typename... Args>
    T* add(Args&&... args) {
        return static_cast<T*>(add(std::make_unique<T>(std::forward<Args>(args)...)));
    }

    const std::vector<std::unique_ptr<SidebarEntryBase>>& items() const {
        return m_items;
    }

    const std::string& title() const {
        return m_title;
    }

    static int resolveInitialIndex(int requestedIndex, std::size_t itemCount) {
        if (itemCount == 0) {
            return 0;
        }

        if (requestedIndex < 0) {
            return 0;
        }

        const std::size_t safeIndex = static_cast<std::size_t>(requestedIndex);
        if (safeIndex >= itemCount) {
            return static_cast<int>(itemCount - 1);
        }

        return requestedIndex;
    }

    void setInitialIndex(int index);

    int index() const {
        return m_index;
    }

    void setStatusMessage(const std::string& msg) { m_statusMessage = msg; }
    const std::string& statusMessage() const { return m_statusMessage; }

private:
    void setIndex(int index);
    void syncActions();

    std::string m_title;
    Side m_side = Side::Right;
    std::vector<std::unique_ptr<SidebarEntryBase>> m_items;
    std::unique_ptr<List> m_list;
    int m_initialIndex = 0;
    int m_index = 0;
    std::string m_statusMessage;
};

} // namespace ui::saves
