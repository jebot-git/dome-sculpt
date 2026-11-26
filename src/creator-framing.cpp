// SPDX-License-Identifier: GPL-3.0-or-later
#include "creator-framing.hpp"
#include "document-transaction.hpp"
#include "dome/view-clip.hpp"
#include <cmath>
#include <limits>
namespace dome {
namespace {
bool rigid(const CanvasTransform&t){
  const auto q=t.orientation;
  return ob::finite(t)&&std::abs(t.scale-1)<1e-5f&&
    std::abs(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w-1)<1e-3f;
}
ob::Vec3 obv(Vec3 p){return{p.x,p.y,p.z};}

// Intersection of the two physical angular frusta in the head's forward
// tangent plane. Its interior center also handles valid off-axis/asymmetric
// projections whose overlap does not contain the head-forward ray.
std::optional<ob::Vec3> stereo_direction(CanvasTransform head,
    const std::array<SpatialView,2>&views,const std::array<std::array<double,4>,2>&slopes){
  std::array<std::array<double,3>,10> planes;
  for(size_t eye=0;eye<2;++eye){
    const auto inverse=ob::conjugate(views[eye].pose.orientation);
    const auto x=ob::rotate(inverse,ob::rotate(head.orientation,{1,0,0}));
    const auto y=ob::rotate(inverse,ob::rotate(head.orientation,{0,1,0}));
    const auto z=ob::rotate(inverse,ob::rotate(head.orientation,{0,0,-1}));
    const auto&s=slopes[eye];
    const std::array<std::array<double,3>,5> normals{{{1,0,s[0]},{-1,0,-s[1]},{0,1,s[2]},{0,-1,-s[3]},{0,0,-1}}};
    for(size_t i=0;i<5;++i){const auto&n=normals[i];planes[eye*5+i]={n[0]*x.x+n[1]*x.y+n[2]*x.z,n[0]*y.x+n[1]*y.y+n[2]*y.z,n[0]*z.x+n[1]*z.y+n[2]*z.z};}
  }
  std::array<std::array<double,2>,64> points;size_t count=0;
  auto add=[&](double x,double y){
    if(!std::isfinite(x)||!std::isfinite(y))return;
    for(const auto&p:planes)if(p[0]*x+p[1]*y+p[2]<-1e-8)return;
    for(size_t i=0;i<count;++i)if(std::hypot(points[i][0]-x,points[i][1]-y)<1e-8)return;
    if(count<points.size())points[count++]={x,y};
  };
  for(size_t i=0;i<planes.size();++i)for(size_t j=i+1;j<planes.size();++j){
    const auto&a=planes[i];const auto&b=planes[j];const auto determinant=a[0]*b[1]-b[0]*a[1];
    if(std::abs(determinant)>1e-12)add((a[1]*b[2]-a[2]*b[1])/determinant,(a[2]*b[0]-a[0]*b[2])/determinant);
  }
  if(count<3){
    add(0,0);
    for(size_t eye=0;eye<2;++eye){
      const auto&s=slopes[eye];const auto d=ob::rotate(ob::conjugate(head.orientation),ob::rotate(views[eye].pose.orientation,{float((s[0]+s[1])*.5),float((s[2]+s[3])*.5),-1}));
      if(d.z<-1e-7f)add(d.x/-d.z,d.y/-d.z);
    }
  }
  if(!count)return{};
  double x=0,y=0;for(size_t i=0;i<count;++i){x+=points[i][0]/count;y+=points[i][1]/count;}
  for(const auto&p:planes)if(p[0]*x+p[1]*y+p[2]<=1e-9)return{}; // No usable common interior.
  const auto length=std::sqrt(1+x*x+y*y);
  return ob::rotate(head.orientation,{float(x/length),float(y/length),float(-1/length)});
}

}
std::optional<CanvasTransform> detail::initial_creator_fit(Vec3 lo,Vec3 hi,
    CanvasTransform canvas,CanvasTransform head,const std::array<SpatialView,2>&views){
  if(!finite(lo)||!finite(hi)||lo.x>hi.x||lo.y>hi.y||lo.z>hi.z||!rigid(head)||
      !ob::finite(canvas)||canvas.scale<=0)return{};
  auto normalized_canvas=canvas;normalized_canvas.scale=1;if(!rigid(normalized_canvas))return{};
  std::array<std::array<double,4>,2> slopes;
  for(size_t eye=0;eye<views.size();++eye){
    if(!rigid(views[eye].pose))return{};
    const auto&f=views[eye].fov;
    for(auto angle:f)if(!std::isfinite(angle)||std::abs(angle)>=1.5707f)return{};
    if(f[1]-f[0]<1e-4f||f[3]-f[2]<1e-4f)return{};
    auto&s=slopes[eye];for(size_t k=0;k<4;++k)s[k]=std::tan(double(f[k]));
    // Reserve 5% of the actual tangent span at each edge, including asymmetric
    // and canted eyes. No assumed headset FoV or replacement projection.
    const auto x=(s[1]-s[0])*.05,y=(s[3]-s[2])*.05;
    s[0]+=x;s[1]-=x;s[2]+=y;s[3]-=y;
  }
  const auto center=(obv(lo)+obv(hi))*.5f;
  double lower=.85,upper=std::numeric_limits<double>::infinity();bool already=true;
  const auto direction=stereo_direction(head,views,slopes);if(!direction)return{};
  const auto forward=*direction;
  for(unsigned corner=0;corner<8;++corner){
    const ob::Vec3 point{corner&1?hi.x:lo.x,corner&2?hi.y:lo.y,corner&4?hi.z:lo.z};
    const auto offset=ob::rotate(canvas.orientation,(point-center)*canvas.scale);
    for(size_t eye=0;eye<views.size();++eye){
      const auto inverse=ob::conjugate(views[eye].pose.orientation);
      const auto current=ob::rotate(inverse,ob::transform(canvas,point)-views[eye].pose.position);
      const auto a=ob::rotate(inverse,head.position+offset-views[eye].pose.position);
      const auto b=ob::rotate(inverse,forward);const auto&s=slopes[eye];
      const auto near=std::max(.05f,view_near_clip),far=view_far_clip-.01f;
      const std::array<double,6> c{a.x+a.z*s[0],-a.x-a.z*s[1],a.y+a.z*s[2],-a.y-a.z*s[3],-a.z-near,far+a.z};
      const std::array<double,6> k{b.x+b.z*s[0],-b.x-b.z*s[1],b.y+b.z*s[2],-b.y-b.z*s[3],-b.z,b.z};
      already&=current.z<-near&&current.z>-far&&current.x+current.z*s[0]>=0&&-current.x-current.z*s[1]>=0&&
        current.y+current.z*s[2]>=0&&-current.y-current.z*s[3]>=0;
      for(size_t plane=0;plane<c.size();++plane){
        if(k[plane]>1e-9)lower=std::max(lower,-c[plane]/k[plane]);
        else if(k[plane]<-1e-9)upper=std::min(upper,-c[plane]/k[plane]);
        else if(c[plane]<0)return{};
      }
    }
  }
  if(already)return canvas;
  if(!std::isfinite(lower)||lower+.002>upper||lower>std::numeric_limits<float>::max()/4)return{};
  auto result=canvas;
  result.position=head.position+forward*float(lower+.002)-ob::rotate(canvas.orientation,center*canvas.scale);
  return ob::finite(result)?std::optional<CanvasTransform>{result}:std::nullopt;
}
void Document::cancel_initial_creator_fit_locked() noexcept {
  initial_creator_.pending.reset();initial_creator_.target_group=0;
}
bool Document::fit_initial_creator_locked(const SpatialFrame&f){
  auto&fit=initial_creator_;
  if(fit.pending&&fit.pending->done.load())fit.pending.reset(); // Failed/cancelled operation: no accepted target.
  if(!fit.target_group)return false;
  if(fit.navigated||fit.target_generation!=state_.generation||fit.target_revision!=state_.revision||
      fit.target_content!=content_||fit.target_input_epoch!=state_.input_epoch||scene_.active().group!=fit.target_group){cancel_initial_creator_fit_locked();return false;}
  if(!f.views||state_.creator||state_.asset_busy||input_.menu_open()||
      f.hands[0].tracked.trigger>.02f||f.hands[1].tracked.trigger>.02f||
      f.hands[0].tracked.squeeze>.55f||f.hands[1].tracked.squeeze>.55f)return false;
  const auto mesh=mesh_.load();
  if(mesh->generation!=fit.target_generation||mesh->revision!=fit.target_revision||mesh->view_epoch!=state_.view_epoch)return false;
  Vec3 lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};size_t count=0;
  for(const auto&range:mesh->ranges){
    bool owned=false;for(const auto&group:scene_.group_path(scene_.object(range.object).group))owned|=group.id==fit.target_group;
    if(!owned)continue;
    for(size_t index=range.first;index<range.first+range.count;++index){
      const auto p=mesh->mesh.positions.at(mesh->mesh.indices.at(index));++count;
      lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
      hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
    }
  }
  if(!count)return false;
  const auto canvas=detail::initial_creator_fit(lo,hi,state_.canvas,f.head,*f.views);
  if(!canvas)return false;
  Transaction transaction(*this);state_.canvas=*canvas;cancel_initial_creator_fit_locked();
  publish_locked(false,false);transaction.commit(false);return true;
}
}
