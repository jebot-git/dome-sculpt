// SPDX-License-Identifier: GPL-3.0-or-later
#include "post-uv-keys-fixtures.hpp"
#include <iostream>
int main(int argc, char **argv) try {
  if (argc != 3) return 2;
  if (std::string_view(argv[1]) == "--inspect") {
    std::cout << dome::Scene::load(argv[2]).encode().dump() << '\n';
    return 0;
  }
  if (std::string_view(argv[1]) != "--ui-fixture") return 2;
  dome::post_uv_key_fixture::dense_model().save(argv[2]);
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
