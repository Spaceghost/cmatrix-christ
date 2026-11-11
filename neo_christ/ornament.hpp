// SPDX-License-Identifier: GPL-3.0-or-later
// Procedural openwork: all curves, pearls and settings live in object space.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace christ::ornament {
constexpr double pi = 3.14159265358979323846;
struct V {
    double x=0, y=0, z=0;
    V operator+(V b) const { return {x+b.x,y+b.y,z+b.z}; }
    V operator-(V b) const { return {x-b.x,y-b.y,z-b.z}; }
    V operator*(double s) const { return {x*s,y*s,z*s}; }
};
inline double dot(V a,V b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline double length(V a) { return std::sqrt(dot(a,a)); }
inline V unit(V a) { double n=length(a); return n>1e-12?a*(1/n):V{0,0,1}; }
enum class Material { gold, ruby, pearl, halo };
struct Wire { V a,b; double radius; Material material=Material::gold; };
struct Model {
    std::vector<Wire> wires;
    void line(V a,V b,double radius=0.009,Material material=Material::gold) {
        wires.push_back({a,b,radius,material});
    }
    void bead(V p,double r,Material m=Material::gold) { line(p,p,r,m); }
    template<class F> void curve(F point,int steps=48,double radius=0.009,Material m=Material::gold) {
        V last=point(0.0);
        for(int i=1;i<=steps;++i) {
            V next=point(static_cast<double>(i)/steps);
            line(last,next,radius,m);last=next;
        }
    }
    void bezier(V a,V b,V c,V d,double r=0.01) {
        curve([=](double t){double u=1-t;return a*(u*u*u)+b*(3*u*u*t)+c*(3*u*t*t)+d*(t*t*t);},24,r);
    }
    void ring(V center,double radius,double wire=0.009,Material m=Material::gold,int steps=80) {
        curve([=](double t){double a=t*2*pi;return center+V{radius*std::cos(a),radius*std::sin(a),0};},steps,wire,m);
    }
};

inline Model cross() {
    Model model;
    constexpr double joint=0.35;
    const std::array<std::array<double,3>,4> arms={{{0,1,0.82},{0,-1,1.40},{1,0,0.96},{-1,0,0.96}}};
    for(auto arm:arms) {
        double dx=arm[0],dy=arm[1],end=arm[2];
        auto point=[=](double u,double v,double z=0.0){return V{-dy*u+dx*v,joint+dx*u+dy*v,z};};
        // Paired rails and a twisted inner vine leave the arm visibly open.
        for(double side:{-1.0,1.0}) {
            model.bezier(point(side*0.115,0.19),point(side*0.08,end*0.45),
                         point(side*0.10,end-0.24),point(side*0.16,end-0.14),0.013);
            model.curve([=](double t){double v=0.18+t*(end-0.32);
                return point(side*0.052*std::sin(v*22),v,0.040*std::cos(v*22));},64,0.008);
            // Pearls on the rails and a second, recessed gold strand give depth.
            for(double v=0.24;v<end-0.2;v+=0.10)model.bead(point(side*0.105,v,0.015),0.018,Material::pearl);
            model.bezier(point(side*0.115,0.19,-0.045),point(side*0.08,end*0.45,-0.045),
                         point(side*0.10,end-0.24,-0.045),point(side*0.16,end-0.14,-0.045),0.008);
        }
        // Leaf-shaped openings along each arm; longer stem gets more leaves.
        for(double v=0.32;v<end-0.22;v+=0.245) {
            model.curve([=](double t){double a=t*2*pi;
                return point(0.066*std::sin(a),v+0.125*std::cos(a),0.035);},36,0.008);
        }
        // Each terminal is a three-petal fleur-de-lis, with scroll shoulders.
        for(double side:{-1.0,1.0}) {
            model.bezier(point(0,end-0.12),point(side*0.16,end+0.01),
                         point(side*0.15,end+0.17),point(0,end+0.31),0.012);
            model.bezier(point(0,end-0.13),point(side*0.26,end+0.16),
                         point(side*0.36,end+0.02),point(side*0.27,end-0.13),0.012);
            model.bezier(point(side*0.27,end-0.13),point(side*0.13,end-0.31),
                         point(side*0.035,end-0.21),point(0,end-0.13),0.012);
            model.curve([=](double t){double a=-pi*0.6+t*pi*1.8,r=0.080*(1-t*0.78);
                return point(side*(0.205+r*std::cos(a)),end-0.065+r*std::sin(a),0.04);},36,0.008);
            model.bead(point(side*0.29,end-0.045,0.025),0.021,Material::pearl);
            model.line(point(side*0.105,end-0.22,-0.045),point(side*0.105,end-0.22,0.045),0.013);
        }
        model.line(point(-0.135,end-0.20,0.035),point(0.135,end-0.20,0.035),0.016);
        model.bead(point(0,end+0.31),0.026,Material::pearl);
        // A small cabochon hangs in the central petal, visible on both faces.
        model.curve([=](double t){double a=t*2*pi;return point(0.042*std::sin(a),end+0.063+0.068*std::cos(a),0.025);},40,0.009);
        model.bead(point(0,end+0.065,0.025),0.039,Material::ruby);
        model.bead(point(0,end+0.065,-0.025),0.039,Material::ruby);
    }
    // Four open acanthus leaves bridge the arms, each enclosing a curled tendril.
    for(double sx:{-1.0,1.0})for(double sy:{-1.0,1.0}) {
        auto p=[=](double x,double y,double z=0.025){return V{sx*x,joint+sy*y,z};};
        model.bezier(p(0.12,0.19),p(0.55,0.13),p(0.72,0.51),p(0.48,0.63),0.012);
        model.bezier(p(0.48,0.63),p(0.20,0.67),p(0.16,0.43),p(0.12,0.19),0.012);
        model.bezier(p(0.14,0.20),p(0.28,0.28),p(0.38,0.52),p(0.48,0.63),0.007);
        model.curve([=](double t){double a=-pi*0.35+t*2*pi,r=0.17*(1-t*0.83);
            return p(0.41+r*std::cos(a),0.39+r*std::sin(a),0.065);},70,0.009);
        model.bead(p(0.48,0.63),0.024,Material::pearl);
        model.bead(p(0.41,0.39,0.065),0.019,Material::pearl);
        // Small leaflets follow the outer scroll without filling its opening.
        model.bezier(p(0.30,0.27),p(0.43,0.19),p(0.49,0.25),p(0.49,0.33),0.007);
    }
    // Raised central rosette, eight lobes, granulated setting, ruby on both sides.
    for(double face:{-1.0,1.0}) {
        V center{0,joint,face*0.065};
        model.curve([=](double t){double a=t*2*pi,r=0.198+0.025*std::cos(8*a);
            return center+V{r*std::cos(a),r*std::sin(a),0};},160,0.013);
        model.ring(center,0.115,0.012);
        for(int i=0;i<16;++i) {
            double a=i*2*pi/16;
            model.bead(center+V{0.153*std::cos(a),0.153*std::sin(a),face*0.015},0.017,Material::pearl);
        }
        for(int i=0;i<8;++i) {
            double a=i*2*pi/8;
            model.line(center+V{0.12*std::cos(a),0.12*std::sin(a),0},
                       center+V{0.208*std::cos(a),0.208*std::sin(a),0},0.008);
        }
        model.bead({0,joint,face*0.10},0.096,Material::ruby);
    }
    return model;
}

inline Model nimbus() {
    Model model;
    V center{0,0.35,-2};
    model.ring(center,1.035,0.004,Material::halo,200);
    model.ring(center,1.095,0.004,Material::halo,200);
    model.curve([=](double t){double a=t*2*pi,r=1.065+0.018*std::cos(32*a);
        return center+V{r*std::cos(a),r*std::sin(a),0};},512,0.0035,Material::halo);
    for(int i=0;i<48;++i) {
        double a=i*2*pi/48;
        double outer=1.17+(i%4==0?0.075:i%2==0?0.030:0);
        V dir{std::cos(a),std::sin(a),0};
        model.line(center+dir*1.115,center+dir*outer,0.0035,Material::halo);
        model.bead(center+dir*(outer+0.022),0.008,Material::halo);
    }
    return model;
}
}
