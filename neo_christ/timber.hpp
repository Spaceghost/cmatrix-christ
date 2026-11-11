// SPDX-License-Identifier: GPL-3.0-or-later
// A plausible timber reconstruction, not a claim about an exact surviving cross.
#pragma once
#include "ornament.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace christ::timber {
using ornament::V;
using ornament::dot;
using ornament::unit;
constexpr double pi=ornament::pi;
inline double clamp(double x) { return std::clamp(x,0.0,1.0); }
inline uint32_t hash(uint32_t x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);
}
inline double random(int x,int y) {
    return hash(static_cast<uint32_t>(x)*3737u+static_cast<uint32_t>(y)*97u)/4294967295.0;
}
inline double noise(double x,double y) {
    int ix=static_cast<int>(std::floor(x)),iy=static_cast<int>(std::floor(y));
    double u=x-ix,v=y-iy;u=u*u*(3-2*u);v=v*v*(3-2*v);
    double a=random(ix,iy)*(1-u)+random(ix+1,iy)*u;
    double b=random(ix,iy+1)*(1-u)+random(ix+1,iy+1)*u;
    return a*(1-v)+b*v;
}
struct Rotation {
    double cy,sy,cx,sx;
    Rotation(double time,double period) {
        double yaw=0.30+time*2*pi/period,pitch=-0.075;
        cy=std::cos(yaw);sy=std::sin(yaw);cx=std::cos(pitch);sx=std::sin(pitch);
    }
    V world(V p) const {
        V q{cy*p.x+sy*p.z,p.y,-sy*p.x+cy*p.z};
        return {q.x,cx*q.y-sx*q.z,sx*q.y+cx*q.z};
    }
    V local(V p) const {
        V q{p.x,cx*p.y+sx*p.z,-sx*p.y+cx*p.z};
        return {cy*q.x-sy*q.z,q.y,sy*q.x+cy*q.z};
    }
};
struct Piece {
    V center,half;
    int kind; // Upright, crossbeam, inscription board.
    V local(V p) const {
        p=p-center;
        if(kind==2)return {0.99875026*p.x-0.04997917*p.y,0.04997917*p.x+0.99875026*p.y,p.z};
        return p;
    }
    V vector(V p) const {
        if(kind==2)return {0.99875026*p.x-0.04997917*p.y,0.04997917*p.x+0.99875026*p.y,p.z};
        return p;
    }
    V normalToObject(V n) const {
        if(kind==2)return {0.99875026*n.x+0.04997917*n.y,-0.04997917*n.x+0.99875026*n.y,n.z};
        return n;
    }
    double distance(V p) const {
        V h=half;
        double along=kind==0?p.y:p.x;
        // Low-amplitude hewing irregularity changes the geometry, not just color.
        if(kind==0) {
            h.x+=0.006*std::sin(along*3)+0.003*std::sin(along*17+1);
            h.z+=0.005*std::sin(along*5+2);
            p.x-=0.004*std::sin(along*4);
            h.y+=0.009*std::sin(p.x*43+p.z*18)+0.004*std::sin(p.x*93-p.z*31);
        } else {
            h.y+=0.005*std::sin(along*4+1)+0.002*std::sin(along*19);
            if(kind==1)h.z+=0.004*std::sin(along*9);
            h.x+=0.009*std::sin(p.y*46+p.z*15)+0.004*std::sin(p.y*91-p.z*37);
        }
        double bevel=kind==2?0.003:0.008;
        V q{std::abs(p.x)-h.x+bevel,std::abs(p.y)-h.y+bevel,std::abs(p.z)-h.z+bevel};
        V outside{std::max(0.0,q.x),std::max(0.0,q.y),std::max(0.0,q.z)};
        return std::sqrt(dot(outside,outside))+std::min(std::max({q.x,q.y,q.z}),0.0)-bevel;
    }
    V normal(V p) const {
        constexpr double e=0.0003;
        return unit({distance(p+V{e,0,0})-distance(p-V{e,0,0}),
                     distance(p+V{0,e,0})-distance(p-V{0,e,0}),
                     distance(p+V{0,0,e})-distance(p-V{0,0,e})});
    }
    bool interval(V origin,V direction,double& near,double& far) const {
        near=0;far=10;
        const double o[]={origin.x,origin.y,origin.z};
        const double d[]={direction.x,direction.y,direction.z};
        const double bounds[]={half.x+0.025,half.y+0.025,half.z+0.025};
        for(int axis=0;axis<3;++axis) {
            if(std::abs(d[axis])<1e-12) {if(std::abs(o[axis])>bounds[axis])return false;continue;}
            double a=(-bounds[axis]-o[axis])/d[axis],b=(bounds[axis]-o[axis])/d[axis];
            if(a>b)std::swap(a,b);
            near=std::max(near,a);far=std::min(far,b);
            if(near>far)return false;
        }
        return true;
    }
};
inline bool inscription(V p) {
    // Traditional Latin shorthand, painted in object space on the front face.
    // Four 4x7 Roman letters. No screen-space text floating over the rotation.
    constexpr unsigned letters[4][7]={
        {15,6,6,6,6,6,15}, {9,13,13,11,11,9,9},
        {14,9,9,14,10,9,9}, {15,6,6,6,6,6,15}
    };
    int x=static_cast<int>(std::floor((p.x+0.2375)/0.025));
    int y=static_cast<int>(std::floor((0.0875-p.y)/0.025));
    if(x<0 || x>=19 || y<0 || y>=7 || x%5==4)return false;
    return (letters[x/5][y]&(1u<<(3-x%5)))!=0;
}
inline V wood(V p,V n,int kind) {
    double along=kind==0?p.y:p.x;
    double across=std::abs(n.z)>0.55?(kind==0?p.x:p.y):p.z;
    double phase=across*112+2.1*std::sin(along*2.4)+1.4*noise(along*3,across*9);
    double knotDark=0;
    if(kind!=2)for(int k=0;k<2;++k) {
        double ku=k==0?0.037:-0.052;
        double kv=kind==0?(k==0?-0.63:0.94):(k==0?-0.67:0.48);
        double dx=(across-ku)/0.058,dy=(along-kv)/0.14;
        double r=std::hypot(dx,dy);
        if(r<5) {
            phase+=7*std::exp(-r*0.8)*std::sin(std::atan2(dy,dx));
            knotDark+=0.32*std::exp(-r*r*2)+0.13*std::exp(-r*r*0.15)*std::pow(0.5+0.5*std::sin(r*8),6);
        }
    }
    double vein=std::pow(0.5+0.5*std::sin(phase),14);
    double tone=0.77+0.24*noise(across*17,along*1.8)+0.045*std::sin(phase*0.5)-0.22*vein-knotDark;
    tone+=0.023*std::sin(across*350+along*6);
    // Sparse longitudinal checks, worn fibers and exposed end-grain.
    if(kind!=2)for(int i=0;i<4;++i) {
        double u=-0.125+i*0.079+0.006*std::sin(along*7+i*9);
        double envelope=std::exp(-std::pow((along+0.9-i*0.55)/0.4,6));
        tone-=0.24*std::exp(-std::pow((across-u)/0.0035,2))*envelope;
    }
    bool end=kind==0?std::abs(n.y)>0.65:std::abs(n.x)>0.65;
    if(end) {
        double u=kind==0?p.x:p.y;
        double radius=std::hypot(u*1.3,p.z);
        tone=0.62+0.10*std::sin(radius*160+noise(u*13,p.z*13)*2);
    }
    V base=kind==2?V{0.46,0.31,0.15}:V{0.30,0.135,0.052};
    V color=base*std::max(0.15,tone)+V{0.012,0.011,0.009};
    if(kind==2 && p.z>0.014) {
        if(inscription(p))color=color*0.075;
        for(double x:{-0.29,0.29})if(std::hypot(p.x-x,p.y)<0.009)color={0.017,0.016,0.014};
    }
    return color;
}
struct Sample { V light; bool solid; };
class Scene {
    Rotation rotation;
    std::array<Piece,3> pieces={{{{0,-0.025,0},{0.173,1.48,0.151},0},
                                {{0.012,0.53,0.11},{1.075,0.164,0.146},1},
                                {{0,1.12,0.20},{0.355,0.13,0.027},2}}};
    V direction;
public:
    Scene(double time,double period):rotation(time,period),direction(rotation.local({0,0,-1})) {}
    Sample sample(double x,double y) const {
        // A quiet, neutral dusk backdrop and a contact shadow give timber weight.
        double vignette=std::exp(-(x*x*0.2+y*y*0.12));
        double ground=clamp((-y-1.35)*2);
        V background=V{0.0022,0.0030,0.0039}*(0.6+0.4*vignette)+V{0.003,0.0022,0.0013}*ground;
        double shadow=0.7*std::exp(-std::pow(x/0.65,2)-std::pow((y+1.51)/0.045,2));
        background=background*(1-shadow);
        V origin=rotation.local({x,y,4});
        double closest=20;int part=-1;V point;
        for(size_t i=0;i<pieces.size();++i) {
            const Piece& piece=pieces[i];
            V o=piece.local(origin),d=piece.vector(direction);
            double begin,end;
            if(!piece.interval(o,d,begin,end) || begin>closest)continue;
            double t=begin;
            for(int step=0;step<72 && t<=end && t<closest;++step) {
                V p=o+d*t;
                double distance=piece.distance(p);
                if(distance<0.0003) {closest=t;part=static_cast<int>(i);point=p;break;}
                t+=distance*0.76;
            }
        }
        if(part<0)return {background,false};
        const Piece& piece=pieces[static_cast<size_t>(part)];
        V nlocal=piece.normal(point),n=rotation.world(piece.normalToObject(nlocal));
        static const V light=unit({-0.65,0.9,1.6}),fill=unit({0.8,0.15,0.7});
        double diffuse=std::max(0.0,dot(n,light)),bounce=std::max(0.0,dot(n,fill));
        double illumination=0.26+0.85*diffuse+0.15*bounce;
        if(part==0 && point.z>0.08)illumination*=1-0.22*std::exp(-std::pow((point.y-0.36)/0.05,2));
        return {wood(point,nlocal,piece.kind)*illumination,true};
    }
};
}
