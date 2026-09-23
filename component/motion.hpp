// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_COMPONENT_MOTION_HPP
#define ZENGINE_COMPONENT_MOTION_HPP

#include <algorithm>
#include <cmath>

namespace zengine::component::motion {
struct Point { double x = 0, y = 0; };
inline Point mix(Point a, Point b, double t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}
// Time is normalized independently of the geometry. Equal times traverse a linear path
// at equal speed; Bezier time is its parameter (not arc-length reparameterization).
inline double progress(double elapsed, double duration) {
    return duration > 0 ? std::clamp(elapsed / duration, 0.0, 1.0) : 1.0;
}
struct Path {
    Point start, control1, control2, end;
    Point at(double t) const {
        t = std::clamp(t, 0.0, 1.0);
        const auto a = mix(start, control1, t), b = mix(control1, control2, t);
        const auto c = mix(control2, end, t);
        return mix(mix(a, b, t), mix(b, c, t), t);
    }
    static Path linear(Point a, Point b) { return {a, mix(a,b,1.0/3), mix(a,b,2.0/3), b}; }
    static Path curved(Point a, Point b, double bend) {
        auto p = linear(a, b);
        const auto length = std::hypot(b.x-a.x, b.y-a.y);
        if (length > 0) {
            const Point offset{-(b.y-a.y)*bend/length, (b.x-a.x)*bend/length};
            p.control1.x += offset.x; p.control1.y += offset.y;
            p.control2.x += offset.x; p.control2.y += offset.y;
        }
        return p;
    }
};
} // namespace zengine::component::motion
#endif
