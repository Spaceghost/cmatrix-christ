// SPDX-License-Identifier: GPL-3.0-or-later
// Original renderer, inspired by st3w/neo's graded, asynchronous rain and
// Spaceghost/cmatrix-christ's code cross, gold edging and double nimbus.
#include "renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace christ {
namespace {
constexpr double pi = 3.14159265358979323846;
struct V {
    double x, y, z;
    V operator+(V b) const { return {x+b.x, y+b.y, z+b.z}; }
    V operator-(V b) const { return {x-b.x, y-b.y, z-b.z}; }
    V operator*(double s) const { return {x*s, y*s, z*s}; }
};
double dot(V a, V b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
double length(V a) { return std::sqrt(dot(a,a)); }
V unit(V a) { return a * (1.0 / length(a)); }
double clamp(double x) { return std::clamp(x, 0.0, 1.0); }
double fract(double x) { return x - std::floor(x); }
double smooth(double a, double b, double x) {
    x = clamp((x-a)/(b-a));
    return x*x*(3-2*x);
}
uint32_t hash(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
    x *= 0x846ca68bu; return x ^ (x >> 16);
}
double random(uint32_t x) { return hash(x) / 4294967295.0; }
struct Rotation {
    double sy, cy, sx, cx;
    explicit Rotation(double t, double period) {
        double yaw = 0.30 + t * (2*pi / period);
        double pitch = -0.11 + 0.035*std::sin(t*2*pi/period);
        sy = std::sin(yaw); cy = std::cos(yaw);
        sx = std::sin(pitch); cx = std::cos(pitch);
    }
    V world(V p) const {
        V q{cy*p.x+sy*p.z, p.y, -sy*p.x+cy*p.z};
        return {q.x, cx*q.y-sx*q.z, sx*q.y+cx*q.z};
    }
    V local(V p) const {
        V q{p.x, cx*p.y+sx*p.z, -sx*p.y+cx*p.z};
        return {cy*q.x-sy*q.z, q.y, sy*q.x+cy*q.z};
    }
};
double box2(double x, double y, double w, double h) {
    double dx = std::abs(x)-w, dy = std::abs(y)-h;
    double ox=std::max(dx,0.0), oy=std::max(dy,0.0);
    return std::sqrt(ox*ox+oy*oy) + std::min(std::max(dx,dy),0.0);
}
// One extruded cruciform, rather than intersecting translucent boxes. The
// outline is inset before rounding, keeping the traditional Latin proportions.
double outline(V p) {
    return std::min(box2(p.x,p.y,0.185,1.405), box2(p.x,p.y-0.52,1.075,0.185));
}
double distance(V p) {
    double d = outline(p), z = std::abs(p.z)-0.115;
    double od=std::max(d,0.0), oz=std::max(z,0.0);
    return std::sqrt(od*od+oz*oz) + std::min(std::max(d,z),0.0) - 0.045;
}
V normal(V p) {
    constexpr double e = 0.0002;
    return unit({distance(p+V{e,0,0})-distance(p-V{e,0,0}),
                 distance(p+V{0,e,0})-distance(p-V{0,e,0}),
                 distance(p+V{0,0,e})-distance(p-V{0,0,e})});
}
RGB encode(V c) {
    // Gentle filmic shoulder: retain warm highlights instead of clipping white.
    auto channel = [](double v) {
        return static_cast<int>(std::lround(255*std::pow(1-std::exp(-std::max(0.0,v)), 1/2.2)));
    };
    return {channel(c.x),channel(c.y),channel(c.z)};
}
V hue(Theme theme) {
    if (theme == Theme::matrix) return {0.26,0.95,0.49};
    if (theme == Theme::ivory) return {0.92,0.91,0.79};
    return {1.0,0.58,0.19};
}
bool inscription(V p, double t) {
    // Tiny object-space strokes remain attached to the metal as it turns.
    double u = (p.x+2)*28, v = (p.y+2)*28;
    int col = static_cast<int>(std::floor(u)), row = static_cast<int>(std::floor(v));
    int x = static_cast<int>(fract(u)*5), y = static_cast<int>(fract(v)*7);
    if (x >= 3 || y >= 5) return false;
    uint32_t seed = hash(static_cast<uint32_t>(col*311+row*73));
    // Only a few glyphs change, on a slow shared clock. No random flashing.
    if (seed % 7 == 0) seed = hash(seed+static_cast<uint32_t>(t*2));
    return ((seed >> (y*3+x)) & 1u) != 0;
}
V backdrop(double x, double y, double pixel, const Settings& s) {
    V accent = hue(s.theme);
    double radius = std::hypot(x,y-0.5);
    double aa = std::max(0.007,pixel*0.40);
    double ring = std::exp(-std::pow((radius-0.94)/aa,2))*0.13
                + std::exp(-std::pow((radius-1.015)/aa,2))*0.065;
    double glow = 0.012*std::exp(-radius*radius*1.3);
    double rays = std::pow(std::abs(std::cos(std::atan2(y-0.5,x)*12)),20)
                * std::exp(-std::pow((radius-1.20)/0.30,2))*0.001;
    return V{0.0006,0.0011,0.0013} + accent*(ring+glow+rays);
}
struct Sample { V light; bool solid; };
Sample sample(double x, double y, double t, const Settings& s, const Rotation& rot, V background) {
    V accent = hue(s.theme);
    double lens = 2.45-x*x-y*y;
    if (lens <= 0) return {background,false};
    double front = std::sqrt(lens);
    V origin = rot.local({x,y,front+0.01});
    V direction = rot.local({0,0,-1});
    double travel = 0;
    V p{};
    bool hit = false;
    for (int step=0; step<80; ++step) {
        p = origin+direction*travel;
        double d = distance(p);
        if (d < 0.0007) { hit = true; break; }
        travel += d;
        if (travel > 2*front+0.03) break;
    }
    if (!hit) return {background,false};
    V nlocal = normal(p), n = rot.world(nlocal);
    static const V key = unit({-0.65,0.85,1.4}), fill = unit({0.9,0.1,0.4});
    static const V halfway = unit(key+V{0,0,1});
    double diffuse = std::max(0.0,dot(n,key));
    double bounce = std::max(0.0,dot(n,fill));
    double shine = std::pow(std::max(0.0,dot(n,halfway)),18);
    double rim = std::pow(1-std::max(0.0,n.z),3);
    V metal = accent*(0.18+0.65*diffuse+0.10*bounce)
            + V{1.0,0.90,0.68}*(1.3*shine) + accent*(0.16*rim);
    double edge = -outline(p);
    bool face = std::abs(nlocal.z)>0.98;
    if (face && edge>0.035) {
        // Engraved code within a bright, continuous gold border.
        double ink = inscription(p,t) ? 0.68 : 0.48;
        metal = metal*ink;
        double engraving = std::exp(-std::pow((edge-0.064)/0.009,2));
        metal = metal + accent*(0.16*engraving);
        double sweep = std::exp(-std::pow((p.y-0.65*std::sin(t*0.20))/0.33,2));
        metal = metal + accent*(0.028*sweep);
    }
    return {metal,true};
}
char32_t rainGlyph(uint32_t seed, bool unicode) {
    static const char ascii[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:+=<>";
    return unicode ? 0xff66+seed%56 : ascii[seed%(sizeof(ascii)-1)];
}
std::string color(RGB c) {
    char out[8];
    std::snprintf(out,sizeof(out),"#%02x%02x%02x",c.r,c.g,c.b);
    return out;
}
int index256(RGB c) {
    auto cube=[](int x) { return x<48 ? 0 : x<115 ? 1 : std::min(5,(x-35)/40); };
    int r=cube(c.r),g=cube(c.g),b=cube(c.b);
    const int values[]={0,95,135,175,215,255};
    auto sq=[](int v){return v*v;};
    int error=sq(c.r-values[r])+sq(c.g-values[g])+sq(c.b-values[b]);
    int grey=std::clamp(((c.r+c.g+c.b)/3-8+5)/10,0,23), v=8+10*grey;
    return sq(c.r-v)+sq(c.g-v)+sq(c.b-v)<error ? 232+grey : 16+36*r+6*g+b;
}
void sgr(std::string& out, RGB c, bool background, int colors) {
    if (colors == 0) return;
    char b[64];
    if (colors == 256) std::snprintf(b,sizeof(b),"\033[%d;5;%dm",background?48:38,index256(c));
    else std::snprintf(b,sizeof(b),"\033[%d;2;%d;%d;%dm",background?48:38,c.r,c.g,c.b);
    out += b;
}
}

Frame render(int columns, int rows, double t, const Settings& s) {
    Frame frame{columns,rows,std::vector<Cell>(static_cast<size_t>(columns)*rows)};
    if (columns < 1 || rows < 1) return frame;
    const int height=rows*2;
    double scale=std::min(height/3.65,columns*0.5/s.aspect/2.85);
    double pixel=1/scale;
    Rotation rotation(t,s.period);
    // The nimbus does not move. Cache its antialiased light at each half-cell
    // instead of repeating trigonometry on every animation frame.
    struct BackdropCache {
        int columns=0,rows=0,samples=0;
        double aspect=0;
        Theme theme=Theme::gold;
        std::vector<V> light;
    };
    static thread_local BackdropCache cache;
    if(cache.columns!=columns || cache.rows!=rows || cache.aspect!=s.aspect || cache.theme!=s.theme || cache.samples!=s.samples) {
        cache={columns,rows,s.samples,s.aspect,s.theme,std::vector<V>(static_cast<size_t>(columns)*height)};
        for(int y=0;y<height;++y)for(int x=0;x<columns;++x) {
            V light{0,0,0};
            for(int sy=0;sy<s.samples;++sy)for(int sx=0;sx<s.samples;++sx) {
                double px=(x+(sx+0.5)/s.samples-columns/2.0)*2*s.aspect/scale;
                double py=(height/2.0-y-(sy+0.5)/s.samples)/scale;
                light=light+backdrop(px,py,pixel,s);
            }
            cache.light[static_cast<size_t>(y)*columns+x]=light*(1.0/(s.samples*s.samples));
        }
    }
    std::vector<bool> occupied(frame.cells.size(),false);
    for (int row=0;row<rows;++row) for (int col=0;col<columns;++col) {
        Cell& cell=frame.cells[static_cast<size_t>(row)*columns+col];
        for (int half=0;half<2;++half) {
            V sum{0,0,0};
            for (int sy=0;sy<s.samples;++sy) for (int sx=0;sx<s.samples;++sx) {
                double x=(col+(sx+0.5)/s.samples-columns/2.0)*(2*s.aspect)/scale;
                double y=(height/2.0-(row*2+half+(sy+0.5)/s.samples))/scale;
                Sample hit=sample(x,y,t,s,rotation,cache.light[static_cast<size_t>(row*2+half)*columns+col]);
                sum=sum+hit.light;
                if (hit.solid) occupied[static_cast<size_t>(row)*columns+col]=true;
            }
            RGB c=encode(sum*(1.0/(s.samples*s.samples)));
            (half==0?cell.top:cell.bottom)=c;
        }
    }
    // Independent deterministic streams, graded tails and occasional pearl heads
    // echo neo without making the quiet central composition flash or strobe.
    if (s.rain) for (int col=0;col<columns;++col) {
        uint32_t seed=hash(static_cast<uint32_t>(col)+39u);
        if (seed%5==0) continue;
        double speed=1.4+random(seed)*3.2, span=rows+14.0;
        double head=std::fmod(t*speed+random(seed+1)*span,span);
        double tail=5+random(seed+2)*rows*0.45;
        for (int row=0;row<rows;++row) {
            size_t i=static_cast<size_t>(row)*columns+col;
            if (occupied[i]) continue;
            double age=head-row;
            if (age<0 || age>tail) continue;
            double x=(col+0.5-columns/2.0)*2*s.aspect/scale;
            double y=(height/2.0-row*2-1)/scale;
            // Keep the two fine rings legible; rain belongs behind the nimbus.
            double radius=std::hypot(x,y-0.5);
            if (std::abs(radius-0.94)<pixel*1.4 || std::abs(radius-1.015)<pixel*1.4) continue;
            // Make room for the silhouette and halo: peripheral rain is brighter.
            double quiet=0.25+0.75*smooth(0.55,1.8,std::hypot(x,y-0.3));
            double energy=std::pow(1-age/tail,1.7)*quiet;
            Cell& cell=frame.cells[i];
            cell.glyph=rainGlyph(hash(seed+static_cast<uint32_t>(row)*97u+static_cast<uint32_t>(t*1.5)),s.unicode);
            V rain=s.theme==Theme::ivory ? V{0.19,0.29,0.31} : V{0.055,0.36,0.16};
            cell.ink=encode(rain*(0.018+0.16*energy));
            // Preserve the aureole underneath the glyph's cell background.
            cell.top=cell.bottom={std::max(cell.top.r,cell.bottom.r),std::max(cell.top.g,cell.bottom.g),std::max(cell.top.b,cell.bottom.b)};
            if (age<0.8) cell.ink=encode(V{0.40,0.66,0.48}*(0.09*quiet));
        }
    }
    if (s.caption && columns>=54 && rows>=18) {
        const std::string title="L U X   C R U C I S";
        const std::string keys="space pause   +/- speed   t palette   r rain   q quit";
        auto label=[&](int row,const std::string& text,RGB ink){
            int left=(columns-static_cast<int>(text.size()))/2;
            for (size_t k=0;k<text.size();++k) {
                Cell& c=frame.cells[static_cast<size_t>(row)*columns+left+k];
                c.top=c.bottom={3,6,7}; c.ink=ink; c.glyph=text[k];
            }
        };
        label(rows-2,title,{148,127,84});
        label(rows-1,keys,{56,75,70});
    }
    if (!s.unicode) {
        const std::string ramp=" .,:;irsXA253hMHGS#9B&@";
        for (Cell& c:frame.cells) if (!c.glyph) {
            c.ink={(c.top.r+c.bottom.r)/2,(c.top.g+c.bottom.g)/2,(c.top.b+c.bottom.b)/2};
            int brightness=std::max({c.ink.r,c.ink.g,c.ink.b});
            c.glyph=ramp[static_cast<size_t>(brightness)*(ramp.size()-1)/255];
            c.top=c.bottom={0,0,0};
        }
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
       <<"\" viewBox=\"0 0 "<<f.columns*w<<' '<<f.rows*16<<"\"><title>Lux Crucis — rotating code cross</title>"
       <<"<rect width=\"100%\" height=\"100%\" fill=\"#030607\"/>\n";
    for (int y=0;y<f.rows;++y) for (int x=0;x<f.columns;++x) {
        const Cell& c=f.cells[static_cast<size_t>(y)*f.columns+x];
        auto rect=[&](double top,int h,RGB rgb){out<<"<rect x=\""<<x*w<<"\" y=\""<<top<<"\" width=\""<<w<<"\" height=\""<<h<<"\" fill=\""<<color(rgb)<<"\"/>\n";};
        if (!c.glyph) {rect(y*16,8,c.top);rect(y*16+8,8,c.bottom);}
        else {
            rect(y*16,16,c.bottom);
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
