// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc, char **argv) try {
  if (argc != 2)
    throw std::invalid_argument("Usage: dome-sculpt-uv-benchmark painted.dsculpt");
  auto scene = Scene::load(argv[1]);
  if (!scene.active().painted)
    throw std::invalid_argument("The benchmark needs a prepared painting mesh");
  auto original = scene.active().painted->mesh().uv;
  size_t triangles = scene.active().painted->mesh().indices.size() / 3;
  Document doc;
  doc.replace(std::move(scene));
  doc.begin_uv();
  if (!doc.wait_uv() || !doc.frame().state->uv->valid)
    throw std::runtime_error("The baseline atlas is invalid or did not settle");
  doc.edit_uv(UvAction::Pick, .5f, .5f);
  auto before = doc.frame().state->uv;
  auto sample = before->mesh->indices[before->face * 3];
  doc.edit_uv(UvAction::BeginGesture);
  double total = 0, slowest = 0;
  constexpr int samples = 20;
  for (int i = 0; i < samples; ++i) {
    auto start = std::chrono::steady_clock::now();
    doc.edit_uv(UvAction::Move, .0001f, 0);
    double ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
    total += ms;
    slowest = std::max(slowest, ms);
  }
  doc.edit_uv(UvAction::EndGesture);
  auto settle_start = std::chrono::steady_clock::now();
  if (!doc.wait_uv())
    throw std::runtime_error("The final preview did not finish validation");
  double settle_ms = std::chrono::duration<double, std::milli>(
                         std::chrono::steady_clock::now() - settle_start).count();
  auto preview = doc.frame().state->uv;
  bool all_samples = std::abs(preview->mesh->uv[sample][0] -
                              (original[sample][0] + samples * .0001f)) < 1e-5f;
  doc.edit_uv(UvAction::Undo);
  if (!doc.wait_uv())
    throw std::runtime_error("Undo did not finish validation");
  bool exact = doc.frame().state->uv->mesh->uv == original &&
               !doc.frame().state->uv->can_undo && before->mesh->uv == original;
  std::cout << nlohmann::json{{"triangles", triangles}, {"samples", samples},
                            {"mean_edit_ms", total / samples}, {"max_edit_ms", slowest},
                            {"final_validation_wait_ms", settle_ms},
                            {"preview_valid", preview->valid}, {"all_samples", all_samples},
                            {"exact_undo", exact}}.dump(2) << '\n';
  return all_samples && exact ? 0 : 1;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
