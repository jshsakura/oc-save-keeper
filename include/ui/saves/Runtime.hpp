#pragma once

#include "ui/saves/Types.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ui::saves {

class Object;

class Runtime {
public:
    static Runtime& instance();

    void push(std::shared_ptr<Object> object);
    std::shared_ptr<Object> current() const;
    void popToMenu();
    void pop();

    void notify(const std::string& text);
    void pushError(const std::string& text);
    void playSound(SoundEffect effect);

    const Theme& theme() const;
    Theme& theme();

    std::vector<AccountProfileBase> getAccountList() const;

private:
    Runtime();

    Theme m_theme{};
    std::vector<std::shared_ptr<Object>> m_stack{};
};

} // namespace ui::saves
