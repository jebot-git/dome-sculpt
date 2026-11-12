// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-artist-normal-fixture.hpp"
using namespace source_artist_fixture;
int main(int argc,char**argv)try{if(argc!=2)return 2;std::filesystem::create_directories(argv[1]);imported_artist(argv[1]);for(bool legacy:{false,true})for(bool posed:{false,true})run(legacy,posed,std::filesystem::path(argv[1])/(std::to_string(legacy)+std::to_string(posed)+".dsculpt"));std::cout<<"PASS "<<checks<<" artist source CPU controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
