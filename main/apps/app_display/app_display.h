#pragma once

#include <array>
#include <string>
#include <vector>

#include "app.h"
#include "app_common.h"
#include "font_scan.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"
#include "widgets/label.h"

namespace apps {

// "화면 / 글꼴" sub-page: −/+ steppers for front-light brightness plus the
// system (UI) and reader font faces and sizes, and a restore-defaults button.
// Brightness is applied live; font choices are saved to core::Settings — the
// reader font picks up on next open, the system font on next boot (a restart
// row then appears in the Settings list).
class DisplayApp : public App {
public:
    void onCreate(const AppContext& ctx) override;

    ui::Screen* screen() override { return &screen_; }
    const AppMeta& meta() const override { return meta_; }

private:
    enum Row { kBright, kUiFamily, kUiSize, kReadFamily, kReadSize, kRowCount };

    void bump(int row, int delta);
    void refresh();
    void applyDefaults();
    void rescan();   // reload sd_fonts_ + per-script family list

    AppMeta meta_{"display", "화면 / 글꼴"};
    ui::Screen screen_;
    ui::Button back_;
    ui::Label  title_;

    std::array<ui::Label, kRowCount>  name_;
    std::array<ui::Label, kRowCount>  value_;
    std::array<ui::Button, kRowCount> minus_;
    std::array<ui::Button, kRowCount> plus_;
    ui::Label    pad_name_;
    ui::Checkbox pad_;
    ui::Button   defaults_;
    ui::Label    note_;

    std::vector<gfx::SdFont> sd_fonts_;
    std::vector<std::string> families_;   // canonical faces present for the script
    const char* script_ = "ko";
};

}  // namespace apps
