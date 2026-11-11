// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace christ {
struct RGB {
    int r = 0, g = 0, b = 0;
    bool operator==(const RGB& v) const { return r == v.r && g == v.g && b == v.b; }
};
struct Cell {
    RGB top, bottom, ink;
    char32_t glyph = 0; // Zero means two independently shaded half-block pixels.
    bool operator==(const Cell& v) const {
        return top == v.top && bottom == v.bottom && ink == v.ink && glyph == v.glyph;
    }
};
enum class Theme { gold, matrix, ivory };
struct Settings {
    Theme theme = Theme::gold;
    double period = 36.0;
    double aspect = 0.5; // Terminal cell width / height.
    bool rain = true;
    bool unicode = true;
    bool caption = true;
    int samples = 2; // Samples per axis, per half-cell pixel.
};
struct Frame {
    int columns, rows;
    std::vector<Cell> cells;
};
Frame render(int columns, int rows, double seconds, const Settings& settings);
std::string utf8(char32_t codepoint);
std::string svg(const Frame& frame, double aspect);
std::string ansi(const Frame& frame, const Frame* previous, int colors);
}
