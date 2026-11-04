// SPDX-License-Identifier: GPL-3.0-or-later
#define main unskinned_native_fixture_main
#include "rig-source-unskinned.cpp"
#undef main
int main(int argc,char **argv)try{
 if(argc!=3)throw std::invalid_argument("Choose --ui-fixture, --inspect or --expected and a project path");
 const std::string_view mode=argv[1];
 if(mode=="--ui-fixture"){changed_current(bind(fixture(true))).save(argv[2]);return 0;}
 if(mode=="--expected"){std::cout<<fixture(true).encode().dump();return 0;}
 if(mode=="--inspect"){auto s=Scene::load(argv[2]);std::cout<<nlohmann::json{{"scene",s.encode()},{"target_skinned",bool(s.object(2).deformation&&s.object(2).deformation->skeleton)}}.dump();return 0;}
 throw std::invalid_argument("Unknown original-source UI fixture mode");
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
