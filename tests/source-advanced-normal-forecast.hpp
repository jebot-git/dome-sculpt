// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// CPU presentation forecasts only. They never replace the independent normal
// decoder or actual correct/counterfactual GL comparisons in --advanced-render.
namespace advanced_render::forecast {
using Q=std::array<double,4>;
void require(bool condition,const char *message){if(!condition)throw std::runtime_error(message);}
Q image(const Image &im,std::array<float,2> uv,bool srgb,int ws=33071,int wt=33071,int filter=9729) {
    im.validate();require(im.width&&im.height,"Forecast samples a real nonempty image");
    require(filter==9728||filter==9729,"Constant-UV ordinary control uses declared magnification filter");
    auto index=[](int q,int n,int wrap) {
        if(wrap==10497)return (q%n+n)%n;
        if(wrap==33648){q=(q%(n*2)+n*2)%(n*2);return q<n?q:n*2-1-q;}
        require(wrap==33071,"Forecast texture uses supported declared wrap");return std::clamp(q,0,n-1);
    };
    auto texel=[&](int x,int y){Q q{};const auto at=(size_t(index(y,int(im.height),wt))*im.width+index(x,int(im.width),ws))*4;
        for(size_t c=0;c<4;++c){q[c]=im.rgba.at(at+c)/255.;if(srgb&&c<3)q[c]=q[c]<=.04045?q[c]/12.92:std::pow((q[c]+.055)/1.055,2.4);}return q;};
    if(filter==9728)return texel(int(std::floor(uv[0]*im.width)),int(std::floor(uv[1]*im.height)));
    const double x=double(uv[0])*im.width-.5,y=double(uv[1])*im.height-.5;const int ix=int(std::floor(x)),iy=int(std::floor(y));Q out{};
    for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){const auto q=texel(ix+dx,iy+dy);const auto w=(dx?x-ix:1-(x-ix))*(dy?y-iy:1-(y-iy));for(size_t c=0;c<4;++c)out[c]+=q[c]*w;}return out;
}
int encoded(double linear) {
    require(std::isfinite(linear),"Forecast RGB is finite");linear=std::max(0.,linear);
    return int(std::lround(std::clamp(linear<=.0031308?linear*12.92:1.055*std::pow(linear,1/2.4)-.055,0.,1.)*255));
}
struct Inputs {
    Q base{},orm{},glow{},coverage{},emission_coverage{},generic_color{},generic_emission{};
    MToonMaterial typed;
    MToonSamples samples;
    std::array<double,3> effective{},effective_emission{};
    bool alias{};
};
Inputs inputs(const SculptObject &o,std::array<float,2> uv) {
    // The exact ordinary point control deliberately has derived maps, no source
    // pack/overlay or normal authority, and constant UV (zero texture LOD).
    auto control=prepared_point(o,uv,{0,0,1},{},{1,0,0},{0,1,0},1,true);
    const auto &maps=*control->ranges.at(0).textures;
    require(!maps.source_only&&!maps.overlay&&!maps.normal_projection&&maps.normal.rgba.empty(),
          "Forecast describes exact unchanged ordinary point material sampling");
    require(!maps.color_projection&&maps.pbr_color.rgba.empty(),"Forecast has no silently substituted prepared color plane");
    require(!o.material.unlit&&!o.material.toon,"Forecast explicitly models lit CorePBR rather than unlit or generic toon");
    Inputs in;in.base=image(maps.color,uv,true);in.orm=image(maps.metal_rough,uv,false);in.glow=image(maps.emission,uv,true);
    in.coverage=image(maps.color_coverage,uv,false);in.emission_coverage=image(maps.emission_coverage,uv,false);
    auto generic=[&](const PaintSourceTexture &r){
        if(!r.image)return Q{1,1,1,1};
        require(r.texcoord==0,"Forecast generic correction source uses its actual UV0");
        const auto &t=r.uv_transform;const std::array<float,2> q{t[0]*uv[0]+t[1]*uv[1]+t[4],t[2]*uv[0]+t[3]*uv[1]+t[5]};
        return image(*r.image,q,true,r.wrap_s,r.wrap_t,r.mag_filter?r.mag_filter:9729);
    };
    const Q generic_color=maps.source_maps?generic(maps.source_maps->color):Q{1,1,1,1};
    const Q generic_emission=maps.source_maps?generic(maps.source_maps->emission):Q{1,1,1,1};
    in.generic_color=generic_color;in.generic_emission=generic_emission;
    const auto &binding=*o.material_bindings;in.typed=binding.material;
    auto role=[&](MToonSlot slot){const auto at=size_t(slot);const auto &t=binding.material.textures[at];const auto &r=binding.roles[at];
        if(t.index<0||!r.image){const auto d=mtoon_default_texel(slot);return Q{d[0],d[1],d[2],d[3]};}
        require(t.texcoord==0&&t.transform_texcoord<=0,"Forecast ordinary material role uses its real current UV0");
        const auto q=mtoon_uv(binding.material,slot,uv,0);
        return image(*r.image,q,t.encoding==MToonEncoding::Srgb,r.wrap_s,r.wrap_t,r.mag_filter?r.mag_filter:9729);};
    const auto original=role(MToonSlot::Base),shade=role(MToonSlot::Shade),emission=role(MToonSlot::Emission);
    const auto &a=binding.roles[0],&b=binding.roles[1];const auto &ta=binding.material.textures[0],&tb=binding.material.textures[1];
    in.alias=a.image&&a.image==b.image&&a.wrap_s==b.wrap_s&&a.wrap_t==b.wrap_t&&a.min_filter==b.min_filter&&a.mag_filter==b.mag_filter&&
        ta.texcoord==tb.texcoord&&(binding.material.version()==MToonVersion::V0||(ta.offset==tb.offset&&ta.scale==tb.scale&&ta.rotation==tb.rotation));
    std::array<float,3> shade_sample{},emission_sample{};
    for(size_t c=0;c<3;++c){in.effective[c]=mtoon_factor(binding.material.parameters.base_color[c],binding.native_reference.color[c],o.material.color[c]);
        const auto authored=std::array<float,3>{binding.material.parameters.emission.x,binding.material.parameters.emission.y,binding.material.parameters.emission.z};
        in.effective_emission[c]=mtoon_factor(authored[c],binding.native_reference.emission[c],o.material.emissive[c]);
        const auto paint=in.coverage[0]==0?0.:std::max(0.,in.base[c]-double(o.material.color[c])*generic_color[c]*(1-in.coverage[0]));
        in.samples.base[c]=float(in.coverage[0]==0?in.effective[c]*original[c]:std::max(0.,in.base[c]+(in.effective[c]*original[c]-o.material.color[c]*generic_color[c])*(1-in.coverage[0])));
        const double shade_value=in.alias?std::max(0.,shade[c]*(1-in.coverage[0])+(o.material.color[c]!=0?paint/o.material.color[c]:paint)):shade[c];
        shade_sample[c]=float(shade_value);
        emission_sample[c]=float(in.emission_coverage[0]==0?in.effective_emission[c]*emission[c]:std::max(0.,in.glow[c]+(in.effective_emission[c]*emission[c]-o.material.emissive[c]*generic_emission[c])*(1-in.emission_coverage[0])));
    }
    in.samples.shade={shade_sample[0],shade_sample[1],shade_sample[2]};in.samples.emission={emission_sample[0],emission_sample[1],emission_sample[2]};
    in.samples.base[3]=float(std::clamp(in.base[3]+(mtoon_factor(binding.material.parameters.base_color[3],binding.native_reference.color[3],o.material.color[3])*original[3]-o.material.color[3]*generic_color[3])*(1-in.coverage[0]),0.,1.));
    require(!binding.roles[size_t(MToonSlot::Matcap)].image,"Forecast does not silently replace a direction-dependent matcap texture");
    require(binding.material.parameters.scroll==std::array<float,2>{}&&binding.material.parameters.rotation_speed==0,"Forecast declares actual nonanimated material coordinates");
    const auto rim=role(MToonSlot::Rim),matcap=role(MToonSlot::Matcap);
    in.samples.rim={float(rim[0]),float(rim[1]),float(rim[2])};in.samples.matcap={float(matcap[0]),float(matcap[1]),float(matcap[2])};
    // This fixture's additional scalar/light-animation slots must remain absent;
    // refuse an unsupported presentation rather than invent neutral samples.
    for(size_t slot=6;slot<binding.roles.size();++slot)require(!binding.roles[slot].image,"Forecast declares no unmodeled scalar/animated MToon texture");
    in.typed.parameters.base_color={1,1,1,1};in.typed.parameters.emission={1,1,1};
    return in;
}
std::array<int,3> pbr(const Inputs &in,D normal,D view) {
    // General GGX: actual sampled base, metallic, roughness, AO and emission.
    // Derived preview maps make uniforms white/one; never assume rough=1 or
    // neutral tint. Decode sRGB before bilinear interpolation, as the GL texture.
    constexpr double pi=3.141592653589793;auto dp=[](D a,D b){return std::inner_product(a.begin(),a.end(),b.begin(),0.);};
    const double metal=in.orm[2],rough=std::max(.04,in.orm[1]),a=rough*rough,a2=a*a,k=(rough+1)*(rough+1)/8;
    std::array<double,3> linear{};for(size_t c=0;c<3;++c)linear[c]=.045*in.base[c]*in.orm[0]+in.glow[c];
    const std::array<D,2> lights{unit(D{-.4,.8,.7}),unit(D{.7,.2,-.4})};const std::array<D,2> powers{D{3.8,3.8,3.8},D{.65,.8,1}};
    for(size_t l=0;l<2;++l){const auto h=unit(add(view,lights[l]));const auto nv=std::max(dp(normal,view),.0001),nl=std::max(dp(normal,lights[l]),0.),nh=std::max(dp(normal,h),0.),vh=std::max(dp(view,h),0.);
        const auto distribution=a2/(pi*std::pow(nh*nh*(a2-1)+1,2)),geometry=nv/(nv*(1-k)+k)*nl/(nl*(1-k)+k);
        for(size_t c=0;c<3;++c){const auto f0=.04*(1-metal)+in.base[c]*metal,f=f0+(1-f0)*std::pow(1-vh,5);
            linear[c]+=((1-f)*(1-metal)*in.base[c]/pi+distribution*geometry*f/std::max(4*nv*nl,.0001))*powers[l][c]*nl;}}
    std::array<int,3> out{};for(size_t c=0;c<3;++c)out[c]=encoded(linear[c]/(1+linear[c]));return out;
}
std::array<int,3> toon(const Inputs &in,D normal,D view) {
    // Presentation estimate uses the existing CPU MToon reference evaluator;
    // expected normal directions remain the independent advanced decoder.
    MToonLighting light;light.normal=prepared_pixel_oracle::vector(normal);light.view=prepared_pixel_oracle::vector(view);
    light.light_direction=normalized(Vec3{-.4f,.8f,.7f});light.light_color={1,1,1};light.gi_up={.25f,.25f,.25f};light.gi_down={.1f,.1f,.1f};
    light.gi=light.gi_down*(.5f-.5f*light.normal.y)+light.gi_up*(.5f+.5f*light.normal.y);light.normal_oriented=true;
    const auto f=mtoon_shade(in.typed,in.samples,light);return {encoded(f.color[0]),encoded(f.color[1]),encoded(f.color[2])};
}
int run(const std::filesystem::path &output,bool display_mode=false) {
    std::ofstream stream(output);require(bool(stream),"CPU forecast creates explicit output");unsigned views=0;
    auto family=[&](Case f){for(bool source_view:{true,false})for(bool pbr_role:{true,false}){
        const auto drawn=source_view?f.source:f.destination;auto shown=display_mode?display::scene(f.scene,drawn,pbr_role):f.scene.snapshot();
        const auto &o=shown.object(drawn);const auto &source=f.scene.object(f.source);const auto before=f.scene.encode();
        const auto mesh=o.deformation?o.deformation->preview(o.painted->mesh()):o.painted->mesh();
        const auto a=mesh.positions[mesh.indices[0]],ex=mesh.positions[mesh.indices[1]]-a,ey=mesh.positions[mesh.indices[2]]-a;
        const auto center=a+(ex+ey)/3,n=normalized(cross(ex,ey)),x=normalized(ex),y=normalized(cross(n,x)),camera=center+n*2;
        const float span=std::max({length(ex),length(ey),length(ey-ex)})*.85f;SurfaceQuery visibility(mesh);
        const std::string label=f.name+(source_view?"-source":"-target")+(pbr_role?"-pbr":"-mtoon");
        std::array<int,3> maxima{};std::array<std::array<int,2>,3> witnesses{};unsigned points=0;
        for(int py=0;py<96;++py)for(int px=0;px<96;++px){const auto position=center+x*((px+.5f)/96*2-1)*span+y*((py+.5f)/96*2-1)*span,r=position-a;
            const double d00=dot(ex,ex),d01=dot(ex,ey),d11=dot(ey,ey),den=d00*d11-d01*d01;
            const float v=float((d11*dot(r,ex)-d01*dot(r,ey))/den),w=float((d00*dot(r,ey)-d01*dot(r,ex))/den);const std::array<float,3>bary{1-v-w,v,w};
            if(*std::min_element(bary.begin(),bary.end())<=.05f)continue;const auto hit=visibility.raycast(position+n*2,n*-1,4);if(!hit||hit->triangle!=0)continue;
            std::array<float,2> uv{};for(size_t k=0;k<3;++k)for(size_t axis=0;axis<2;++axis)uv[axis]+=o.painted->mesh().uv[mesh.indices[k]][axis]*bary[k];
            const auto in=inputs(o,uv);const auto view=unit(vc(camera-position));const size_t role=pbr_role?0:1;
            const std::array<D,4> normals{advanced_fixture::decode(source,0,bary,role),advanced_fixture::decode(source,0,bary,role,true),advanced_fixture::decode(source,0,bary,role,false,true),advanced_fixture::decode(source,0,bary,1-role)};
            const auto target_normal=source_view?normals[0]:prepared_pixel_oracle::direction(o,0,bary,role);
            double transfer_error=0;for(size_t c=0;c<3;++c)transfer_error=std::max(transfer_error,std::abs(target_normal[c]-normals[0][c]));
            std::array<std::array<int,3>,4> rgb;for(size_t k=0;k<4;++k){prepared_pixel_oracle::finite_sample(normals[k]);rgb[k]=pbr_role?pbr(in,normals[k],view):toon(in,normals[k],view);}
            std::array<int,3> gaps{};for(size_t k=0;k<3;++k){for(size_t c=0;c<3;++c)gaps[k]=std::max(gaps[k],std::abs(rgb[0][c]-rgb[k+1][c]));if(gaps[k]>maxima[k]){maxima[k]=gaps[k];witnesses[k]={px,py};}}
            const auto xy=std::array<int,2>{px,py};const bool original=std::find(original_render_probes.begin(),original_render_probes.end(),xy)!=original_render_probes.end();
            stream<<nlohmann::json{{"kind","point"},{"label",label},{"xy",xy},{"original",original},{"bary",bary},{"UV",uv},{"frontmost_triangle",hit->triangle},
                {"correct_and3negative_normals",normals},{"independent_target_normal",target_normal},{"independent_transfer_error",transfer_error},{"correct_and3negative_RGB",rgb},{"negative_gaps",gaps},{"actual_filtered_linear_base",in.base},
                {"actual_filtered_data_AO_rough_metal",in.orm},{"actual_filtered_linear_emission",in.glow},{"actual_filtered_generic_color",in.generic_color},{"actual_filtered_generic_emission",in.generic_emission},{"generic_current_UV0_domain_authenticated",true},{"native_material_color",o.material.color},
                {"native_material_roughness",o.material.roughness},{"native_material_metallic",o.material.metallic},{"display_mode",display_mode},{"declared_shade",std::array<float,3>{o.material_bindings->material.parameters.shade_color.x,o.material_bindings->material.parameters.shade_color.y,o.material_bindings->material.parameters.shade_color.z}},{"declared_toony",o.material_bindings->material.parameters.toony}}.dump()<<'\n';++points;
        }
        const auto projection=o.painted->normal_projection();const auto preview=o.painted->preview(o.material);
        nlohmann::json normal_metadata{{"source_view",source_view},{"projection_version",projection?projection->version:0},{"retained_projection_domain",projection&&bool(projection->sampling_domain)},{"retained_binding_domain",o.material_bindings->normal_preparation&&bool(o.material_bindings->normal_preparation->sampling_domain)},{"normal_image_size",std::array<uint32_t,2>{preview->normal.width,preview->normal.height}},{"mtoon_image_size",std::array<uint32_t,2>{preview->mtoon_normal.width,preview->mtoon_normal.height}},{"mesh_vertices",mesh.positions.size()},{"normal_scale",o.material_bindings->material.parameters.normal_scale}};
        stream<<nlohmann::json{{"kind","view"},{"normal_metadata",normal_metadata},{"label",label},{"points",points},{"maximum_gaps",maxima},{"witnesses",witnesses},{"PBR_model","independent GGX with genuine filtered material maps"},{"MToon_model","CPU reference presentation estimate; not graphical acceptance"},{"required_gap_unchanged",7},{"original_probes_unchanged",original_render_probes}}.dump()<<std::endl;
        require(f.scene.encode()==before,"CPU contrast forecast retains complete genuine scene authority");++views;
    }};
    for(bool legacy:{false,true})for(bool live:{false,true})for(bool seam:{false,true})for(auto method:{SourceNormalUvMethod::Rasterize,SourceNormalUvMethod::RetainSource})family(construct(legacy,live,seam,method));
    family(construct(false,true,true,SourceNormalUvMethod::RetainSource,true));require(views==68,"CPU forecast visits all genuine family/stage/role views");
    std::cout<<(display_mode?"PASS CPU advanced display forecast views=":"PASS CPU advanced contrast forecast views=")<<views<<"; no GL context or normal/material mutation\n";return 0;
}
} // namespace advanced_render::forecast
