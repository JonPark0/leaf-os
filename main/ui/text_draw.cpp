#include "text_draw.h"

namespace ui::text {

void drawEllipsized(core::Framebuffer& fb, const gfx::Font& f, int x, int baseline,
                    const std::string& s, int avail, uint8_t ink)
{
    if (avail <= 0 || s.empty()) return;
    const Measure m = measureWith(f);
    if (f.measure(s.c_str()) <= avail) {
        f.draw(fb, x, baseline, s.c_str(), ink);
        return;
    }
    const char* ell = "...";
    const int ew = f.measure(ell);
    const size_t n = fitPrefix(s, avail, ew, m);
    f.draw(fb, x, baseline, s.c_str(), ink, static_cast<int>(n));
    f.draw(fb, x + f.measure(s.c_str(), static_cast<int>(n)), baseline, ell, ink);
}

}  // namespace ui::text
