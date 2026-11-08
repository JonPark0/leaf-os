#pragma once

#include <string>
#include <vector>

#include "page_app.h"
#include "app_common.h"
#include "widget.h"
#include "widgets/button.h"
#include "widgets/label.h"
#include "widgets/text_view.h"

namespace apps {

// "정보" page: version, hardware, and third-party / open-source attributions.
// Centred, wrapped text, paged when it runs past the screen.
class AboutApp : public PageApp {
public:
    AboutApp() : PageApp("about") {}
    void onCreate(const AppContext& ctx) override;

private:
    ui::TextView body_;
};

}  // namespace apps
