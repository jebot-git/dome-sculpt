// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace advanced_render::display {
// This is an explicit display snapshot, after both genuine Bakes. Never change
// a published frame binding or its proof, or author this presentation into the
// family scene. Original normals/roles/recipes and every sampler stay owned.
Scene scene(const Scene &authentic,uint64_t drawn,bool pbr) {
    const auto before=authentic.encode();const auto original_paint=authentic.object(drawn).painted;
    auto shown=authentic.snapshot();const auto detached_paint=shown.object(drawn).painted;
    if(!pbr){
        const auto original=authentic.object(drawn).material_bindings;
        auto binding=std::make_shared<SourceMaterialBindings>(*original);
        binding->material.parameters.shade_color={.015f,.020f,.025f};
        binding->material.parameters.toony=0;
        for(size_t role=0;role<binding->roles.size();++role){const auto &a=binding->roles[role],&b=original->roles[role];
            check(a.image==b.image&&a.wrap_s==b.wrap_s&&a.wrap_t==b.wrap_t&&a.min_filter==b.min_filter&&a.mag_filter==b.mag_filter&&a.texcoord==b.texcoord&&a.uv_transform==b.uv_transform,
                  "Display snapshot preserves every exact role image and declared source sampler");}
        check(binding->normal_preparation==original->normal_preparation&&binding->normal_frames==original->normal_frames&&
              binding->uv_origin==original->uv_origin&&binding->key_derivation==original->key_derivation&&
              binding->material.parameters.normal_scale==original->material.parameters.normal_scale,
              "Display-only shade/toony keep exact normal recipe/frame/UV/key owners and scale");
        binding->validate();shown.object(drawn).material_bindings=std::move(binding);
        auto restored=shown.snapshot();restored.object(drawn).material_bindings=original;
        check(restored.encode()==before,"Restoring one display binding recovers exact complete genuine scene");
    }
    check(authentic.encode()==before&&authentic.object(drawn).painted==original_paint&&shown.object(drawn).painted==detached_paint&&
          shown.object(drawn).appearance==authentic.object(drawn).appearance&&shown.object(drawn).deformation==authentic.object(drawn).deformation,
          "Display leaves original scene owners and its independently detached paint wrapper unchanged");
    check(detached_paint->mesh_owner()==original_paint->mesh_owner()&&detached_paint->source_maps()==original_paint->source_maps()&&
          detached_paint->normal_projection()==original_paint->normal_projection()&&detached_paint->color_projection()==original_paint->color_projection()&&
          detached_paint->baked()==original_paint->baked(),"Display snapshot shares exact immutable geometry, sampling, projections and baked authority");
    const auto &a=detached_paint->layers(),&b=original_paint->layers();check(a.size()==b.size(),"Display retains all real paint layers");
    for(size_t layer=0;layer<a.size();++layer)check(a[layer].id==b[layer].id&&a[layer].name==b[layer].name&&a[layer].visible==b[layer].visible&&
        a[layer].opacity==b[layer].opacity&&a[layer].tiles==b[layer].tiles&&a[layer].pbr_tiles==b[layer].pbr_tiles&&a[layer].emission_tiles==b[layer].emission_tiles,
        "Detached display layers retain exact common, PBR and emission tile owners");
    return shown;
}
int execute(const std::filesystem::path &out,const std::filesystem::path &witness_file) {
    std::ifstream input(witness_file);const auto manifest=nlohmann::json::parse(input);
    check(manifest.at("kind")=="advanced-normal-display-witnesses-v1"&&manifest.at("views").size()==68,
          "Display graphics requires an explicit reviewed 68-view forecast witness manifest");
    graphics_output=out;NativeGraphics graphics;std::cout<<"ACTUAL_RENDERER "<<glGetString(GL_RENDERER)<<std::endl;
    unsigned families=0,views=0,points=0;
    auto render=[&](Case family){const auto original=family.scene.encode();const auto paint=family.scene.object(family.source).painted;
        const auto binding=family.scene.object(family.source).material_bindings;
        for(bool source_view:{true,false})for(bool pbr:{true,false}){
            const auto label=family.name+(source_view?"-source":"-target")+(pbr?"-pbr":"-mtoon");
            const auto extra=manifest.at("views").at(label).at("extra_probes").get<std::vector<std::array<int,2>>>();
            check(!extra.empty()&&extra.size()<=3,"Each view has bounded additional genuine forecast witnesses");
            const auto drawn=source_view?family.source:family.destination;
            Case shown{scene(family.scene,drawn,pbr),family.source,family.destination,family.name};
            view(shown,source_view,pbr,extra,true);points+=unsigned(original_render_probes.size()+extra.size());++views;
        }
        check(family.scene.encode()==original&&family.scene.object(family.source).painted==paint&&family.scene.object(family.source).material_bindings==binding,
              "Full display matrix leaves exact authentic first/second Bake scene and source owners unchanged");++families;
    };
    for(bool legacy:{false,true})for(bool live:{false,true})for(bool seam:{false,true})for(auto method:{SourceNormalUvMethod::Rasterize,SourceNormalUvMethod::RetainSource})
        render(construct(legacy,live,seam,method));
    render(construct(false,true,true,SourceNormalUvMethod::RetainSource,true));
    check(families==17&&views==68,"Display qualification retains all genuine families, source/target and roles");
    std::cout<<"PASS "<<checks<<" advanced display source/second-Bake graphical controls families="<<families<<" views="<<views<<" points="<<points<<'\n';return 0;
}
} // namespace advanced_render::display
