#pragma once

#include <functional>
#include <string>
#include <variant>

#ifdef __SWITCH__
#include <switch.h>
#else
using u8 = unsigned char;
using u32 = unsigned int;
using u64 = unsigned long long;
struct AccountUid {
    u64 uid[2]{};
};
struct AccountProfileBase {
    AccountUid uid{};
    char nickname[33]{};
};
#endif

namespace ui::saves {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

enum ThemeEntryID {
    ThemeEntryID_BACKGROUND,
    ThemeEntryID_GRID,
    ThemeEntryID_POPUP,
    ThemeEntryID_ERROR,
    ThemeEntryID_TEXT,
    ThemeEntryID_TEXT_INFO,
    ThemeEntryID_TEXT_SELECTED,
    ThemeEntryID_SELECTED_BACKGROUND,
    ThemeEntryID_FOCUS,
    ThemeEntryID_LINE,
    ThemeEntryID_LINE_SEPARATOR,
    ThemeEntryID_SIDEBAR,
    ThemeEntryID_SCROLLBAR,
    ThemeEntryID_SCROLLBAR_BACKGROUND,
    ThemeEntryID_PROGRESSBAR,
    ThemeEntryID_PROGRESSBAR_BACKGROUND,
    ThemeEntryID_HIGHLIGHT_1,
    ThemeEntryID_HIGHLIGHT_2,
    ThemeEntryID_ICON_COLOUR,
    ThemeEntryID_ICON_AUDIO,
    ThemeEntryID_ICON_VIDEO,
    ThemeEntryID_ICON_IMAGE,
    ThemeEntryID_ICON_FILE,
    ThemeEntryID_ICON_FOLDER,
    ThemeEntryID_ICON_ZIP,
    ThemeEntryID_ICON_NRO,
    ThemeEntryID_MAX,
};

struct ThemeColor {
    u8 r = 255;
    u8 g = 255;
    u8 b = 255;
    u8 a = 255;
};

struct Theme {
    ThemeColor colors[ThemeEntryID_MAX]{};

    ThemeColor getColor(ThemeEntryID id) const {
        return colors[id];
    }
};

enum class Button : u64 {
    None = 0,
    A = 1ULL << 0,
    B = 1ULL << 1,
    X = 1ULL << 2,
    Y = 1ULL << 3,
    L = 1ULL << 4,
    R = 1ULL << 5,
    L2 = 1ULL << 6,
    R2 = 1ULL << 7,
    L3 = 1ULL << 8,
    R3 = 1ULL << 9,
    Start = 1ULL << 10,
    Select = 1ULL << 11,
    Left = 1ULL << 12,
    Right = 1ULL << 13,
    Up = 1ULL << 14,
    Down = 1ULL << 15,
    AnyButton = (1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3) |
                (1ULL << 4) | (1ULL << 5) | (1ULL << 6) | (1ULL << 7) |
                (1ULL << 8) | (1ULL << 9) | (1ULL << 10) | (1ULL << 11),
};

inline Button operator|(Button lhs, Button rhs) {
    return static_cast<Button>(static_cast<u64>(lhs) | static_cast<u64>(rhs));
}

enum ActionType : u8 {
    ActionTypeDown = 1 << 0,
    ActionTypeUp = 1 << 1,
    ActionTypeHeld = 1 << 2,
};

struct Action {
    using Callback = std::variant<std::function<void()>, std::function<void(bool)>>;

    u8 type = ActionTypeDown;
    std::string hint;
    Callback callback = std::function<void()>{};

    Action() = default;
    explicit Action(const std::function<void()>& cb) : callback(cb) {}
    Action(const std::string& actionHint, const std::function<void()>& cb)
        : hint(actionHint), callback(cb) {}
};

struct Controller {
    u64 down = 0;
    u64 held = 0;
    u64 up = 0;

    bool gotDown(Button button) const {
        return (down & static_cast<u64>(button)) != 0;
    }

    bool gotHeld(Button button) const {
        return (held & static_cast<u64>(button)) != 0;
    }

    bool gotUp(Button button) const {
        return (up & static_cast<u64>(button)) != 0;
    }
};

struct TouchInfo {
    int x = 0;
    int y = 0;
    bool isTouching = false;
    bool isTap = false;
    bool isClicked = false;

    bool inRange(const Vec4& rect) const {
        return x >= rect.x && x <= rect.x + rect.w &&
               y >= rect.y && y <= rect.y + rect.h;
    }
};

enum class SoundEffect {
    Focus,
    Error,
};

} // namespace ui::saves
