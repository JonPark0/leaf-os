/*
 * Shared integer rectangle type. UI coordinates are portrait: x in [0,480),
 * y in [0,800), origin top-left (matches the touch controller).
 */
#pragma once

#include <algorithm>

namespace core {

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    constexpr bool empty() const { return w <= 0 || h <= 0; }
    constexpr int right() const { return x + w; }
    constexpr int bottom() const { return y + h; }

    constexpr bool contains(int px, int py) const
    {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    // Smallest rect covering both (treats an empty rect as "nothing").
    Rect unite(const Rect& o) const
    {
        if (empty()) return o;
        if (o.empty()) return *this;
        const int nx = std::min(x, o.x);
        const int ny = std::min(y, o.y);
        return {nx, ny, std::max(right(), o.right()) - nx, std::max(bottom(), o.bottom()) - ny};
    }

    Rect intersect(const Rect& o) const
    {
        const int nx = std::max(x, o.x);
        const int ny = std::max(y, o.y);
        const int nr = std::min(right(), o.right());
        const int nb = std::min(bottom(), o.bottom());
        return {nx, ny, nr - nx, nb - ny};
    }

    Rect clampedTo(int cw, int ch) const
    {
        return intersect({0, 0, cw, ch});
    }
};

}  // namespace core
