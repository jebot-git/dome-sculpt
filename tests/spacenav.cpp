// SPDX-License-Identifier: GPL-3.0-or-later
#include "spacenav.hpp"
#include <atomic>
#include <bit>
#include <climits>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
using namespace dome;
using namespace dome::desktop;
namespace {
size_t checks=0;
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
std::atomic_bool pending_connect=false;
struct Server {
 std::filesystem::path directory,path;int listening=-1,peer=-1;
 Server(){char pattern[]="/tmp/dome-spnav-XXXXXX";auto p=mkdtemp(pattern);if(!p)throw std::runtime_error("mkdtemp");directory=p;path=directory/"spnav.sock";listening=socket(AF_UNIX,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);sockaddr_un a{};a.sun_family=AF_UNIX;std::strcpy(a.sun_path,path.c_str());if(listening<0||bind(listening,reinterpret_cast<sockaddr*>(&a),sizeof(a))||listen(listening,1))throw std::runtime_error("Unix fixture bind/listen");}
 ~Server(){if(peer>=0)close(peer);if(listening>=0)close(listening);std::error_code e;std::filesystem::remove_all(directory,e);}
 void accept_client(){peer=accept4(listening,nullptr,nullptr,SOCK_NONBLOCK|SOCK_CLOEXEC);if(peer<0)throw std::runtime_error("Unix fixture accept");}
 void bytes(const void *data,size_t count){auto p=static_cast<const unsigned char*>(data);while(count){auto n=send(peer,p,count,MSG_NOSIGNAL);if(n<0&&errno==EINTR)continue;if(n<=0)throw std::runtime_error("Unix fixture send");p+=n;count-=size_t(n);}}
 void event(std::array<int32_t,8> words){bytes(words.data(),sizeof(words));}
 void flood(){std::array<int32_t,8*300> words{};for(size_t i=0;i<300;++i){words[i*8+1]=350;words[i*8+7]=8;}bytes(words.data(),sizeof(words));}
 void motion(int32_t x=0,int32_t y=0,int32_t z=0,int32_t rx=0,int32_t ry=0,int32_t rz=0,int32_t period=8){event({0,x,y,z,rx,ry,rz,period});}
};
bool same(const SpaceView&a,const SpaceView&b){return a.rotation.w==b.rotation.w&&a.rotation.x==b.rotation.x&&a.rotation.y==b.rotation.y&&a.rotation.z==b.rotation.z&&a.pan.x==b.pan.x&&a.pan.y==b.pan.y&&a.pan.z==b.pan.z&&a.distance==b.distance;}
using M=std::array<double,9>;
M multiply(M a,M b){M r{};for(int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)r[i*3+j]+=a[i*3+k]*b[k*3+j];return r;}
std::array<double,3> point(M a,std::array<double,3> p){return {a[0]*p[0]+a[1]*p[1]+a[2]*p[2],a[3]*p[0]+a[4]*p[1]+a[5]*p[2],a[6]*p[0]+a[7]*p[1]+a[8]*p[2]};}
M matrix(Quat q){auto a=q.matrix();return {a[0],a[4],a[8],a[1],a[5],a[9],a[2],a[6],a[10]};}
void mathematics(){
 SpaceSettings settings;settings.validate();
 auto extreme=space_axes({INT_MIN,INT_MAX,INT_MIN,INT_MAX,INT_MIN,INT_MAX},settings);
 double t=0,r=0;for(int i=0;i<3;++i){t+=extreme[i]*extreme[i];r+=extreme[i+3]*extreme[i+3];}
 check(t<=1.000001&&r<=1.000001,"All six native int32 axes, including extremes, saturate to bounded translation and rotation forces without integer overflow");
 check(space_axes({15,-15,0,15,-15,0},settings)==std::array<float,6>{},"Configured neutral deadzone stops all six axes exactly");
 const auto axes=space_axes({120,105,50,60,90,75},settings);
 SpaceView initial;initial.rotation={std::cos(.21f),0,0,std::sin(.21f)};initial.pan={-.4f,.7f,-.2f};initial.pivot={1.2f,-.8f,.9f};initial.distance=4.3f;
 constexpr double duration=1.2;
 auto original=matrix(initial.rotation);auto center=point(original,{initial.pivot.x,initial.pivot.y,initial.pivot.z});auto initial_pan=std::array{initial.pan.x,initial.pan.y,initial.pan.z};for(int i=0;i<3;++i)center[i]+=initial_pan[i];
 double depth=initial.distance-center[2],new_depth=depth*std::exp(axes[2]*settings.translation*duration);
 std::array<double,3> target{(center[0]/depth+axes[0]*settings.translation*duration)*new_depth,(center[1]/depth+axes[1]*settings.translation*duration)*new_depth,center[2]};
 double angular_norm=std::sqrt(double(axes[3])*axes[3]+double(axes[4])*axes[4]+double(axes[5])*axes[5]);double angle=angular_norm*settings.rotation*3.14159265358979323846/180*duration,c=std::cos(angle),s=std::sin(angle);std::array<double,3> u{axes[3]/angular_norm,axes[4]/angular_norm,axes[5]/angular_norm};M increment{};
 for(int i=0;i<3;++i)for(int j=0;j<3;++j)increment[i*3+j]=(i==j?c:0)+(1-c)*u[i]*u[j];
 increment[1]-=s*u[2];increment[2]+=s*u[1];increment[3]+=s*u[2];increment[5]-=s*u[0];increment[6]-=s*u[1];increment[7]+=s*u[0];auto expected=multiply(increment,original);auto rotated=point(expected,{initial.pivot.x,initial.pivot.y,initial.pivot.z});
 for(int frames:{30,60,120,240}) {
  auto view=initial;bool advancing=true;for(int i=0;i<frames;++i)advancing &= advance_space_view(view,axes,settings,duration/frames);check(advancing,"Every continuous six-axis interval advances in the tested frame schedule");
  auto actual=matrix(view.rotation);double error=0;for(int k=0;k<9;++k)error=std::max(error,std::abs(actual[k]-expected[k]));auto actual_pan=std::array{view.pan.x,view.pan.y,view.pan.z};for(int k=0;k<3;++k)error=std::max(error,std::abs(double(actual_pan[k])-(target[k]-rotated[k])));error=std::max(error,std::abs(view.distance-(center[2]+new_depth)));
  check(error<3e-5,"Independent double Rodrigues/normalized-screen solution agrees across four frame schedules for composed orbit, pan and dolly");
  float tangent=std::tan(3.14159265358979323846f/8),aspect=1.6f;
  Vec3 eye{0,0,view.distance};Vec3 ray=normalized(Vec3{float(target[0]/new_depth),float(target[1]/new_depth),-1});
  // Same native perspective/ray convention, independently reconstructed from
  // analytic target projection; it must still pass through the world pivot.
  float nx=float(target[0]/new_depth)/tangent/aspect,ny=float(target[1]/new_depth)/tangent;
  ray=normalized(Vec3{nx*aspect*tangent,ny*tangent,-1});auto direction=view.rotation.inverse().rotate(ray);auto origin=view.rotation.inverse().rotate(eye-view.pan);auto delta=view.pivot-origin;
  check(length(cross(delta,direction))<3e-5,"Native inverse perspective ray still reaches the stable world pivot after combined six-axis motion");
 }
 auto unchanged=initial;check(!advance_space_view(unchanged,axes,settings,1)&&same(unchanged,initial),"A stalled camera interval cannot create an unbounded catch-up jump");
 settings.rotation=INFINITY;check(!advance_space_view(unchanged,axes,settings,.01)&&same(unchanged,initial),"Malformed settings cannot partially replace the camera");
}
void transport(){
 Server server;SpaceUnix client({server.path.string()});size_t events=0;SpacePacket last;
 auto sink=[&](const SpacePacket&p){++events;last=p;};auto result=client.pump(0,sink);server.accept_client();
 check(result.connected&&(fcntl(client.descriptor(),F_GETFL)&O_NONBLOCK)&&(fcntl(client.descriptor(),F_GETFD)&FD_CLOEXEC),"Actual native Unix transport connects without blocking or leaking across exec");
 char byte;check(recv(server.peer,&byte,1,MSG_DONTWAIT)<0&&(errno==EAGAIN||errno==EWOULDBLOCK),"Receive-only client never changes daemon sensitivity, protocol or global configuration");
 std::array<int32_t,8> packet{0,1,2,3,4,5,6,17};server.bytes(packet.data(),7);result=client.pump(.01,sink);check(events==0&&result.drained,"A partial native stream packet is retained without inventing an event");server.bytes(reinterpret_cast<unsigned char*>(packet.data())+7,25);client.pump(.02,sink);check(events==1&&last.axes==std::array<int32_t,6>{1,2,3,4,5,6}&&last.period==17,"Split packet reconstructs every axis and period exactly");
 server.bytes(packet.data(),11);client.pump(.03,sink);client.taint_partial();server.bytes(reinterpret_cast<unsigned char*>(packet.data())+11,21);client.pump(.04,sink);check(events==1,"Focus fencing discards an old incomplete packet without losing stream framing");server.motion();client.pump(.05,sink);check(events==2&&last.axes==std::array<int32_t,6>{},"Next complete neutral packet survives the partial-packet fence");
 server.flood();result=client.pump(.06,sink);check(result.backlog&&result.packets==256,"Backpressure pulse bounds native reads instead of growing a pending event queue");result=client.pump(.07,sink);check(result.drained&&result.packets==44,"Following pulse drains the exact remaining native backlog");
 server.event({1,INT_MAX,0,0,0,0,0,0});client.pump(.08,sink);check(last.kind==SpacePacket::Kind::Invalid,"Out-of-range mapped button cannot index or shift outside the bounded button state");
 close(server.peer);server.peer=-1;result=client.pump(.09,sink);check(!result.connected,"Actual EOF disconnects the native endpoint immediately");client.pump(.2,sink);check(!client.connected(),"Reconnect uses a bounded retry delay instead of a busy loop");result=client.pump(1,sink);server.accept_client();check(result.connected,"Real local daemon reconnection starts a new connection epoch");
}
void discovery(){
 const char*existing=std::getenv("SPNAV_SOCKET");std::optional<std::string> previous;if(existing)previous=existing;
 struct Restore {std::optional<std::string>&value;~Restore(){if(value)setenv("SPNAV_SOCKET",value->c_str(),1);else unsetenv("SPNAV_SOCKET");}} restore{previous};
 setenv("SPNAV_SOCKET","/tmp/dome-spacenav-explicit.sock",1);
 check(spacenav_socket_paths()==std::vector<std::string>{"/tmp/dome-spacenav-explicit.sock"},"Explicit SPNAV_SOCKET selects only the requested native daemon");
 std::string huge(108,'x');setenv("SPNAV_SOCKET",huge.c_str(),1);check(spacenav_socket_paths().empty(),"Overlong explicit daemon path remains quiet and cannot fall back to another daemon");
 SpaceUnix empty(std::vector<std::string>{});check(!empty.pump(0,[](const SpacePacket&){}).connected,"A deliberately unavailable daemon configuration leaves ordinary application use intact");
 bool refused=false;try{SpaceUnix malformed({std::string("/tmp/a\0b",8)});}catch(const std::invalid_argument&){refused=true;}check(refused,"Embedded-NUL native paths refuse before creating a socket");
}
void backlog_connect(){
 Server server;
 std::vector<int> waiting;
 struct Cleanup {std::vector<int>&fds;~Cleanup(){for(int fd:fds)close(fd);}} cleanup{waiting};
 bool full=false;
 for(int n=0;n<8;++n){
  int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);if(fd<0)throw std::runtime_error("queue socket");
  waiting.push_back(fd);sockaddr_un a{};a.sun_family=AF_UNIX;std::strcpy(a.sun_path,server.path.c_str());
  if(connect(fd,reinterpret_cast<sockaddr*>(&a),sizeof(a))<0){if(errno!=EAGAIN)throw std::runtime_error("expected real AF_UNIX backlog EAGAIN");full=true;break;}
 }
 check(full,"A real bounded Unix listener produces native backlog EAGAIN");
 SpaceUnix client({server.path.string()});auto sink=[](const SpacePacket&){};
 auto p=client.pump(0,sink);check(!p.connected&&client.descriptor()<0,"AF_UNIX EAGAIN closes the unsuccessful endpoint instead of treating it as pending success");
 int accepted;while((accepted=accept4(server.listening,nullptr,nullptr,SOCK_NONBLOCK|SOCK_CLOEXEC))>=0)close(accepted);
 p=client.pump(.2,sink);check(!p.connected,"Backlog refusal respects retry timing after the server drains");
 p=client.pump(1,sink);server.accept_client();check(p.connected,"A genuine connection succeeds after native backlog recovery");
}
size_t descriptors(){size_t count=0;for(const auto &entry:std::filesystem::directory_iterator("/proc/self/fd")){(void)entry;++count;}return count;}
void lifecycle(){
 const auto count=descriptors();
 for(int n=0;n<32;++n){Server server;SpaceUnix client({server.path.string()});client.pump(0,[](const SpacePacket&){});server.accept_client();client.enable(false,.1);if(client.descriptor()>=0)throw std::runtime_error("Turning navigation off retained its transport");client.enable(true,.2);client.pump(.2,[](const SpacePacket&){});close(server.peer);server.peer=-1;server.accept_client();}
 check(descriptors()==count,"Repeated real connect, disable, reconnect and destruction retain no descriptors");
 SpaceUnix absent({"/tmp/dome-spacenav-missing-fixture/socket"});auto p=absent.pump(0,[](const SpacePacket&){});check(!p.connected&&p.packets==0,"Missing daemon is an ordinary quiet state");
}
void legacy_button_fence(){
 Server server;SpaceNavigator nav({server.path.string()});SpaceView view;
 nav.tick(view,true,0);server.accept_client();server.motion();nav.tick(view,true,.01);
 server.event({1,7,0,0,0,0,0,0});nav.tick(view,true,.02);
 nav.tick(view,false,.03);nav.tick(view,true,.04);server.motion();nav.tick(view,true,.05);
 server.motion(350);nav.tick(view,true,.06);auto before=view;nav.tick(view,true,.07);
 check(view.pan.x>before.pan.x,"Legacy device without unplug notification cannot leave an obsolete button latch blocking fresh neutral navigation");
 server.event({1,0,0,0,0,0,0,0});auto update=nav.tick(view,true,.08);
 check(update.action==SpaceUpdate::Action::None,"An unknown held button still cannot invoke Frame after ownership recovery");
 server.event({2,0,0,0,0,0,0,0});nav.tick(view,true,.09);server.event({1,0,0,0,0,0,0,0});update=nav.tick(view,true,.10);
 check(update.action==SpaceUpdate::Action::FrameSelection,"A genuine new release restores the frame button after legacy ownership recovery");
 nav.tick(view,false,.11);server.event({1,7,0,0,0,0,0,0});nav.tick(view,true,.12);
 server.motion();nav.tick(view,true,.13);server.motion(350);nav.tick(view,true,.14);before=view;nav.tick(view,true,.15);
 check(view.pan.x>before.pan.x,"A stale button packet drained at focus restoration cannot recreate an obsolete held latch");
}
void controller(){
 Server server;SpaceNavigator nav({server.path.string()});SpaceView view;nav.tick(view,true,0);server.accept_client();server.motion();nav.tick(view,true,.01);check(std::string_view(nav.status()).find("ready")!=std::string_view::npos,"New connection needs an observed fresh neutral before navigation");
 server.event({2,0,0,0,0,0,0,0});server.motion(350);nav.tick(view,true,.02);auto start=view;nav.tick(view,true,.03);check(view.pan.x>start.pan.x,"Held native axis integrates elapsed camera time rather than emulating a mouse");
 auto before=view;nav.tick(view,false,.04);server.motion(350);nav.tick(view,false,.05);nav.tick(view,true,.06);server.motion(350);nav.tick(view,true,.07);check(same(view,before),"Focus/modal/gesture ownership fences held and queued axes until a new neutral");server.motion();nav.tick(view,true,.08);server.motion(350);nav.tick(view,true,.09);nav.tick(view,true,.1);check(view.pan.x>before.pan.x,"Neutral then fresh input resumes camera navigation");
 before=view;nav.tick(view,true,.5);check(same(view,before),"Stalled UI time drops old deflection instead of jumping the camera");server.motion();nav.tick(view,true,.51);nav.tick(view,true,.52);server.motion();nav.tick(view,true,.53);
 server.event({1,0,0,0,0,0,0,0});auto action=nav.tick(view,true,.54);check(action.action==SpaceUpdate::Action::FrameSelection&&same(view,before),"Known released first button requests existing Frame selection without moving the camera or scene");server.event({1,0,0,0,0,0,0,0});check(nav.tick(view,true,.55).action==SpaceUpdate::Action::None,"Repeated held frame button cannot reissue actions");server.event({2,0,0,0,0,0,0,0});server.motion();nav.tick(view,true,.56);server.motion();nav.tick(view,true,.57);server.event({1,1,0,0,0,0,0,0});check(nav.tick(view,true,.58).action==SpaceUpdate::Action::None,"A reconnect-unknown button must first establish release before it can frame");server.event({2,1,0,0,0,0,0,0});nav.tick(view,true,.59);server.event({1,1,0,0,0,0,0,0});check(nav.tick(view,true,.60).action==SpaceUpdate::Action::FrameAll,"Known released second button requests existing Frame all");
 nav.tick(view,true,.61);server.event({2,1,0,0,0,0,0,0});server.motion();nav.tick(view,true,.62);server.motion(350);nav.tick(view,true,.63);before=view;server.flood();nav.tick(view,true,.64);nav.tick(view,true,.65);server.motion(350);nav.tick(view,true,.66);check(same(view,before),"Queued input flood discards pending motion and requires post-drain neutral re-arm");server.motion();nav.tick(view,true,.67);server.motion(350);nav.tick(view,true,.68);nav.tick(view,true,.69);check(view.pan.x>before.pan.x,"Backpressure recovery accepts fresh neutral then fresh motion");

 // A device/config transition may not provide release for an old held button.
 server.event({1,7,0,0,0,0,0,0});nav.tick(view,true,.695);
 server.event({3,0,0,0,0,0,0,0});nav.tick(view,true,.70);
 server.motion();nav.tick(view,true,.71);server.motion();nav.tick(view,true,.72);
 server.motion(350,0,0,0,0,0,0);nav.tick(view,true,.73);before=view;nav.tick(view,true,.74);
 check(view.pan.x>before.pan.x,"Device replacement drops obsolete held buttons and re-arms only after fresh neutral");
 server.motion();nav.tick(view,true,.75);before=view;
 for(int n=1;n<=30;++n)nav.tick(view,true,.75+n*.01);
 server.motion(350,0,0,0,0,0,INT_MIN);nav.tick(view,true,1.06);nav.tick(view,true,1.07);
 check(view.pan.x>before.pan.x,"Idle neutral stays ready; zero or wrapped wire periods never set elapsed camera time");
 before=view;close(server.peer);server.peer=-1;nav.tick(view,true,1.08);check(same(view,before),"Socket loss clears pending movement before it can affect the camera");
}
}
extern "C" int __real_connect(int,const sockaddr*,socklen_t);
extern "C" int __wrap_connect(int fd,const sockaddr*address,socklen_t size){int result=__real_connect(fd,address,size);if(result==0&&pending_connect.exchange(false)){errno=EINPROGRESS;return -1;}return result;}
int main()try{mathematics();transport();discovery();backlog_connect();lifecycle();controller();legacy_button_fence();{Server server;SpaceUnix c({server.path.string()});pending_connect=true;auto p=c.pump(0,[](const SpacePacket&){});server.accept_client();check(p.connected,"Native EINPROGRESS completion uses zero-time poll and SO_ERROR on a real connected Unix endpoint");}std::cout<<checks<<" native spacenav protocol/camera/fence checks passed\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
