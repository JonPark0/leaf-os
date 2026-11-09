#include "page_app.h"

namespace apps {

void PageApp::setupPage(const AppContext& ctx, const std::string& title, TitleStyle style)
{
    App::onCreate(ctx);
    screen_.setBackground(core::kWhite);

    makeBackButton(back_, ctx);

    has_title_ = style != TitleStyle::None;
    title_.setFont(ctx.ui_font);
    title_.setText(title);
    switch (style) {
        case TitleStyle::Centered:
            title_.setBounds(ui::metrics::titleCentered());
            title_.setAlign(ui::Label::Align::Center);
            break;
        case TitleStyle::CenteredWide:
            title_.setBounds(ui::metrics::titleCentered(240));
            title_.setAlign(ui::Label::Align::Center);
            break;
        case TitleStyle::BesideBack:
            title_.setBounds(ui::metrics::titleBesideBack());
            title_.setAlign(ui::Label::Align::Left);
            break;
        case TitleStyle::None:
            break;
    }
    addHeader();
}

void PageApp::addHeader()
{
    screen_.add(&back_);
    if (has_title_) screen_.add(&title_);
}

}  // namespace apps
