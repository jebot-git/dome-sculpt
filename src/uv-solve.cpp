// SPDX-License-Identifier: GPL-3.0-or-later
// Original native LSCM implementation of Levy et al. (2002); local triangle
// projection/complex relations inspected in pinned xatlas/OpenNL. See UV-SOLVE.md.
#include "dome/uv-solve.hpp"
#include "dome/rig.hpp"
#include <map>
#include <numeric>
#include <set>
namespace dome {
namespace {
using UV = std::array<float, 2>;
struct Dsu {
  std::vector<uint32_t> parents;
  explicit Dsu(size_t count) : parents(count) { std::iota(parents.begin(),parents.end(),0); }
  uint32_t root(uint32_t i) {
    while(parents[i]!=i){parents[i]=parents[parents[i]];i=parents[i];}return i;
  }
  void join(uint32_t a,uint32_t b){a=root(a);b=root(b);if(a!=b)parents[std::max(a,b)]=std::min(a,b);}
};
uint32_t next(uint32_t c){return c/3*3+(c+1)%3;}
double distance(Vec3 a,Vec3 b){double x=double(a.x)-b.x,y=double(a.y)-b.y,z=double(a.z)-b.z;return std::sqrt(x*x+y*y+z*z);}
struct Budget {
  const UvSolveOptions &options;
  const std::function<bool()> &cancelled;
  size_t work=0;
  void step(size_t amount=1){
    if(amount>options.maximum_work-work)throw std::runtime_error("UV solve work budget exceeded");
    work+=amount;
    if(cancelled&&cancelled())throw std::runtime_error("UV solve cancelled");
  }
};
struct Row {
  std::array<uint32_t,6> indices{};
  std::array<double,6> coefficients{};
  uint32_t count=0;
  double rhs=0;
};
UvSolveResult run(const Mesh &input,std::span<const UvStitchEdge> stitches,
                  double tolerance,const UvSolveOptions &options,
                  const std::function<bool()> &cancelled){
  if(!options.maximum_vertices||options.maximum_vertices>2'000'000||
     !options.maximum_triangles||options.maximum_triangles>2'000'000||
     !options.maximum_work||options.maximum_work>2'000'000'000||
     options.maximum_bytes<1024||options.maximum_bytes>1024ull*1024*1024||
     !options.maximum_iterations||options.maximum_iterations>20000||
     !std::isfinite(options.relative_tolerance)||options.relative_tolerance<1e-13||
     options.relative_tolerance>1e-2||!std::isfinite(tolerance)||tolerance<0||tolerance>1)
    throw std::invalid_argument("Invalid UV solve resource/convergence/tolerance options");
  if(input.positions.size()>options.maximum_vertices||input.indices.size()/3>options.maximum_triangles)
    throw std::runtime_error("UV solve vertex/triangle budget exceeded");
  // Conservative peak reservation, including map nodes, split attributes, rows,
  // solver vectors and adjacency. No quadratic/dense matrix allocations.
  long double estimate=static_cast<long double>(input.positions.size())*256+
                       static_cast<long double>(input.indices.size())*640;
  if(estimate>options.maximum_bytes)throw std::runtime_error("UV solve memory budget exceeded");
  input.validate();
  if(input.uv.size()!=input.positions.size())throw std::invalid_argument("UV conformal solve requires existing per-vertex UV coordinates");
  Budget budget{options,cancelled};budget.step(input.indices.size());
  const size_t corners=input.indices.size(),faces=corners/3;
  if(options.seed_faces.size()>faces||stitches.size()>corners/2)
    throw std::invalid_argument("UV selection/stitch count exceeds surface bounds");
  Dsu fans(corners),regions(faces);
  std::map<std::array<float,3>,uint32_t> point_ids;
  std::vector<uint32_t> point(input.positions.size());
  for(size_t i=0;i<input.positions.size();++i){
    budget.step();
    auto p=input.positions[i];
    if(std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>1e6f)
      throw std::invalid_argument("UV geometry exceeds finite coordinate bounds");
    auto[it,added]=point_ids.emplace(std::array{p.x,p.y,p.z},uint32_t(point_ids.size()));point[i]=it->second;
  }
  std::map<std::pair<uint32_t,uint32_t>,std::vector<uint32_t>> edges;
  for(uint32_t c=0;c<corners;++c){budget.step();edges[std::minmax(point[input.indices[c]],point[input.indices[next(c)]])].push_back(c);}
  std::vector<uint8_t> internal(corners,0);
  auto join=[&](uint32_t a,uint32_t b,bool reverse){
    fans.join(a,reverse?next(b):b);fans.join(next(a),reverse?b:next(b));
    regions.join(a/3,b/3);internal[a]=internal[b]=1;
  };
  auto same=[&](uint32_t a,uint32_t b){return point[input.indices[a]]==point[input.indices[b]];};
  for(const auto &[key,list]:edges){
    budget.step(list.size());
    if(list.size()>2)throw std::runtime_error("UV edge is nonmanifold; separate overlapping surfaces before conformal unwrap");
    if(list.size()!=2)continue;
    auto a=list[0],b=list[1];bool reverse=same(a,next(b));
    if(!reverse)throw std::runtime_error("UV chart has inconsistent triangle winding; repair orientation before unwrap");
    bool cut=!input.uv_seams.empty()&&(input.uv_seams[a]||input.uv_seams[b]);
    bool old_cut=input.uv[input.indices[a]]!=input.uv[input.indices[next(b)]]||input.uv[input.indices[next(a)]]!=input.uv[input.indices[b]];
    if(!cut&&(!options.preserve_existing_cuts||!old_cut))join(a,b,true);
  }
  UvSolveResult result;result.mesh=input;result.source_topology=morph_topology_id(input);
  if(!stitches.empty()&&result.mesh.uv_seams.empty())result.mesh.uv_seams.resize(corners);
  std::set<uint32_t> paired;
  for(auto pair:stitches){
    budget.step();
    if(pair.a>=corners||pair.b>=corners||pair.a/3==pair.b/3||
       !paired.insert(pair.a).second||!paired.insert(pair.b).second||
       internal[pair.a]||internal[pair.b])
      throw std::invalid_argument("Stitch requires unique explicit boundary/seam corner pairs");
    auto a=input.positions[input.indices[pair.a]],b=input.positions[input.indices[next(pair.a)]],
         c=input.positions[input.indices[pair.b]],d=input.positions[input.indices[next(pair.b)]];
    if(distance(a,d)>tolerance||distance(b,c)>tolerance)
      throw std::invalid_argument("Stitch edges must have opposite matching geometric endpoints within tolerance");
    join(pair.a,pair.b,true);
    result.mesh.uv_seams[pair.a]=result.mesh.uv_seams[pair.b]=0;
  }
  std::map<uint32_t,uint32_t> fan_ids;
  std::vector<uint32_t> corner_fan(corners),representative;
  std::vector<std::vector<uint32_t>> members;
  for(uint32_t c=0;c<corners;++c){
    budget.step();
    auto[it,added]=fan_ids.emplace(fans.root(c),uint32_t(fan_ids.size()));
    if(added){representative.push_back(input.indices[c]);members.emplace_back();}
    corner_fan[c]=it->second;members[it->second].push_back(c);
  }
  std::map<uint32_t,std::vector<uint32_t>> chart_faces;
  for(uint32_t f=0;f<faces;++f){budget.step();chart_faces[regions.root(f)].push_back(f);}
  std::set<uint32_t> selected;
  for(auto f:options.seed_faces){budget.step();if(f>=faces)throw std::invalid_argument("UV selected face out of range");selected.insert(regions.root(f));}
  if(!stitches.empty()&&options.seed_faces.empty())for(auto p:stitches)selected.insert(regions.root(p.a/3));
  std::vector<UV> solved(representative.size());
  std::vector<uint8_t> fixed(representative.size(),0);
  for(size_t i=0;i<members.size();++i){
    budget.step(members[i].size());
    std::array<double,2> sum{};
    for(auto c:members[i]){
      auto v=input.indices[c];auto uv=input.uv[v];sum[0]+=uv[0];sum[1]+=uv[1];
      if(!input.uv_pins.empty()&&input.uv_pins[v]){
        if(fixed[i]&&solved[i]!=uv)throw std::runtime_error("Stitch joins incompatible hard UV pins; unpin or align those endpoints first");
        solved[i]=uv;fixed[i]=1;
      }
    }
    if(!fixed[i])solved[i]={float(sum[0]/members[i].size()),float(sum[1]/members[i].size())};
  }
  std::vector<int> free_id(representative.size(),-1);
  for(const auto &[chart,chart_list]:chart_faces){
    if(!selected.empty()&&!selected.contains(chart))continue;
    budget.step();
    std::set<uint32_t> variable_ids,boundary;
    for(auto f:chart_list)for(uint32_t k=0;k<3;++k){
      budget.step();auto c=f*3+k;variable_ids.insert(corner_fan[c]);
      if(!internal[c]){boundary.insert(corner_fan[c]);boundary.insert(corner_fan[next(c)]);}
    }
    if(boundary.size()<2)throw std::runtime_error("UV chart "+std::to_string(chart)+" is closed; mark seams to open the surface before conformal unwrap");
    UvChartSolve report;report.first_face=chart;report.faces=chart_list.size();report.vertices=variable_ids.size();
    std::vector<uint32_t> anchors;
    for(auto id:variable_ids)if(fixed[id]){anchors.push_back(id);++report.hard_pins;}
    auto farthest=[&](uint32_t from){
      uint32_t best=*boundary.begin();double best_distance=-1;
      for(auto v:boundary){budget.step();double d=distance(input.positions[representative[from]],input.positions[representative[v]]);if(d>best_distance){best=v;best_distance=d;}}
      return best;
    };
    if(anchors.empty()){
      auto a=farthest(*boundary.begin()),b=farthest(a);anchors={a,b};
      fixed[a]=fixed[b]=1;report.automatic_pins=2;
      if(solved[a]==solved[b]){solved[a]={0,0};solved[b]={1,0};}
    }else if(anchors.size()==1){
      auto b=farthest(anchors[0]);anchors.push_back(b);fixed[b]=1;report.automatic_pins=1;
      if(solved[anchors[0]]==solved[b])solved[b]={solved[b][0]+1,solved[b][1]};
    }
    bool distinct=false;
    for(auto a:anchors)if(solved[a]!=solved[anchors[0]])distinct=true;
    if(!distinct)throw std::runtime_error("UV hard pins collapse chart scale; give two pins distinct UV coordinates");
    uint32_t count=0;
    for(auto v:variable_ids)if(!fixed[v]){free_id[v]=int(count);count+=2;}
    std::vector<Row> rows;rows.reserve(chart_list.size()*2);
    for(auto f:chart_list){
      budget.step(12);
      auto p=input.positions[input.indices[f*3]],q=input.positions[input.indices[f*3+1]],r=input.positions[input.indices[f*3+2]];
      double ax=double(q.x)-p.x,ay=double(q.y)-p.y,az=double(q.z)-p.z,
             bx=double(r.x)-p.x,by=double(r.y)-p.y,bz=double(r.z)-p.z;
      double length=std::sqrt(ax*ax+ay*ay+az*az),x=(ax*bx+ay*by+az*bz)/length;
      double square=bx*bx+by*by+bz*bz-x*x,y=std::sqrt(std::max(0.0,square));
      if(!std::isfinite(y)||length<=0||y<=length*1e-12)
        throw std::runtime_error("UV triangle "+std::to_string(f)+" is geometrically degenerate/ill-conditioned");
      std::array<double,3> gx{-y,y,0},gy{x-length,-x,length};
      double scale=1/std::sqrt(length*y);
      for(int equation=0;equation<2;++equation){
        Row row;
        for(int k=0;k<3;++k){
          auto v=corner_fan[f*3+k];
          for(int component=0;component<2;++component){
            double coefficient=(equation==0?(component==0?gx[k]:-gy[k]):(component==0?gy[k]:gx[k]))*scale;
            if(fixed[v])row.rhs-=coefficient*solved[v][component];
            else{row.indices[row.count]=uint32_t(free_id[v])+component;row.coefficients[row.count++]=coefficient;}
          }
        }
        rows.push_back(row);
      }
    }
    if(count){
      std::vector<double> x(count),rhs(count,0),diagonal(count,0),residual(count),z(count),direction(count),product(count);
      for(auto v:variable_ids)if(!fixed[v]){x[size_t(free_id[v])]=solved[v][0];x[size_t(free_id[v])+1]=solved[v][1];}
      for(const auto &row:rows)for(uint32_t j=0;j<row.count;++j){auto id=row.indices[j];double a=row.coefficients[j];rhs[id]+=a*row.rhs;diagonal[id]+=a*a;}
      auto multiply=[&](const std::vector<double>&in,std::vector<double>&out){
        std::fill(out.begin(),out.end(),0);
        for(const auto &row:rows){budget.step(row.count*2);double dot=0;for(uint32_t k=0;k<row.count;++k)dot+=row.coefficients[k]*in[row.indices[k]];for(uint32_t k=0;k<row.count;++k)out[row.indices[k]]+=row.coefficients[k]*dot;}
      };
      multiply(x,product);double rhs_norm=0,rz=0;
      for(uint32_t i=0;i<count;++i){
        if(!std::isfinite(diagonal[i])||diagonal[i]<=1e-30)throw std::runtime_error("UV chart has singular variables; add valid cuts/pins");
        residual[i]=rhs[i]-product[i];z[i]=residual[i]/diagonal[i];direction[i]=z[i];rz+=residual[i]*z[i];rhs_norm+=rhs[i]*rhs[i];
      }
      double denominator=std::max(std::sqrt(rhs_norm),1e-20);
      auto relative=[&](){double sum=0;for(double v:residual)sum+=v*v;return std::sqrt(sum)/denominator;};
      report.relative_residual=relative();
      while(report.relative_residual>options.relative_tolerance&&report.iterations<options.maximum_iterations){
        multiply(direction,product);double curvature=0;for(uint32_t i=0;i<count;++i)curvature+=direction[i]*product[i];
        if(!std::isfinite(curvature)||curvature<=0||!std::isfinite(rz)||rz<=0)throw std::runtime_error("UV chart solver lost rank; revise seams or pins");
        double alpha=rz/curvature,next_rz=0;
        for(uint32_t i=0;i<count;++i){x[i]+=alpha*direction[i];residual[i]-=alpha*product[i];z[i]=residual[i]/diagonal[i];next_rz+=residual[i]*z[i];}
        double beta=next_rz/rz;for(uint32_t i=0;i<count;++i)direction[i]=z[i]+beta*direction[i];rz=next_rz;
        ++report.iterations;report.relative_residual=relative();
      }
      // Check actual normal-equation residual, not only recursively updated CG.
      multiply(x,product);for(uint32_t i=0;i<count;++i)residual[i]=rhs[i]-product[i];
      report.relative_residual=relative();
      if(!std::isfinite(report.relative_residual)||report.relative_residual>options.relative_tolerance*4)
        throw std::runtime_error("UV chart "+std::to_string(chart)+" failed convergence; increase bounded iterations or revise cuts/pins");
      for(auto v:variable_ids)if(!fixed[v])solved[v]={float(x[size_t(free_id[v])]),float(x[size_t(free_id[v])+1])};
    }
    int winding=0;
    for(auto f:chart_list){
      auto a=solved[corner_fan[f*3]],b=solved[corner_fan[f*3+1]],c=solved[corner_fan[f*3+2]];
      double area=(double(b[0])-a[0])*(double(c[1])-a[1])-(double(b[1])-a[1])*(double(c[0])-a[0]);
      double ab0=double(b[0])-a[0],ab1=double(b[1])-a[1],
             ac0=double(c[0])-a[0],ac1=double(c[1])-a[1];
      double scale=std::max(ab0*ab0+ab1*ab1,ac0*ac0+ac1*ac1);
      if(!std::isfinite(area)||scale<=0||std::abs(area)<=scale*1e-12)
        throw std::runtime_error("UV chart collapsed; revise conflicting pins or add seams");
      int sign=area>0?1:-1;if(winding&&sign!=winding)throw std::runtime_error("UV chart folds across triangles; revise pins/cuts before publication");winding=sign;
    }
    result.charts.push_back(report);
  }
  result.source_vertices.resize(input.positions.size());std::iota(result.source_vertices.begin(),result.source_vertices.end(),0);
  std::map<std::pair<uint32_t,uint32_t>,uint32_t> output_vertices;
  std::vector<uint8_t> used(input.positions.size(),0);
  for(uint32_t c=0;c<corners;++c){
    budget.step();auto source=input.indices[c],fan=corner_fan[c];
    auto key=std::pair{source,fan};auto found=output_vertices.find(key);uint32_t out;
    if(found!=output_vertices.end())out=found->second;
    else{
      if(!used[source]){out=source;used[source]=1;}
      else{
        if(result.mesh.positions.size()>=options.maximum_vertices)throw std::runtime_error("UV seam split exceeds output vertex budget");
        out=result.mesh.positions.size();result.source_vertices.push_back(source);
        result.mesh.positions.push_back(input.positions[source]);result.mesh.normals.push_back(input.normals[source]);result.mesh.uv.push_back(input.uv[source]);
        if(!input.uv_pins.empty())result.mesh.uv_pins.push_back(input.uv_pins[source]);
      }
      output_vertices.emplace(key,out);
    }
    auto chart=regions.root(c/3);
    result.mesh.uv[out]=selected.empty()||selected.contains(chart)
        ? solved[fan] : input.uv[source];
    result.mesh.indices[c]=out;
  }
  result.corner_vertices=result.mesh.indices;result.mesh.validate();budget.step();return result;
}
} // namespace
UvSolveResult conformal_uv(const Mesh &mesh,const UvSolveOptions &options,const std::function<bool()> &cancelled){return run(mesh,{},0,options,cancelled);}
UvSolveResult stitch_uv(const Mesh &mesh,std::span<const UvStitchEdge> edges,double tolerance,const UvSolveOptions &options,const std::function<bool()> &cancelled){
  if(edges.empty())throw std::invalid_argument("Select at least one explicit seam edge pair to stitch");
  return run(mesh,edges,tolerance,options,cancelled);
}
} // namespace dome
