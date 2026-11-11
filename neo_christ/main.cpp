// SPDX-License-Identifier: GPL-3.0-or-later
#include "renderer.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <clocale>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <langinfo.h>
#include <poll.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace {
using Clock = std::chrono::steady_clock;
volatile sig_atomic_t stopped=0, suspended=0, resized=0;
void signalHandler(int signal) {
    if (signal==SIGWINCH || signal==SIGCONT) resized=1;
    else if (signal==SIGTSTP) suspended=1;
    else stopped=1;
}
bool writeAll(const std::string& data) {
    size_t offset=0;
    while (offset<data.size()) {
        ssize_t n=write(STDOUT_FILENO,data.data()+offset,data.size()-offset);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) return false;
        offset+=static_cast<size_t>(n);
    }
    return true;
}
class Terminal {
    termios saved{};
    bool active=false;
public:
    Terminal() {
        if (tcgetattr(STDIN_FILENO,&saved)<0) throw std::runtime_error("cannot read terminal settings");
        enter();
    }
    void enter() {
        termios mode=saved;
        mode.c_lflag &= static_cast<tcflag_t>(~(ECHO|ICANON));
        mode.c_cc[VMIN]=0; mode.c_cc[VTIME]=0;
        if (tcsetattr(STDIN_FILENO,TCSANOW,&mode)<0) throw std::runtime_error("cannot set terminal input mode");
        active=true;
        if (!writeAll("\033[?1049h\033[?25l\033[?7l\033[0m\033[2J\033[H")) {
            leave(); throw std::runtime_error("cannot write to terminal");
        }
    }
    void leave() noexcept {
        if (!active) return;
        // End a synchronized frame even when interrupted halfway through one.
        writeAll("\033[?2026l\033[0m\033[?7h\033[?25h\033[?1049l");
        tcsetattr(STDIN_FILENO,TCSANOW,&saved);
        active=false;
    }
    ~Terminal() { leave(); }
};
struct Options {
    christ::Settings settings;
    int fps=60, frames=0, columns=160, rows=54, colors=-1;
    double at=0;
    bool fixedSize=false, first=false;
    std::string output;
};
const char* help=R"(neo-christ — Lux Crucis
A slowly rotating, beveled code cross in true color.

Usage: neo-christ [options]
       neo-christ --first-rendition
       neo-christ --export frame.svg --size 160x54 --at 6

  --theme gold|matrix|ivory   Gold by default
  --period SECONDS           Seconds per revolution (4–600; default 36)
  --fps N                    Frame limit (1–120; default 60)
  --quality high|low         4 or 1 samples per half-cell pixel
  --color auto|truecolor|256|mono
  --ascii                    Use ASCII shading and rain
  --aspect RATIO             Cell width / height (0.25–1; default 0.5)
  --no-rain                  Cross and nimbus alone
  --quiet                    Hide title and keyboard hints
  --first-rendition          Run the original cmatrix-christ code cross
  --export PATH.svg          Export one actual rendered frame; no TTY needed
  --size COLUMNSxROWS        Export size, or maximum live size (8x4–400x180)
  --at SECONDS               Start/export at this animation time (0–86400)
  --frames N                 Stop after N frames (for recordings/tests)
  -h, --help                 Show this help
  --version                  Show version

Keys: space pause/resume · +/- speed · t palette · r rain · h hints · q/Esc quit
Ctrl-C exits cleanly. Ctrl-Z suspends and restores the terminal until resumed.
Use a UTF-8 terminal with a Japanese font for half-width kana. Non-UTF-8
locales and NO_COLOR automatically use ASCII. No GPU or font download needed.

Inspired by st3w/neo and Spaceghost/cmatrix-christ. GPL-3.0-or-later.
)";
double number(const std::string& text,double low,double high,const std::string& option) {
    size_t used=0;
    double value;
    try {value=std::stod(text,&used);} catch (...) {throw std::runtime_error(option+": invalid number");}
    if (used!=text.size() || !std::isfinite(value) || value<low || value>high)
        throw std::runtime_error(option+": number outside the supported range (see --help)");
    return value;
}
int integer(const std::string& text,int low,int high,const std::string& option) {
    double n=number(text,low,high,option);
    if (std::floor(n)!=n) throw std::runtime_error(option+": expected a whole number");
    return static_cast<int>(n);
}
Options parse(int argc,char** argv) {
    Options o;
    std::setlocale(LC_CTYPE,"");
    std::string encoding=nl_langinfo(CODESET);
    o.settings.unicode=(encoding=="UTF-8" || encoding=="UTF8");
    for (int i=1;i<argc;++i) {
        std::string arg=argv[i];
        auto value=[&](){if(i+1>=argc)throw std::runtime_error(arg+": missing value");return std::string(argv[++i]);};
        if (arg=="--help" || arg=="-h") {std::cout<<help;std::exit(0);}
        else if (arg=="--version") {std::cout<<"neo-christ 0.1.0 (Lux Crucis)\n";std::exit(0);}
        else if (arg=="--period") o.settings.period=number(value(),4,600,arg);
        else if (arg=="--fps") o.fps=integer(value(),1,120,arg);
        else if (arg=="--frames") o.frames=integer(value(),1,10000000,arg);
        else if (arg=="--at") o.at=number(value(),0,86400,arg);
        else if (arg=="--aspect") o.settings.aspect=number(value(),0.25,1,arg);
        else if (arg=="--no-rain") o.settings.rain=false;
        else if (arg=="--ascii") o.settings.unicode=false;
        else if (arg=="--quiet") o.settings.caption=false;
        else if (arg=="--first-rendition") o.first=true;
        else if (arg=="--export") o.output=value();
        else if (arg=="--size") {
            std::string size=value();size_t x=size.find('x');
            if(x==std::string::npos)throw std::runtime_error("--size: expected COLUMNSxROWS");
            o.columns=integer(size.substr(0,x),8,400,arg);
            o.rows=integer(size.substr(x+1),4,180,arg);o.fixedSize=true;
        } else if (arg=="--theme") {
            std::string name=value();
            if(name=="gold")o.settings.theme=christ::Theme::gold;
            else if(name=="matrix")o.settings.theme=christ::Theme::matrix;
            else if(name=="ivory")o.settings.theme=christ::Theme::ivory;
            else throw std::runtime_error("--theme: choose gold, matrix or ivory");
        } else if (arg=="--quality") {
            std::string name=value();
            if(name=="high")o.settings.samples=2;
            else if(name=="low")o.settings.samples=1;
            else throw std::runtime_error("--quality: choose high or low");
        } else if (arg=="--color") {
            std::string name=value();
            if(name=="auto")o.colors=-1;
            else if(name=="truecolor")o.colors=16777216;
            else if(name=="256")o.colors=256;
            else if(name=="mono")o.colors=0;
            else throw std::runtime_error("--color: choose auto, truecolor, 256 or mono");
        } else throw std::runtime_error("unknown option: "+arg+" (see --help)");
    }
    if(o.colors<0) {
        std::string color=std::getenv("COLORTERM")?std::getenv("COLORTERM"):"";
        std::string term=std::getenv("TERM")?std::getenv("TERM"):"";
        if(std::getenv("NO_COLOR"))o.colors=0;
        else if(color=="truecolor" || color=="24bit" || term.find("direct")!=std::string::npos)o.colors=16777216;
        else o.colors=256;
    }
    if(o.colors==0)o.settings.unicode=false;
    return o;
}
void original(const char* program) {
    std::error_code error;
    auto path=std::filesystem::read_symlink("/proc/self/exe",error);
    if(error)path=std::filesystem::absolute(program,error);
    auto binary=path.parent_path()/"cmatrix";
    if(setenv("CMATRIX_CROSS","1",1)<0)throw std::runtime_error("cannot enable the first rendition");
    execl(binary.c_str(),binary.c_str(),"-a","-b","-c","-u","2","-C","green",static_cast<char*>(nullptr));
    throw std::runtime_error("first rendition unavailable; build both programs with make -f Makefile.christ");
}
std::pair<int,int> size(const Options& o) {
    winsize window{};
    int cols=80,rows=24;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&window)==0) {
        if(window.ws_col)cols=window.ws_col;
        if(window.ws_row)rows=window.ws_row;
    }
    cols=std::clamp(cols,1,o.fixedSize?o.columns:400);
    rows=std::clamp(rows,1,o.fixedSize?o.rows:180);
    return {cols,rows};
}
int run(Options o) {
    for(int sig:{SIGINT,SIGTERM,SIGHUP,SIGQUIT,SIGWINCH,SIGTSTP,SIGCONT}) {
        struct sigaction action{};
        action.sa_handler=signalHandler;
        sigemptyset(&action.sa_mask);
        if(sigaction(sig,&action,nullptr)<0)throw std::runtime_error("cannot install terminal signal handler");
    }
    std::signal(SIGPIPE,SIG_IGN);
    Terminal terminal;
    christ::Frame previous{0,0,{}};
    auto last=Clock::now(),due=last;
    const auto interval=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0/o.fps));
    double time=o.at,speed=1;
    bool paused=false,dirty=true;
    int frames=0;
    while(!stopped) {
        if(suspended) {
            terminal.leave();suspended=0;
            raise(SIGSTOP);
            if(stopped)break;
            terminal.enter();previous={0,0,{}};dirty=true;last=due=Clock::now();
        }
        auto now=Clock::now();
        if(!paused)time+=std::chrono::duration<double>(now-last).count()*speed;
        last=now;
        if(resized) {resized=0;dirty=true;previous={0,0,{}};writeAll("\033[2J");}
        auto [columns,rows]=size(o);
        if(columns!=previous.columns || rows!=previous.rows)dirty=true;
        if(!paused || dirty) {
            auto frame=christ::render(columns,rows,time,o.settings);
            // DEC synchronized updates are ignored by terminals without support.
            if(!writeAll("\033[?2026h"+christ::ansi(frame,previous.cells.empty()?nullptr:&previous,o.colors)+"\033[?2026l"))
                throw std::runtime_error("terminal output closed");
            previous=std::move(frame);dirty=false;
        }
        if(o.frames && ++frames>=o.frames)break;
        due+=interval;
        if(due<Clock::now())due=Clock::now(); // Never burst frames to catch up.
        // Poll at least once even when rendering exceeds the frame budget.
        // Otherwise a slow terminal could starve every keyboard control.
        do {
            double remaining=std::chrono::duration<double,std::milli>(due-Clock::now()).count();
            pollfd fd{STDIN_FILENO,POLLIN,0};
            int status=poll(&fd,1,std::max(0,static_cast<int>(std::ceil(remaining))));
            if(status<0) {if(errno==EINTR)break;throw std::runtime_error("terminal input failed");}
            if(fd.revents&(POLLHUP|POLLERR|POLLNVAL)) {stopped=1;break;}
            if(status>0 && (fd.revents&POLLIN)) {
                char input[32];ssize_t n=read(STDIN_FILENO,input,sizeof(input));
                for(ssize_t i=0;i<n;++i) {
                    char c=input[i];
                    if(c=='q' || c=='Q' || c==27)stopped=1;
                    else if(c==' ') {paused=!paused;last=Clock::now();}
                    else if(c=='+' || c=='=')speed=std::min(4.0,speed*1.2);
                    else if(c=='-')speed=std::max(0.125,speed/1.2);
                    else if(c=='r') {o.settings.rain=!o.settings.rain;dirty=true;}
                    else if(c=='h') {o.settings.caption=!o.settings.caption;dirty=true;}
                    else if(c=='t') {o.settings.theme=static_cast<christ::Theme>((static_cast<int>(o.settings.theme)+1)%3);dirty=true;}
                }
            }
        } while(!stopped && !suspended && Clock::now()<due);
    }
    return 0;
}
}

int main(int argc,char** argv) {
    try {
        Options options=parse(argc,argv);
        if(options.first) {
            if(!options.output.empty())throw std::runtime_error("--first-rendition cannot be combined with --export");
            if(!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
                throw std::runtime_error("run the first rendition in an interactive terminal");
            original(argv[0]);
        }
        if(!options.output.empty()) {
            auto frame=christ::render(options.columns,options.rows,options.at,options.settings);
            std::ofstream file(options.output,std::ios::binary);
            if(!file || !(file<<christ::svg(frame,options.settings.aspect)))
                throw std::runtime_error("cannot write export: "+options.output);
            file.close();
            if(!file)throw std::runtime_error("cannot finish export: "+options.output);
            return 0;
        }
        if(!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
            throw std::runtime_error("run in an interactive terminal, or use --export frame.svg");
        if(std::getenv("TERM") && std::string(std::getenv("TERM"))=="dumb")
            throw std::runtime_error("TERM=dumb cannot animate; use --export frame.svg");
        return run(options);
    } catch(const std::exception& e) {
        std::cerr<<"neo-christ: "<<e.what()<<'\n';return 1;
    }
}
