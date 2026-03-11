#pragma once

#include "ui/saves/ScrollingText.hpp"
#include "ui/saves/Widget.hpp"

#include <string>

namespace ui::saves {

struct PolledData {
    int signalStrength = 0;
    long long sdFree = 0;
    long long sdTotal = 0;
};

class MenuBase : public Widget {
public:
    enum MenuFlag {
        MenuFlagNone = 0,
        MenuFlagTab = 1 << 1,
    };

    MenuBase(const std::string& title, unsigned int flags);
    virtual ~MenuBase() = default;

    virtual const char* shortTitle() const = 0;
    virtual void update(const Controller& controller, const TouchInfo& touch);
    virtual void draw() override;

    bool isMenu() const {
        return true;
    }

    void setTitle(const std::string& title) {
        m_title = title;
    }

    void setTitleSubHeading(const std::string& value) {
        m_titleSubHeading = value;
    }

    void setSubHeading(const std::string& value) {
        m_subHeading = value;
    }

    const std::string& title() const {
        return m_title;
    }

    static PolledData getPolledData(bool forceRefresh = false);

protected:
    std::string m_title{};
    std::string m_titleSubHeading{};
    std::string m_subHeading{};
    ScrollingText m_titleScroll{};
    ScrollingText m_subScroll{};
    unsigned int m_flags = 0;
};

} // namespace ui::saves
