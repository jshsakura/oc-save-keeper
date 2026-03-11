#pragma once

#include "ui/saves/List.hpp"
#include "ui/saves/MenuBase.hpp"

#include <memory>

namespace ui::saves {

enum LayoutType {
    LayoutTypeList,
    LayoutTypeGrid,
    LayoutTypeGridDetail,
};

class GridMenuBase : public MenuBase {
public:
    using MenuBase::MenuBase;

protected:
    void onLayoutChange(std::unique_ptr<List>& list, int layout);
    Vec4 drawEntryNoImage(int layout, const Vec4& rect, bool selected, const char* name, const char* author, const char* version);
    void drawEntry(int layout, const Vec4& rect, bool selected, int image, const char* name, const char* author, const char* version);

private:
    Vec4 drawEntryInternal(bool drawImage, int layout, const Vec4& rect, bool selected, int image, const char* name, const char* author, const char* version);

    ScrollingText m_nameScroll{};
    ScrollingText m_authorScroll{};
    ScrollingText m_versionScroll{};
};

} // namespace ui::saves
