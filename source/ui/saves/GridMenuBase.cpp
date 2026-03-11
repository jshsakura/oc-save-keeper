#include "ui/saves/GridMenuBase.hpp"

namespace ui::saves {

void GridMenuBase::onLayoutChange(std::unique_ptr<List>& list, int layout) {
    m_nameScroll.reset();
    m_authorScroll.reset();
    m_versionScroll.reset();

    switch (layout) {
        case LayoutTypeList:
            list = std::make_unique<List>(1, 4, Vec4{0, 0, 1280, 720}, Vec4{106, 194, 256, 256}, Vec2{14, 14});
            list->setLayout(List::Layout::Home);
            break;
        case LayoutTypeGrid:
            list = std::make_unique<List>(6, 12, Vec4{0, 0, 1280, 720}, Vec4{93, 186, 174, 174}, Vec2{10, 10});
            list->setLayout(List::Layout::Grid);
            break;
        case LayoutTypeGridDetail:
        default:
            list = std::make_unique<List>(3, 9, Vec4{0, 0, 1280, 720}, Vec4{75, 110, 370, 155}, Vec2{10, 10});
            list->setLayout(List::Layout::Grid);
            break;
    }
}

void GridMenuBase::drawEntry(int layout, const Vec4& rect, bool selected, int image, const char* name, const char* author, const char* version) {
    (void)drawEntryInternal(true, layout, rect, selected, image, name, author, version);
}

Vec4 GridMenuBase::drawEntryNoImage(int layout, const Vec4& rect, bool selected, const char* name, const char* author, const char* version) {
    return drawEntryInternal(false, layout, rect, selected, 0, name, author, version);
}

Vec4 GridMenuBase::drawEntryInternal(bool drawImage, int layout, const Vec4& rect, bool selected, int image, const char* name, const char* author, const char* version) {
    (void)layout;
    (void)selected;
    (void)image;
    (void)drawImage;
    m_nameScroll.reset(name ? name : "");
    m_authorScroll.reset(author ? author : "");
    m_versionScroll.reset(version ? version : "");
    return rect;
}

} // namespace ui::saves
