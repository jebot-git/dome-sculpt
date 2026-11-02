// SPDX-License-Identifier: GPL-3.0-or-later
#include "prepared-normal-shapes-fixtures.hpp"
#include "dome/document.hpp"
#include <iostream>

int main(int argc, char **argv) try {
  if (argc != 3) return 2;
  if (std::string_view(argv[1]) == "--inspect") {
    std::cout << dome::Scene::load(argv[2]).encode().dump() << '\n';
    return 0;
  }
  if (std::string_view(argv[1]) != "--ui-fixture") return 2;
  dome::Document document;
  document.replace(dome::prepared_shape_fixture::model(true));
  document.begin_uv();
  if (!document.wait_uv()) throw std::runtime_error("Fixture UV initialization timeout");
  document.edit_uv(dome::UvAction::Scale, .7f);
  if (!document.wait_uv()) throw std::runtime_error("Fixture UV scale timeout");
  // The real native Unwrap control needs a nonconformal starting chart;
  // uniformly scaling an already conformal square is an intentional no-op.
  document.edit_uv(dome::UvAction::VertexMode);
  document.edit_uv(dome::UvAction::Pick, .85f, .85f);
  document.edit_uv(dome::UvAction::Move, -.1f, -.03f);
  if (!document.wait_uv()) throw std::runtime_error("Fixture UV skew timeout");
  document.end_uv(true);
  if (!document.wait_asset() || !document.wait_mesh() ||
      !document.frame().state->error.empty())
    throw std::runtime_error("Fixture UV transfer: " + document.frame().state->error);
  document.save(argv[2]);
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
