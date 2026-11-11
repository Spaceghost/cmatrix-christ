// SPDX-License-Identifier: GPL-3.0-or-later
// Vesper: pierced, carved metal in a candlelit architectural setting.
#pragma once
#include "timber.hpp"

namespace christ::gothic {
using ornament::V;
using ornament::dot;
using ornament::unit;
constexpr double pi=ornament::pi;
inline double sq(double x) { return x*x; }
inline double clamp(double x) { return std::clamp(x,0.0,1.0); }
inline V multiply(V a,V b) { return {a.x*b.x,a.y*b.y,a.z*b.z}; }
inline double box(double x,double y,double w,double h) {
    double a=std::abs(x)-w,b=std::abs(y)-h;
    return std::hypot(std::max(a,0.0),std::max(b,0.0))+std::min(std::max(a,b),0.0);
}
inline double diamond(double x,double y,double w,double h) {
    return (h*std::abs(x)+w*std::abs(y)-w*h)/std::hypot(w,h);
}
inline double finial(double u,double v) {
    double d=diamond(u,v-0.13,0.108,0.28);
    for(double side:{-1.0,1.0}) {
        double x=u-side*0.128,y=v-0.075;
        d=std::min(d,diamond(0.88*x-side*0.475*y,side*0.475*x+0.88*y,0.068,0.16));
    }
    return d;
}
inline double lancet(double u,double v,double height) {
    // An elongated vesica: two intersecting arcs, pointed at both ends.
    double x=u/0.116,y=v/height;
    return (std::max(std::hypot(x+0.72,y),std::hypot(x-0.72,y))-1.23)*0.116;
}
inline double profile(double x,double y) {
    double d=std::min(box(x,y+0.15,0.12,1.35),box(x,y-0.52,0.99,0.12));
    d=std::min(d,std::hypot(x,y-0.52)-0.245);
    if(y>0.95)d=std::min(d,finial(x,y-1.20));
    if(y<-1.25)d=std::min(d,finial(x,-y-1.50));
    if(x>0.74)d=std::min(d,finial(y-0.52,x-0.99));
    if(x<-0.74)d=std::min(d,finial(y-0.52,-x-0.99));
    double cut=std::min(lancet(x,y+0.23,0.28),lancet(x,y+0.97,0.27));
    cut=std::min(cut,lancet(x,y-0.99,0.17));
    cut=std::min(cut,lancet(y-0.52,x-0.65,0.23));
    cut=std::min(cut,lancet(y-0.52,x+0.65,0.23));
    return std::max(d,-cut);
}
inline double distance(V p) {
    constexpr double bevel=0.019;
    double a=profile(p.x,p.y)+bevel,b=std::abs(p.z)-0.055+bevel;
    double d=std::hypot(std::max(a,0.0),std::max(b,0.0))+std::min(std::max(a,b),0.0)-bevel;
    double radius=std::hypot(p.x,p.y-0.52);
    if(radius<0.30) {
        double angle=std::atan2(p.y-0.52,p.x);
        double rose=0.179+0.018*std::cos(angle*8);
        d=std::min(d,std::max(radius-rose,std::abs(p.z)-0.082));
        double boss=(std::sqrt(sq(p.x/0.127)+sq((p.y-0.52)/0.127)+sq(p.z/0.118))-1)*0.118;
        d=std::min(d,boss);
    }
    return d;
}
inline double relief(double x,double y) {
    double border=profile(x,y);
    double h=0.005*std::exp(-sq((border+0.015)/0.007));
    h-=0.005*std::exp(-sq((border+0.038)/0.006));
    double u=x,v=y-0.52;
    if(std::abs(x)>0.26 && std::abs(y-0.52)<0.2) {u=y-0.52;v=std::abs(x);}
    double vine=0.079+0.017*std::sin(v*19);
    h-=0.0045*std::exp(-sq((std::abs(u)-vine)/0.005));
    double leaves=std::abs(u)-0.082-0.019*std::sin(v*38+u*12);
    h+=0.0024*std::exp(-sq(leaves/0.006));
    double r=std::hypot(x,y-0.52);
    if(r<0.25) {
        double angle=std::atan2(y-0.52,x);
        h+=0.007*std::sin(angle*8)*std::exp(-sq((r-0.17)/0.055));
    }
    return h;
}
struct Rotation {
    double cy,sy,cx,sx,cz,sz;
    Rotation(double time,double period) {
        double phase=time*2*pi/period;
        // Linger on the carved faces; cross each edge with a continuous velocity.
        double yaw=0.29+phase-0.18*std::sin(phase*2);
        double pitch=-0.11+0.018*std::sin(phase);
        double roll=-0.015+0.012*std::sin(phase);
        cy=std::cos(yaw);sy=std::sin(yaw);cx=std::cos(pitch);sx=std::sin(pitch);
        cz=std::cos(roll);sz=std::sin(roll);
    }
    V world(V p) const {
        V a{cy*p.x+sy*p.z,p.y,-sy*p.x+cy*p.z};
        V b{a.x,cx*a.y-sx*a.z,sx*a.y+cx*a.z};
        return {cz*b.x-sz*b.y,sz*b.x+cz*b.y,b.z};
    }
    V local(V p) const {
        V a{cz*p.x+sz*p.y,-sz*p.x+cz*p.y,p.z};
        V b{a.x,cx*a.y+sx*a.z,-sx*a.y+cx*a.z};
        return {cy*b.x-sy*b.z,b.y,sy*b.x+cy*b.z};
    }
};
class Scene {
    Rotation rotation;
    V direction;
    double phase;
public:
    Scene(double time,double period):rotation(time,period),direction(rotation.local({0,0,-1})),phase(time*2*pi/period) {}
    V backdrop(double x,double y) const {
        double mist=std::exp(-sq(x/1.55)-sq((y-0.45)/1.85));
        V color=V{0.00045,0.0007,0.0015}+V{0.003,0.0055,0.011}*mist;
        // A central lancet window and receding stone arches. All light is behind
        // the sculpture, so the openings stay legible throughout the turn.
        for(int i=-1;i<=1;++i) {
            double center=i*2.15,u=x-center,w=i==0?0.92:0.62,spring=0.68;
            double a=y>spring?
                std::max(std::hypot(u+w,y-spring),std::hypot(u-w,y-spring))-2*w:
                std::abs(u)-w;
            double height=clamp((y+1.8)*1.2);
            double arch=std::exp(-sq(a/0.028))*height;
            double outer=std::exp(-sq((a-0.105)/0.035))*height;
            color=color+V{0.012,0.015,0.021}*(arch*0.72+outer*0.3);
            double inside=clamp(-a*8)*std::exp(-sq((y-0.9)/1.6));
            color=color+V{0.0015,0.0026,0.0055}*inside;
            if(i==0) {
                double mullion=std::exp(-sq(u/0.022))*inside;
                color=color*(1-0.60*mullion);
            }
        }
        double shaft=std::exp(-sq((x+0.38-0.29*(2.15-y))/0.31));
        double veil=std::exp(-sq((y-0.3)/1.8));
        color=color+V{0.011,0.006,0.0025}*(shaft*veil);
        for(double side:{-1.0,1.0}) {
            double glow=std::exp(-sq((x-side*1.72)/0.52)-sq((y+0.9)/0.83));
            color=color+V{0.018,0.0065,0.0013}*glow;
            double u=x-side*1.72;
            double wax=clamp((0.026-std::abs(u))*180)*clamp((y+1.50)*100)*clamp((-0.96-y)*100);
            color=color*(1-wax)+V{0.062,0.042,0.024}*wax;
            double holder=clamp((0.060-std::abs(u))*110)*clamp((y+1.60)*100)*clamp((-1.50-y)*100);
            color=color*(1-holder)+V{0.020,0.016,0.011}*holder;
            double sway=0.005*std::sin(phase*7+side),flicker=0.9+0.1*std::sin(phase*9+side*2);
            double flame=std::exp(-sq((u-sway)/0.022)-sq((y+0.895)/0.072));
            double heart=std::exp(-sq((u-sway)/0.011)-sq((y+0.915)/0.030));
            color=color+V{0.48,0.17,0.027}*(flame*flicker)+V{0.80,0.53,0.20}*heart;
        }
        double floor=std::exp(-sq(x/1.05)-sq((y+1.99)/0.16));
        color=color+V{0.009,0.008,0.008}*floor;
        double shadow=0.72*std::exp(-sq(x/0.43)-sq((y+1.96)/0.06));
        return color*(1-shadow);
    }
    timber::Sample sample(double x,double y) const {
        V origin=rotation.local({x,y,4});
        static const timber::Piece bounds{{0,-0.12,0},{1.46,1.86,0.20},0};
        V bounded=origin-bounds.center;
        double begin,end;
        if(!bounds.interval(bounded,direction,begin,end))return {{},false};
        V p;
        bool hit=false;
        for(int step=0;step<88 && begin<=end;++step) {
            p=origin+direction*begin;
            double d=distance(p);
            if(d<0.00045) {hit=true;break;}
            begin+=std::max(d*0.68,0.00008);
        }
        if(!hit)return {{},false};
        constexpr double e=0.0006;
        V n=unit({distance(p+V{e,0,0})-distance(p-V{e,0,0}),
                  distance(p+V{0,e,0})-distance(p-V{0,e,0}),
                  distance(p+V{0,0,e})-distance(p-V{0,0,e})});
        double radius=std::hypot(p.x,p.y-0.52);
        if(std::abs(n.z)>0.7 && radius>0.13) {
            constexpr double delta=0.0015;
            double dx=(relief(p.x+delta,p.y)-relief(p.x-delta,p.y))/(2*delta);
            double dy=(relief(p.x,p.y+delta)-relief(p.x,p.y-delta))/(2*delta);
            n=unit(n+V{-dx,-dy,0});
        }
        double sideChannel=std::abs(n.z)<0.5?std::exp(-sq(p.z/0.015)):0;
        n=rotation.world(n);
        static const V key=unit({-0.65,1.1,1.15}),rim=unit({1.0,0.3,-0.65});
        static const V candle=unit({-1.5,-0.25,0.8}),fill=unit({0.8,0.4,0.4});
        static const V halfKey=unit(key+V{0,0,1}),halfCandle=unit(candle+V{0,0,1});
        double diffuse=std::max(0.0,dot(n,key)),warm=std::max(0.0,dot(n,candle));
        double rimlight=std::pow(std::max(0.0,dot(n,rim)),1.8);
        double cool=std::max(0.0,dot(n,fill));
        double grain=0.86+0.14*timber::noise(p.x*67+p.z*19,p.y*74);
        double border=std::abs(profile(p.x,p.y));
        double edging=std::exp(-sq((border-0.013)/0.013));
        V metal=V{0.026,0.033,0.047}*grain;
        double carving=radius>0.23?clamp((relief(p.x,p.y)+0.001)*100):0;
        metal=metal+V{0.16,0.145,0.12}*(edging+carving*0.20);
        double polish=24;
        if(radius<0.127 && std::abs(p.z)>0.079) {metal={0.13,0.012,0.028};polish=55;}
        if(radius>0.13 && radius<0.2)metal=V{0.19,0.17,0.13}*grain;
        V light=V{0.10,0.13,0.22}+V{0.92,1.05,1.3}*diffuse+
                V{1.10,0.49,0.16}*warm+V{0.13,0.20,0.36}*cool;
        V color=multiply(metal,light);
        double shine=std::pow(std::max(0.0,dot(n,halfKey)),polish);
        double gold=std::pow(std::max(0.0,dot(n,halfCandle)),36);
        color=color+V{0.45,0.54,0.72}*(shine*(0.22+0.34*edging))+V{0.68,0.33,0.10}*(gold*0.55);
        color=color+V{0.50,0.28,0.10}*(rimlight*0.72);
        return {color*(1-0.60*sideChannel),true};
    }
};
}
