// SPDX-License-Identifier: GPL-3.0-or-later
#include "renderer.hpp"
#include "gothic.hpp"
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
            bool visible=f.style==christ::Style::calvary?
                std::max(cell.ink.r,cell.bottom.r)>80:
                cell.glyph>=0x2801 && cell.glyph<=0x28ff && cell.ink.r>185 && cell.ink.g>135;
            if(visible) {
                left=std::min(left,x);right=x;
            }
        }
        widest=std::max(widest,right-left+1);
    }
    return widest;
}
int main() {
    christ::Settings s;
    s.style=christ::Style::ornate;
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
    christ::Settings timber;
    timber.style=christ::Style::calvary;
    timber.caption=false;
    auto wood=christ::render(120,45,0,timber);
    auto woodAgain=christ::render(120,45,0,timber);
    check(wood.cells==woodAgain.cells,"timber material must render deterministically");
    auto largeWood=christ::render(163,57,4,timber);
    check(largeWood.cells==christ::render(163,57,4,timber).cells,
          "parallel rendering must be deterministic across uneven row strips");
    auto woodSide=christ::render(120,45,timber.period/4,timber);
    check(silhouetteWidth(wood)>silhouetteWidth(woodSide)*2,"timber must be real 3D geometry");
    auto revolution=christ::render(120,45,timber.period,timber);
    int changed=0;
    for(size_t i=0;i<wood.cells.size();++i)if(!(wood.cells[i]==revolution.cells[i]))++changed;
    check(changed<6,"grain must stay attached to the wood through a full revolution");
    auto nextWood=christ::render(120,45,1.0/60,timber);
    long woodChange=0;
    for(size_t i=0;i<wood.cells.size();++i) {
        woodChange+=std::abs(wood.cells[i].ink.r-nextWood.cells[i].ink.r);
        woodChange+=std::abs(wood.cells[i].bottom.r-nextWood.cells[i].bottom.r);
    }
    check(woodChange<static_cast<long>(wood.cells.size())*8,"timber motion must remain continuous");
    check(std::none_of(wood.cells.begin(),wood.cells.end(),[](const auto& c){return c.glyph>=0x2801 && c.glyph<=0x28ff;}),"Calvary must not inherit ornamental dotwork");
    timber.unicode=false;
    auto woodAscii=christ::render(80,24,0,timber);
    check(std::all_of(woodAscii.cells.begin(),woodAscii.cells.end(),[](const auto& c){return c.glyph>0 && c.glyph<128;}),"timber ASCII fallback must contain only ASCII glyphs");
    christ::Settings devotional;
    devotional.caption=false;
    auto gothic=christ::render(163,57,0,devotional);
    check(gothic.cells==christ::render(163,57,0,devotional).cells,
          "Gothic carving and candlelight must be deterministic across worker rows");
    auto gothicLoop=christ::render(163,57,devotional.period,devotional);
    int loopChange=0;
    for(size_t i=0;i<gothic.cells.size();++i)if(!(gothic.cells[i]==gothicLoop.cells[i]))++loopChange;
    check(loopChange<8,"carving, candlelight and motion must close the full loop");
    auto gothicNext=christ::render(163,57,1.0/60,devotional);
    long gothicChange=0;
    for(size_t i=0;i<gothic.cells.size();++i) {
        gothicChange+=std::abs(gothic.cells[i].ink.r-gothicNext.cells[i].ink.r);
        gothicChange+=std::abs(gothic.cells[i].bottom.r-gothicNext.cells[i].bottom.r);
    }
    check(gothicChange<static_cast<long>(gothic.cells.size())*8,"Gothic motion must remain continuous");
    christ::gothic::Scene sculpture(0,devotional.period);
    christ::gothic::Rotation pose(0,devotional.period);
    for(auto hole:{christ::ornament::V{0,-0.23,0}, {0,-0.97,0}, {0,0.99,0}, {-0.65,0.52,0}, {0.65,0.52,0}}) {
        auto world=pose.world(hole);
        check(!sculpture.sample(world.x,world.y).solid,"Gothic lancets must be actual openings through the metal");
    }
    auto center=pose.world({0,0.52,0});
    check(sculpture.sample(center.x,center.y).solid,"central rosette must have solid depth");
    devotional.unicode=false;
    auto gothicAscii=christ::render(80,24,0,devotional);
    check(std::all_of(gothicAscii.cells.begin(),gothicAscii.cells.end(),[](const auto& c){return c.glyph>0 && c.glyph<128;}),"Gothic ASCII fallback must contain only ASCII glyphs");
    std::cout<<"PASS: all styles, deterministic geometry, carved openings, complete loops, smooth motion, diff rendering and fallbacks\n";
}
