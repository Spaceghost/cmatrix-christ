// SPDX-License-Identifier: GPL-3.0-or-later
// Original terminal renderer inspired by st3w/neo and cmatrix-christ.
#include "renderer.hpp"
#include "ornament.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace christ {
namespace {
using ornament::V;
using ornament::dot;
using ornament::unit;
using ornament::Material;
constexpr double pi=ornament::pi;
double clamp(double x) { return std::clamp(x,0.0,1.0); }
uint32_t hash(uint32_t x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);
}
double random(uint32_t x) { return hash(x)/4294967295.0; }
struct Rotation {
    double sy,cy,sx,cx;
    Rotation(double t,double period) {
        double yaw=0.22+t*2*pi/period,pitch=-0.10+0.025*std::sin(t*2*pi/period);
        sy=std::sin(yaw);cy=std::cos(yaw);sx=std::sin(pitch);cx=std::cos(pitch);
    }
    V world(V p) const {
        V q{cy*p.x+sy*p.z,p.y,-sy*p.x+cy*p.z};
        return {q.x,cx*q.y-sx*q.z,sx*q.y+cx*q.z};
    }
};
RGB encode(V c) {
    auto channel=[](double v){return static_cast<int>(std::lround(255*std::pow(1-std::exp(-std::max(0.0,v)),1/2.2)));};
    return {channel(c.x),channel(c.y),channel(c.z)};
}
V hue(Theme theme) {
    if(theme==Theme::matrix)return {0.26,0.95,0.49};
    if(theme==Theme::ivory)return {0.92,0.91,0.79};
    return {1.0,0.58,0.19};
}
V shade(V n,Material material,Theme theme) {
    static const V key=unit({-0.65,0.85,1.4}),fill=unit({0.9,0.1,0.4});
    static const V half=unit(key+V{0,0,1});
    if(material==Material::ruby) {
        // Facet normals give small stones a crisp, stable glint as they rotate.
        n=unit({std::round(n.x*5),std::round(n.y*5),std::round(n.z*5)});
    }
    double diffuse=std::max(0.0,dot(n,key));
    double shine=std::pow(std::max(0.0,dot(n,half)),28);
    double bounce=std::max(0.0,dot(n,fill));
    if(material==Material::ruby) {
        V jewel=theme==Theme::matrix?V{0.04,0.40,0.24}:V{0.72,0.012,0.045};
        return jewel*(0.22+0.65*diffuse)+V{1,0.82,0.70}*(1.8*shine);
    }
    if(material==Material::pearl)return V{1,0.90,0.67}*(0.34+0.6*diffuse+shine);
    V metal=hue(theme)*(0.25+0.85*diffuse+0.15*bounce)+V{1,0.91,0.70}*(1.7*shine);
    return metal*(material==Material::halo?0.22:1.0);
}
struct Pixel { float depth=-100; V light; };
struct Canvas {
    int width,height;
    double sx,sy;
    std::vector<Pixel> pixels;
    Canvas(int cols,int rows,int aa,double aspect):width(cols*2*aa),height(rows*4*aa),
        sx(0),sy(std::min(height/3.85,width/(2*aspect)/3.05)),
        pixels(static_cast<size_t>(width)*height) {sx=sy/(2*aspect);}

    // Analytic orthographic ray/capsule intersections. Only a wire's projected
    // bounds are visited, so intricate openwork does not require a full-screen
    // ray marcher. The depth buffer keeps scrolls, pearls and rear wires ordered.
    void wire(V a,V b,double radius,Material material,Theme theme) {
        int x0=std::max(0,static_cast<int>(std::floor(width/2.0+(std::min(a.x,b.x)-radius)*sx)));
        int x1=std::min(width-1,static_cast<int>(std::ceil(width/2.0+(std::max(a.x,b.x)+radius)*sx)));
        int y0=std::max(0,static_cast<int>(std::floor(height/2.0-(std::max(a.y,b.y)+radius)*sy)));
        int y1=std::min(height-1,static_cast<int>(std::ceil(height/2.0-(std::min(a.y,b.y)-radius)*sy)));
        V ba=b-a;
        double baba=dot(ba,ba),bard=-ba.z,A=ba.x*ba.x+ba.y*ba.y;
        for(int py=y0;py<=y1;++py)for(int px=x0;px<=x1;++px) {
            double x=(px+0.5-width/2.0)/sx,y=(height/2.0-py-0.5)/sy;
            double z=-100;
            // Spherical caps also handle beads and view-parallel segments.
            for(V end:{a,b}) {
                double dx=x-end.x,dy=y-end.y,h=radius*radius-dx*dx-dy*dy;
                if(h>=0)z=std::max(z,end.z+std::sqrt(h));
            }
            if(A>1e-14) {
                V oa=V{x,y,4}-a;
                double baoa=dot(ba,oa),rdoa=-oa.z;
                double B=baba*rdoa-baoa*bard;
                double C=baba*dot(oa,oa)-baoa*baoa-radius*radius*baba;
                double h=B*B-A*C;
                if(h>=0) {
                    double t=(-B-std::sqrt(h))/A;
                    double along=baoa+t*bard;
                    if(along>=0 && along<=baba)z=std::max(z,4-t);
                }
            }
            Pixel& pixel=pixels[static_cast<size_t>(py)*width+px];
            if(z<=pixel.depth)continue;
            V surface{x,y,z};
            double u=baba>1e-14?clamp(dot(surface-a,ba)/baba):0;
            pixel.depth=static_cast<float>(z);
            pixel.light=shade(unit(surface-(a+ba*u)),material,theme);
        }
    }
};
char32_t rainGlyph(uint32_t seed,bool unicode) {
    static const char ascii[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:+=<>";
    return unicode?0xff66+seed%56:ascii[seed%(sizeof(ascii)-1)];
}
std::string color(RGB c) {
    char out[8];std::snprintf(out,sizeof(out),"#%02x%02x%02x",c.r,c.g,c.b);return out;
}
int index256(RGB c) {
    auto cube=[](int x){return x<48?0:x<115?1:std::min(5,(x-35)/40);};
    int r=cube(c.r),g=cube(c.g),b=cube(c.b);
    const int values[]={0,95,135,175,215,255};
    auto sq=[](int v){return v*v;};
    int error=sq(c.r-values[r])+sq(c.g-values[g])+sq(c.b-values[b]);
    int grey=std::clamp(((c.r+c.g+c.b)/3-8+5)/10,0,23),v=8+10*grey;
    return sq(c.r-v)+sq(c.g-v)+sq(c.b-v)<error?232+grey:16+36*r+6*g+b;
}
void sgr(std::string& out,RGB c,bool background,int colors) {
    if(colors==0)return;
    char b[64];
    if(colors==256)std::snprintf(b,sizeof(b),"\033[%d;5;%dm",background?48:38,index256(c));
    else std::snprintf(b,sizeof(b),"\033[%d;2;%d;%d;%dm",background?48:38,c.r,c.g,c.b);
    out+=b;
}
}

Frame render(int columns,int rows,double time,const Settings& s) {
    if(columns<1 || rows<1)return {0,0,{}};
    Frame frame{columns,rows,std::vector<Cell>(static_cast<size_t>(columns)*rows)};
    int aa=std::clamp(s.samples,1,2);
    Canvas canvas(columns,rows,aa,s.aspect);
    static const auto cross=ornament::cross(),halo=ornament::nimbus();
    Rotation rotation(time,s.period);
    for(const auto& wire:halo.wires)canvas.wire(wire.a,wire.b,wire.radius,wire.material,s.theme);
    for(const auto& wire:cross.wires)canvas.wire(rotation.world(wire.a),rotation.world(wire.b),wire.radius,wire.material,s.theme);
    std::vector<bool> occupied(frame.cells.size(),false);
    // Unicode Braille supplies eight addressable dots in each terminal cell.
    // Its open spaces are deliberate: no opaque rectangle fills the metalwork.
    constexpr int bits[4][2]={{0,3},{1,4},{2,5},{6,7}};
    for(int row=0;row<rows;++row)for(int col=0;col<columns;++col) {
        size_t i=static_cast<size_t>(row)*columns+col;
        Cell& cell=frame.cells[i];
        double x=(col*2*aa+aa-canvas.width/2.0)/canvas.sx;
        double y=(canvas.height/2.0-row*4*aa-2*aa)/canvas.sy;
        double glow=0.002*std::exp(-(x*x+(y-0.35)*(y-0.35))*1.8);
        cell.top=cell.bottom=encode(V{0.00035,0.00065,0.0007}+hue(s.theme)*glow);
        unsigned mask=0;
        V light;
        double weight=0;
        int dots=0;
        for(int dy=0;dy<4;++dy)for(int dx=0;dx<2;++dx) {
            int covered=0;V sum;
            for(int ay=0;ay<aa;++ay)for(int ax=0;ax<aa;++ax) {
                const Pixel& pixel=canvas.pixels[static_cast<size_t>((row*4+dy)*aa+ay)*canvas.width+(col*2+dx)*aa+ax];
                if(pixel.depth>-99) {++covered;sum=sum+pixel.light;}
            }
            if(covered==0)continue;
            mask|=1u<<bits[dy][dx];++dots;
            double coverage=static_cast<double>(covered)/(aa*aa);
            light=light+sum*(std::sqrt(coverage)/covered);
            weight+=1;
        }
        if(mask) {
            occupied[i]=true;
            cell.ink=encode(light*(1/weight));
            cell.glyph=s.unicode?0x2800+mask:" .:+oxO@#"[dots];
        } else cell.glyph=' ';
    }
    if(s.rain)for(int col=0;col<columns;++col) {
        uint32_t seed=hash(static_cast<uint32_t>(col)+39u);
        if(seed%4==0)continue;
        double speed=1.4+random(seed)*3.2,span=rows+14.0;
        double head=std::fmod(time*speed+random(seed+1)*span,span);
        double tail=5+random(seed+2)*rows*0.45;
        for(int row=0;row<rows;++row) {
            size_t i=static_cast<size_t>(row)*columns+col;
            if(occupied[i])continue;
            double age=head-row;
            if(age<0 || age>tail)continue;
            double x=(col+0.5-columns/2.0)/(canvas.sx/(2*aa));
            double y=(rows/2.0-row-0.5)/(canvas.sy/(4*aa));
            double quiet=0.20+0.80*clamp((std::hypot(x,y-0.35)-0.4)/1.5);
            double energy=std::pow(1-age/tail,1.7)*quiet;
            Cell& cell=frame.cells[i];
            cell.glyph=rainGlyph(hash(seed+static_cast<uint32_t>(row)*97u+static_cast<uint32_t>(time*1.5)),s.unicode);
            V rain=s.theme==Theme::ivory?V{0.19,0.29,0.31}:V{0.055,0.36,0.16};
            cell.ink=encode(rain*(0.010+0.10*energy));
            if(age<0.8)cell.ink=encode(V{0.40,0.66,0.48}*(0.06*quiet));
        }
    }
    if(s.caption && columns>=54 && rows>=18) {
        auto label=[&](int row,const std::string& text,RGB ink) {
            int left=(columns-static_cast<int>(text.size()))/2;
            for(size_t k=0;k<text.size();++k) {
                Cell& c=frame.cells[static_cast<size_t>(row)*columns+left+k];
                c.top=c.bottom={3,6,7};c.ink=ink;c.glyph=text[k];
            }
        };
        label(rows-2,"L U X   C R U C I S",{148,127,84});
        label(rows-1,"space pause   +/- speed   t palette   r rain   q quit",{56,75,70});
    }
    return frame;
}

std::string utf8(char32_t c) {
    std::string s;
    if (c<0x80) s+=static_cast<char>(c);
    else if (c<0x800) {s+=static_cast<char>(0xc0|(c>>6));s+=static_cast<char>(0x80|(c&63));}
    else if (c<0x10000) {s+=static_cast<char>(0xe0|(c>>12));s+=static_cast<char>(0x80|((c>>6)&63));s+=static_cast<char>(0x80|(c&63));}
    else {s+=static_cast<char>(0xf0|(c>>18));s+=static_cast<char>(0x80|((c>>12)&63));s+=static_cast<char>(0x80|((c>>6)&63));s+=static_cast<char>(0x80|(c&63));}
    return s;
}

std::string ansi(const Frame& frame, const Frame* previous, int colors) {
    std::string out;
    out.reserve(frame.cells.size()*30);
    RGB lastfg{-1,-1,-1},lastbg{-1,-1,-1};
    for (int row=0;row<frame.rows;++row) {
        bool continuous=false;
        for (int col=0;col<frame.columns;++col) {
            size_t i=static_cast<size_t>(row)*frame.columns+col;
            const Cell& c=frame.cells[i];
            if (previous && previous->columns==frame.columns && previous->rows==frame.rows && c==previous->cells[i]) {
                continuous=false; continue;
            }
            if (!continuous) {out+="\033["+std::to_string(row+1)+";"+std::to_string(col+1)+"H";continuous=true;}
            RGB fg=c.glyph?c.ink:c.top, bg=c.bottom;
            if (!(fg==lastfg)) {sgr(out,fg,false,colors);lastfg=fg;}
            if (!(bg==lastbg)) {sgr(out,bg,true,colors);lastbg=bg;}
            out+=utf8(c.glyph?c.glyph:U'▀');
        }
    }
    out+="\033[0m";
    return out;
}

std::string svg(const Frame& f, double aspect) {
    double w=16*aspect;
    std::ostringstream out;
    out<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""<<f.columns*w<<"\" height=\""<<f.rows*16
       <<"\" viewBox=\"0 0 "<<f.columns*w<<' '<<f.rows*16<<"\"><title>Lux Crucis — rotating filigree cross</title>"
       <<"<rect width=\"100%\" height=\"100%\" fill=\"#030607\"/>\n";
    for (int y=0;y<f.rows;++y) for (int x=0;x<f.columns;++x) {
        const Cell& c=f.cells[static_cast<size_t>(y)*f.columns+x];
        auto rect=[&](double top,int h,RGB rgb){out<<"<rect x=\""<<x*w<<"\" y=\""<<top<<"\" width=\""<<w<<"\" height=\""<<h<<"\" fill=\""<<color(rgb)<<"\"/>\n";};
        if (!c.glyph) {rect(y*16,8,c.top);rect(y*16+8,8,c.bottom);}
        else {
            rect(y*16,16,c.bottom);
            if(c.glyph>=0x2800 && c.glyph<=0x28ff) {
                // Export the actual dot mask without relying on a viewer's
                // font fallback; the live terminal draws the same Braille cell.
                constexpr int bits[4][2]={{0,3},{1,4},{2,5},{6,7}};
                unsigned mask=static_cast<unsigned>(c.glyph-0x2800);
                for(int dy=0;dy<4;++dy)for(int dx=0;dx<2;++dx) {
                    if(mask&(1u<<bits[dy][dx]))
                        out<<"<circle cx=\""<<(x+(dx+0.5)/2)*w<<"\" cy=\""<<y*16+dy*4+2
                           <<"\" r=\""<<std::min(w/2,4.0)*0.24<<"\" fill=\""<<color(c.ink)<<"\"/>\n";
                }
                continue;
            }
            std::string glyph=utf8(c.glyph);
            if(c.glyph==U'&')glyph="&amp;";
            if(c.glyph==U'<')glyph="&lt;";
            if(c.glyph==U'>')glyph="&gt;";
            out<<"<text x=\""<<(x+0.5)*w<<"\" y=\""<<y*16+13<<"\" text-anchor=\"middle\" font-family=\"IPAGothic,Noto Sans Mono CJK JP,monospace\" font-size=\"13\" fill=\""<<color(c.ink)<<"\">"<<glyph<<"</text>\n";
        }
    }
    out<<"</svg>\n";
    return out.str();
}
}
