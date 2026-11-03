// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene-file.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
int main(int argc,char **argv) try {
  if(argc!=2)return 2;
  std::cout<<dome::load_scene_file(argv[1]).scene.encode().dump()<<'\n';
  return 0;
} catch(const std::exception &error) {
  std::cerr<<error.what()<<'\n';return 1;
}
