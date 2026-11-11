// SPDX-License-Identifier: GPL-3.0-or-later
#include "renderer.hpp"
#include <algorithm>
#include <cmath>
#include <bitset>
#include <iostream>
#include <stdexcept>

void check(bool good, const char* message) { if(!good)throw std::runtime_error(message); }
int silhouetteWidth(const christ::Frame& f) {
    int widest=0;
    for(int y=0;y<f.rows;++y) {
        int left=f.columns,right=-1;
        for(int x=0;x<f.columns;++x) {
            const auto& cell=f.cells[static_cast<size_t>(y)*f.columns+x];
            if(cell.glyph>=0x2801 && cell.glyph<=0x28ff && cell.ink.r>185 && cell.ink.g>135) {
                left=std::min(left,x);right=x;
            }
        }
        widest=std::max(widest,right-left+1);
    }
    return widest;
}
int main() {
    christ::Settings s;
    s.rain=false;s.caption=false;
    auto front=christ::render(120,45,0,s);
    auto same=christ::render(120,45,0,s);
    check(front.cells==same.cells,"rendering must be deterministic");
    auto edge=christ::render(120,45,s.period/4,s);
    check(silhouetteWidth(front)>silhouetteWidth(edge)*2,"rotation must have depth and foreshortening");
    auto next=christ::render(120,45,1.0/60,s);
    long change=0,open=0,filled=0;
    for(size_t i=0;i<front.cells.size();++i) {
        const auto& a=front.cells[i];const auto& b=next.cells[i];
        if(a.glyph>=0x2801 && a.glyph<=0x28ff) {
            int dots=static_cast<int>(std::bitset<8>(a.glyph-0x2800).count());
            filled+=dots;open+=8-dots;
        }
        change+=std::abs(a.ink.r-b.ink.r);
        if(a.glyph!=b.glyph)change+=16;
    }
    check(change<static_cast<long>(front.cells.size())*8,"adjacent frames must move smoothly");
    check(filled>200 && open>filled/3,"filigree must retain open spaces rather than solid beams");
    check(christ::ansi(front,&same,16777216)=="\033[0m","unchanged frames must not redraw the screen");
    auto tiny=christ::render(1,1,2,s);
    check(tiny.cells.size()==1,"a minimized terminal must remain safe");
    s.rain=true;
    auto rain=christ::render(120,45,4,s);
    check(std::any_of(rain.cells.begin(),rain.cells.end(),[](const auto& c){return c.glyph>=0xff66 && c.glyph<=0xff9d;}),"rain must include half-width kana");
    s.unicode=false;
    auto ascii=christ::render(80,24,0,s);
    check(std::all_of(ascii.cells.begin(),ascii.cells.end(),[](const auto& c){return c.glyph>0 && c.glyph<128;}),"ASCII fallback must contain only ASCII glyphs");
    std::cout<<"PASS: deterministic geometry, 3D rotation, smooth motion, diff rendering, tiny sizes and glyph fallback\n";
}
