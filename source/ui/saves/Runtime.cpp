#include "ui/saves/Runtime.hpp"

#include "ui/saves/Object.hpp"
#include "utils/Logger.hpp"

namespace ui::saves {

Runtime& Runtime::instance() {
    static Runtime runtime;
    return runtime;
}

Runtime::Runtime() {
    m_theme.colors[ThemeEntryID_BACKGROUND] = {15, 23, 42, 255};
    m_theme.colors[ThemeEntryID_GRID] = {30, 41, 68, 255};
    m_theme.colors[ThemeEntryID_POPUP] = {21, 32, 58, 245};
    m_theme.colors[ThemeEntryID_ERROR] = {248, 113, 113, 255};
    m_theme.colors[ThemeEntryID_TEXT] = {241, 245, 249, 255};
    m_theme.colors[ThemeEntryID_TEXT_INFO] = {148, 163, 184, 255};
    m_theme.colors[ThemeEntryID_TEXT_SELECTED] = {255, 255, 255, 255};
    m_theme.colors[ThemeEntryID_SELECTED_BACKGROUND] = {56, 189, 248, 40};
    m_theme.colors[ThemeEntryID_FOCUS] = {56, 189, 248, 255};
    m_theme.colors[ThemeEntryID_LINE] = {51, 65, 85, 255};
    m_theme.colors[ThemeEntryID_LINE_SEPARATOR] = {51, 65, 85, 255};
    m_theme.colors[ThemeEntryID_SIDEBAR] = {17, 24, 39, 255};
    m_theme.colors[ThemeEntryID_SCROLLBAR] = {56, 189, 248, 255};
    m_theme.colors[ThemeEntryID_SCROLLBAR_BACKGROUND] = {30, 41, 59, 255};
    m_theme.colors[ThemeEntryID_PROGRESSBAR] = {56, 189, 248, 255};
    m_theme.colors[ThemeEntryID_PROGRESSBAR_BACKGROUND] = {30, 41, 59, 255};
    m_theme.colors[ThemeEntryID_HIGHLIGHT_1] = {34, 197, 94, 255};
    m_theme.colors[ThemeEntryID_HIGHLIGHT_2] = {14, 165, 233, 255};
    m_theme.colors[ThemeEntryID_ICON_COLOUR] = {255, 255, 255, 255};
}

void Runtime::push(std::shared_ptr<Object> object) {
    if (object) {
        m_stack.push_back(std::move(object));
    }
}

std::shared_ptr<Object> Runtime::current() const {
    if (m_stack.empty()) {
        return {};
    }
    return m_stack.back();
}

void Runtime::popToMenu() {
    if (!m_stack.empty()) {
        m_stack.resize(1);
    }
}

void Runtime::pop() {
    if (!m_stack.empty()) {
        m_stack.pop_back();
    }
}

void Runtime::notify(const std::string& text) {
    LOG_INFO("ui runtime notify: %s", text.c_str());
    m_lastNotification = text;
}

void Runtime::pushError(const std::string& text) {
    LOG_ERROR("ui runtime error: %s", text.c_str());
    m_lastNotification = "Error: " + text;
}

std::string Runtime::consumeNotification() {
    std::string text = std::move(m_lastNotification);
    m_lastNotification.clear();
    return text;
}

void Runtime::playSound(SoundEffect effect) {
    (void)effect;
    // The lightweight GUI does not use runtime sound assets.
}

const Theme& Runtime::theme() const {
    return m_theme;
}

Theme& Runtime::theme() {
    return m_theme;
}

std::vector<AccountProfileBase> Runtime::getAccountList() const {
    return {};
}

} // namespace ui::saves
