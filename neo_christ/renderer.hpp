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
    char32_t glyph = 0; // Quadrants for wood, Braille for filigree, kana for rain.
    bool operator==(const Cell& v) const {
        return top == v.top && bottom == v.bottom && ink == v.ink && glyph == v.glyph;
    }
};
enum class Theme { gold, matrix, ivory };
enum class Style { gothic, calvary, ornate };
struct Settings {
    Style style = Style::gothic;
    Theme theme = Theme::gold;
    double period = 48.0;
    double aspect = 0.5; // Terminal cell width / height.
    bool rain = false;
    bool unicode = true;
    bool caption = true;
    int samples = 2; // Samples per axis, per quadrant pixel or Braille dot.
};
struct Frame {
    int columns, rows;
    std::vector<Cell> cells;
    Style style = Style::calvary;
};
Frame render(int columns, int rows, double seconds, const Settings& settings);
std::string utf8(char32_t codepoint);
std::string svg(const Frame& frame, double aspect);
std::string ansi(const Frame& frame, const Frame* previous, int colors);
}
