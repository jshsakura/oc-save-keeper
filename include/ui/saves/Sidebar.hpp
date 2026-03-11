#pragma once

#include "ui/saves/List.hpp"
#include "ui/saves/Widget.hpp"

#include <memory>
#include <vector>

namespace ui::saves {

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

protected:
    std::string m_title;
    std::string m_info;
};

class SidebarEntryCallback final : public SidebarEntryBase {
public:
    using Callback = std::function<void()>;

    SidebarEntryCallback(const std::string& title, Callback callback, bool popOnClick = false, const std::string& info = "");

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

    int index() const {
        return m_index;
    }

private:
    void setIndex(int index);
    void syncActions();

    std::string m_title;
    Side m_side = Side::Right;
    std::vector<std::unique_ptr<SidebarEntryBase>> m_items;
    std::unique_ptr<List> m_list;
    int m_index = 0;
};

} // namespace ui::saves
