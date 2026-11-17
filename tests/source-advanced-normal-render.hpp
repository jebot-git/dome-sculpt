// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Include after the ordinary prepared-render helpers. Reuse the exact genuine
// advanced fixture and independent decoder; do not include another test TU.
#include "dome/normal-sampling.hpp"
#include "dome/normal-preview.hpp"
#include "dome/source-uv-bake.hpp"
#include <numeric>
namespace advanced_render {
using source_artist_fixture::fixture;
using prepared_pixel_oracle::D;
using prepared_pixel_oracle::add;
using prepared_pixel_oracle::mul;
using prepared_pixel_oracle::unit;
using prepared_pixel_oracle::vc;
using prepared_pixel_oracle::crossd;
using prepared_pixel_oracle::sample;
std::shared_ptr<const SourceMaterialBindings> blank_typed(bool legacy) {
    auto binding=std::make_shared<SourceMaterialBindings>();
    binding->material=typed(legacy);
    binding->validate();
    return binding;
}
#include "source-advanced-normal-fixture.hpp"

struct Case {
    Scene scene;
    uint64_t source{}, destination{};
    std::string name;
};
Case construct(bool legacy,bool live,bool seam,SourceNormalUvMethod method,bool repeated=false) {
    const std::string name=std::string(legacy?"mt0":"mt1")+(live?"-posed":"-rest")+
        (seam?"-seam":"-shared")+(method==SourceNormalUvMethod::RetainSource?"-retained":"-raster")+
        (repeated?"-three-edits":"");
    std::cout<<"ADVANCED_GL_FAMILY "<<name<<std::endl;
    auto scene=advanced_fixture::uv_source(advanced_fixture::prepared(legacy,check),method,seam);
    if(repeated) {
        scene=advanced_fixture::uv_source(scene,method,seam);
        scene=advanced_fixture::uv_source(scene,method,seam);
    }
    advanced_fixture::pose(scene,live);
    const auto source=scene.active().id;
    const auto source_before=scene.encode();
    const auto paint=scene.active().painted;
    const auto binding=scene.active().material_bindings;
    auto mesh=scene.active().deformation ? scene.active().deformation->preview(paint->mesh()) : paint->mesh();
    if(repeated) for(size_t v=0;v<mesh.uv.size();++v)
        mesh.uv[v]={paint->mesh().positions[v].x+.5f,paint->mesh().positions[v].y+.5f};
    const auto destination=scene.add("Advanced graphical destination",Volume{});
    auto &target=scene.object(destination);
    target.mesh_active=true;
    target.painted=std::make_shared<PaintedMesh>(mesh,64);
    target.material_bindings=blank_typed(legacy);
    scene.select(destination);
    ObjectBakeSettings options;
    options.prepare_normals=true;options.normal_error=.03f;
    options.front_mm=options.back_mm=500;options.ao_samples=0;options.padding=0;
    check(bake_object(scene,source,options).projected>0,"Actual second Bake consumes genuine advanced source");
    check(scene.object(source).painted==paint && scene.object(source).material_bindings==binding,
          "Advanced graphical construction preserves exact source paint/material authority owners");
    auto remaining=scene.snapshot();remaining.erase(destination);remaining.select(source);
    auto context=remaining.encode();
    check(context.at("next_id").get<uint64_t>()==source_before.at("next_id").get<uint64_t>()+1,
          "Graphical destination consumes exactly one source-context allocator identity");
    context["next_id"]=source_before.at("next_id");
    check(context==source_before,"Advanced graphical second Bake preserves every genuine original/source context field");
    const auto encoded=scene.encode();
    check(Scene::decode(encoded).encode()==encoded,"Advanced graphical source and target retain exact native archive data");
    return {std::move(scene),source,destination,name};
}

void view(const Case &family,bool source_view,bool pbr,std::span<const std::array<int,2>> extra={},bool authenticate_display=false) {
    std::vector<std::array<int,2>> probes(original_render_probes.begin(),original_render_probes.end());
    for(const auto xy:extra){check(std::find(probes.begin(),probes.end(),xy)==probes.end(),"Display witnesses supplement rather than replace original probes");probes.push_back(xy);}
    auto scene=family.scene.snapshot();
    const auto drawn=source_view?family.source:family.destination;
    for(const auto &object:scene.objects()) scene.object(object.id).visible=object.id==drawn;
    scene.select(drawn);
    const auto before=scene.encode();
    const auto &object=scene.object(drawn);
    const auto mesh=object.deformation ? object.deformation->preview(object.painted->mesh()) : object.painted->mesh();
    constexpr uint32_t triangle=0;
    std::array<Vec3,3> points;
    for(size_t k=0;k<3;++k) points[k]=mesh.positions.at(mesh.indices.at(k));
    const auto ex=points[1]-points[0],ey=points[2]-points[0];
    const auto n=normalized(cross(ex,ey)),x=normalized(ex),y=normalized(cross(n,x));
    const auto center=(points[0]+points[1]+points[2])/3;
    const float span=std::max({length(ex),length(ey),length(points[2]-points[1])})*.85f;
    const auto matrix=face_projection(center,x,y,n,span);
    const auto camera=center+n*2;
    require_face_depth(matrix,center,n);
    require_visible_probes(mesh,triangle,center,x,y,n,span,probes);
    Document document;document.replace(scene.snapshot());
    if(pbr) document.shading(false); else document.authored_shading();
    check(document.wait_mesh() && document.frame().state->error.empty(),
          "Actual advanced source/target publishes through Document production render route");
    if(authenticate_display){
        bool found=false;
        for(const auto &range:document.frame().surface->ranges)if(range.object==drawn){
            found=true;check(range.material_bindings==object.material_bindings,"Display Document retains its declared material binding owner");
            const auto preparation=range.material_bindings->normal_preparation;
            if(preparation&&preparation->sampling_domain&&preparation->sampling_domain->has_sealed_metadata())
                check(range.normal_preview_proof&&range.normal_preview_proof->matches(range.material_bindings,range.textures),
                      "Document mints genuine owning normal preview proof for the display snapshot");
        }
        check(found,"Display snapshot reaches actual production surface range");
    }
    const std::string label=family.name+(source_view?"-source":"-target")+(pbr?"-pbr":"-mtoon");
    Fbo f;SurfaceRenderer renderer;renderer.set_frame_time(0);f.bind();
    const auto actual=draw_view(renderer,document.frame().surface,matrix,camera);
    retain_pixels(label+"-actual",actual);
    const size_t role=pbr?0:1;
    std::array<unsigned,3> sensitive{};
    int maximum=0;
    for(const auto xy:probes) {
        const auto position=center+x*((xy[0]+.5f)/96*2-1)*span+y*((xy[1]+.5f)/96*2-1)*span;
        const auto r=position-points[0];
        const double d00=dot(ex,ex),d01=dot(ex,ey),d11=dot(ey,ey),d20=dot(r,ex),d21=dot(r,ey);
        const double denominator=d00*d11-d01*d01;
        check(std::isfinite(denominator) && denominator>1e-20,"Advanced graphical probe has finite nonsingular barycentrics");
        const float v=float((d11*d20-d01*d21)/denominator),w=float((d00*d21-d01*d20)/denominator);
        const std::array<float,3> bary{1-v-w,v,w};
        check(*std::min_element(bary.begin(),bary.end())>.05f,"Original advanced graphical probe remains strictly interior");
        std::array<float,2> uv{};
        for(size_t k=0;k<3;++k) for(size_t axis=0;axis<2;++axis)
            uv[axis]+=object.painted->mesh().uv[mesh.indices[k]][axis]*bary[k];
        const auto &source=scene.object(family.source);
        const auto wanted=advanced_fixture::decode(source,triangle,bary,role);
        prepared_pixel_oracle::finite_sample(wanted);
        if(!source_view) {
            const auto decoded=prepared_pixel_oracle::direction(object,triangle,bary,role);
            double error=0;
            for(size_t axis=0;axis<3;++axis) error=std::max(error,std::abs(wanted[axis]-decoded[axis]));
            check(error<.035,"Independent advanced second-Bake direction retains unchanged .035 bound");
        }
        auto control=prepared_point(object,uv,prepared_pixel_oracle::vector(wanted),center,x,y,span,pbr);
        f.bind();const auto correct_pixels=draw_view(renderer,control,matrix,camera);
        const auto correct=at(correct_pixels,xy[0],xy[1]),got=at(actual,xy[0],xy[1]);
        const auto prefix=label+"-"+std::to_string(xy[0])+"-"+std::to_string(xy[1]);
        retain_pixels(prefix+"-correct",correct_pixels);
        int error=0;for(size_t axis=0;axis<3;++axis) error=std::max(error,std::abs(got[axis]-correct[axis]));
        maximum=std::max(maximum,error);
        const bool correct_match=close(got,correct,3);
        const std::array<D,3> negatives{
            advanced_fixture::decode(source,triangle,bary,role,true),
            advanced_fixture::decode(source,triangle,bary,role,false,true),
            advanced_fixture::decode(source,triangle,bary,1-role)};
        constexpr std::array<const char*,3> names{"wrong-domain","omitted","swapped-role"};
        std::cout<<"ADVANCED_GL_POINT label="<<label<<" xy="<<xy[0]<<','<<xy[1]
            <<" actual="<<got[0]<<','<<got[1]<<','<<got[2]
            <<" correct="<<correct[0]<<','<<correct[1]<<','<<correct[2]<<" error="<<error;
        for(size_t k=0;k<negatives.size();++k) {
            prepared_pixel_oracle::finite_sample(negatives[k]);
            auto negative=std::make_shared<MeshSnapshot>(*control);
            negative->mesh.normals.assign(4,prepared_pixel_oracle::vector(negatives[k]));
            f.bind();const auto pixels=draw_view(renderer,negative,matrix,camera);
            const auto bad=at(pixels,xy[0],xy[1]);retain_pixels(prefix+"-"+names[k],pixels);
            int gap=0;for(size_t axis=0;axis<3;++axis) gap=std::max(gap,std::abs(correct[axis]-bad[axis]));
            sensitive[k]+=gap>=7 && !close(got,bad,3) && !close(correct,bad,3);
            std::cout<<' '<<names[k]<<'='<<bad[0]<<','<<bad[1]<<','<<bad[2]<<" gap="<<gap;
        }
        std::cout<<std::endl;
        check(correct_match,"Actual advanced-role GL pixel agrees with independent ordinary-normal control at unchanged RGB3");
    }
    for(size_t k=0;k<sensitive.size();++k)
        check(sensitive[k]>0,"Each advanced view rejects wrong-domain, omitted and swapped controls with gap7/disjoint RGB3 intervals");
    check(document.copy_scene().encode()==before && scene.encode()==before,
          "Advanced graphical consumer preserves exact complete scene/source/material/normal authority");
    check(glGetError()==GL_NO_ERROR,"Advanced graphical controls leave actual GL error queue empty");
    std::cout<<"ADVANCED_GL_CASE label="<<label<<" correct_max="<<maximum
        <<" disjoint_wrong="<<sensitive[0]<<" omitted="<<sensitive[1]<<" swapped="<<sensitive[2]<<std::endl;
}
int execute(const std::filesystem::path &out) {
    graphics_output=out;
    NativeGraphics graphics;
    std::cout<<"ACTUAL_RENDERER "<<glGetString(GL_RENDERER)<<std::endl;
    unsigned families=0,views=0;
    auto render=[&](Case family) {
        for(bool source_view:{true,false}) for(bool pbr:{true,false}) {view(family,source_view,pbr);++views;}
        ++families;
    };
    for(bool legacy:{false,true}) for(bool live:{false,true}) for(bool seam:{false,true})
        for(auto method:{SourceNormalUvMethod::Rasterize,SourceNormalUvMethod::RetainSource})
            render(construct(legacy,live,seam,method));
    render(construct(false,true,true,SourceNormalUvMethod::RetainSource,true));
    check(families==17 && views==68,"All sixteen genuine families and the real three-edit family render both source/target roles");
    std::cout<<"PASS "<<checks<<" advanced native source/second-Bake graphical controls families="<<families<<" views="<<views<<'\n';
    return 0;
}
} // namespace advanced_render
