#include "dome/document.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
void require(bool v, const char *message) {
  if (!v)
    throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
int main() try {
  Volume v;
  v.begin_stroke();
  Brush brush;
  brush.radius = .25f;
  v.stamp(brush);
  v.end_stroke();
  auto base = v.voxels();
  auto baseid = v.active_layer();
  auto detail = v.add_layer("Detail");
  v.begin_stroke();
  brush.tool = Tool::Carve;
  brush.from = brush.to = {0, 0, .19f};
  brush.radius = .14f;
  v.stamp(brush);
  v.end_stroke();
  auto edited = v.voxels();
  require(edited != base, "An independent detail layer changes the field");
  require(v.undo() && v.voxels() == base && v.redo() && v.voxels() == edited,
          "Layer stroke undo and redo restore exact field values");
  v.begin_stroke();
  brush.from = brush.to = {.15f, 0, 0};
  v.stamp(brush);
  v.cancel_stroke();
  require(v.voxels() == edited,
          "Cancelled layered strokes restore exact cached samples");
  auto immutable = v.geometry_snapshot();
  v.layer_properties(detail, "Detail", false, false, 1);
  require(v.voxels() == base,
          "Hiding a detail layer restores the base exactly");
  require(immutable.voxels() == edited && immutable.layers()[1].visible,
          "Published COW field and layer snapshots stay immutable");
  bool blocked = false;
  try {
    v.begin_stroke();
  } catch (const std::logic_error &) {
    blocked = true;
  }
  require(blocked, "Hidden sculpt layers reject strokes");
  v.layer_properties(detail, "Detail", true, true, 1);
  require(v.voxels() == edited && !v.editable_layer(),
          "Locked layers remain visible but uneditable");
  v.layer_properties(detail, "Detail", true, false, 0);
  require(v.voxels() == base && !v.editable_layer(),
          "Zero-strength layers are nondestructive and uneditable");
  v.layer_properties(detail, "Detail", true, false, .5);
  auto halfway = v.voxels();
  require(halfway != base && halfway != edited,
          "Layer strength blends sculpt displacement");
  auto copy = Volume::decode(v.encode());
  require(copy.voxels() == halfway && copy.layers().size() == 2 &&
              copy.active_layer() == detail,
          "Layer persistence restores composition, IDs and selection");
  copy.layer_properties(detail, "Detail", true, false, 1);
  require(copy.voxels() == edited,
          "Strength remains editable after reopening a layer project");
  Volume::Voxels supplied;
  supplied[{0, 0, 0}] = 0;
  supplied[{1, 0, 0}] = .12f;
  auto from = Volume::from_samples(.03f, std::move(supplied));
  require(Volume::decode(from.encode()).voxels() == from.voxels(),
          "Imported field samples initialize canonical editable layers");
  auto legacy = v.encode();
  legacy["version"] = 1;
  legacy.erase("sculpt_layers");
  legacy.erase("active_sculpt_layer");
  legacy.erase("next_sculpt_layer");
  auto imported = Volume::decode(legacy);
  require(imported.layers().size() == 1 && imported.voxels() == halfway,
          "Legacy flat volumes load as one editable base layer");
  auto bad = v.encode();
  bad["sculpt_layers"][1]["id"] = baseid;
  blocked = false;
  try {
    Volume::decode(bad);
  } catch (const std::exception &) {
    blocked = true;
  }
  require(blocked, "Duplicate persisted sculpt IDs are rejected");
  copy.erase_layer(detail);
  require(copy.voxels() == base && copy.active_layer() == baseid,
          "Deleting a layer recomposes and retains the base");
  Document d;
  d.seed();
  auto original = d.copy_volume().voxels();
  d.sculpt_command(SpatialCommand::SculptAdd);
  require(d.frame().state->sculpt_layers.size() == 2,
          "Document creates persistent sculpt layers");
  require(d.begin_desktop(), "New layers acquire desktop sculpt strokes");
  brush.from = brush.to = {0, 0, .32f};
  d.stamp_desktop(brush);
  d.end_desktop();
  auto shape = d.copy_volume().voxels();
  require(shape != original,
          "Desktop brushes write to the selected sculpt layer");
  d.sculpt_command(SpatialCommand::SculptVisible);
  require(d.copy_volume().voxels() == original && !d.begin_desktop(),
          "Document visibility controls composition and brush permissions");
  require(d.undo() && d.copy_volume().voxels() == shape,
          "Global undo restores layer settings and sculpt geometry");
  require(d.undo() && d.copy_volume().voxels() == original,
          "Global undo reaches the original per-layer stroke history");
  require(d.undo() && d.frame().state->sculpt_layers.size() == 1,
          "Layer creation participates in document history");
  require(d.redo() && d.redo() && d.copy_volume().voxels() == shape,
          "Global redo restores layer creation and exact stroke geometry");
  auto s = d.copy_scene();
  auto loaded = Scene::decode(s.encode());
  require(loaded.active().volume->layers().size() == 2 &&
              loaded.active().volume->voxels() == shape,
          "Scene serialization preserves sculpt layer authoring data");
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
