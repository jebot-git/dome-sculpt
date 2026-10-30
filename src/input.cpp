// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/input.hpp"
#include "dome/uv.hpp"
namespace dome {
const std::array<const char *, 12> &spatial_labels(int page) {
  static const std::array<const char *, 12> root = {
      "Clay", "Carve",       "Smooth",  "Flatten",   "Undo",       "Redo",
      "Save", "Export glTF", "Objects", "Materials", "Brush size", "Flow"};
  static const std::array<const char *, 12> objects = {
      "New sphere", "New empty",  "Duplicate",  "Next",
      "Previous",   "Visibility", "Lock",       "Delete",
      "Back",       "Mesh tools", "Avatars",    "Sculpt layers"};
  static const std::array<const char *, 12> materials = {
      "Base color", "Blue",      "White",   "Paint tools",
      "Metallic",   "Roughness", "Opacity", "UV editor",
      "Back",       "Toon",      "PBR",     "Flow"};
  static const std::array<const char *, 12> paint = {
      "Make mesh",   "Edit source", "Paint color", "Paint metal",
      "Paint rough", "Paint glow",  "Mask",        "Add layer",
      "Back",        "Next layer",  "Erase",       "Controls"};
  static const std::array<const char *, 12> colors = {
      "Red",    "Green",  "Blue", "White",      "Black",        "Warm",
      "Flow +", "Flow -", "Back", "Show layer", "Delete layer", "Layer alpha"};
  static const std::array<const char *, 12> controls = {
      "Color",      "Metal value", "Rough value", "Emission",
      "Mask value", "Flow",        "Layer alpha", "Brush size",
      "Back",       "Presets",     "Erase",       "Layer order"};
  static const std::array<const char *, 12> sculpt = {
      "Add layer",  "Next layer", "Previous",     "Show layer",
      "Lock layer", "Strength",   "Delete layer", "Resolution",
      "Back",       "Procedural", "Recovery",     "Masks / symmetry"};
  static const std::array<const char *, 12> recovery = {
      "Refresh", "Next project", "Previous", "Recover", "Save", "Center",
      "Undo", "Redo", "Back", "Export glTF", "Objects", "Materials"};
  static const std::array<const char *, 12> avatars = {
      "Female CC0", "Male CC0", "Center part", "Next part", "Previous part", "Rig and shapes",
      "Delete group", "Ungroup", "Back", "Undo", "Redo", "Save"};
  static const std::array<const char *, 12> mesh = {
      "Prepare paint", "Voxel remesh", "Polygon edit", "UV editor",
      "Plane cut", "Paint tools", "Baking", "Shapekeys",
      "Back", "Holding", "Import", "Retopology"};
  static const std::array<const char *, 12> rig = {
      "Rotate joint", "Next joint", "Previous joint", "Shapekey weight",
      "Next shape", "Previous shape", "Reset pose", "Undo",
      "Back", "Redo", "Weights", "Export avatar"};
  static const std::array<const char *, 12> weights = {
      "Paint weights", "Next joint", "Previous joint", "Mode",
      "Strength", "Target", "Symmetry", "Joint lock",
      "Back", "Linked parts", "Brush size", "Bind / transfer"};
  static const std::array<const char *,12> baking={"Source", "Projection bounds", "AO / padding", "Bake surface", "Clear baked", "Cancel work", "Undo", "Redo", "Back", "Preparation policy", "Export glTF", "Projection policy"};
  static const std::array<const char *,12> skin={"Source", "Method", "Distances", "Falloff", "Influence cap", "Prune", "Use locks", "Bind / transfer", "Back", "Undo", "Redo", "Cancel work"};
  static const std::array<const char *,12> retopo = {
      "New patch", "Tool mode", "Make face", "Connect edge", "Delete points", "Relax",
      "Pin points", "Seam edge", "Back", "Snap range", "Apply patch", "Cancel patch"};
  static const std::array<const char *,12> cut={"Start cut","Offset","Direction","Cap color","From hand","Flip side","Cap","Apply cut","Back","Undo","Redo","Cancel cut"};
  static const std::array<const char *,12> masks = {
    "Paint mask", "Mask mode", "Strength", "Protect all", "Clear mask", "Invert mask",
    "Mirror X", "Mirror Y", "Back", "Mirror Z", "Planes / view", "Cancel work"};
  static const std::array<const char *,12> procedural={"Pattern","Face","Grid","Coverage / shape","Depth","Seed","Apply detail","Next seed","Back","Undo","Redo","Cancel work"};
  static const std::array<const char *,12> holding={"Object grab mode","Hold selected","Release held","Ghost others","Visibility","Previous","Next","Center","Back","Move left","Move right","Cancel motion"};
  static const std::array<const char*,12> importing={"Browse / refresh","Previous file","Next file","Open folder","Parent folder","Options","Import","Cancel","Back","Undo","Redo","Save"};
  static const std::array<const char*,12> import_options={"Format","Units","Axes","Normals","Paint size","Materials","Custom unit","Defaults","Back","Refresh folder","Import","Cancel"};
  static const std::array<const char*,12> polygon={"Start / resume","Vertices","Edges","Faces","Move / select","Add / toggle","Distance","Extrude","Back","Topology","Apply","Cancel"};
  static const std::array<const char*,12> polygon_topology={"All / none","Inset faces","Bridge loops","Dissolve edges","Weld vertices","Cut edges","Cuts","Connect vertices","Back","Delete faces","Flip faces","Cancel"};
  static const std::array<const char*,12> shapes={"New neutral","Capture mix","Duplicate","Mirror","Shape weight","Previous shape","Next shape","Restore original","Back","Undo","Redo","Edit / finish target"};
  return page == 24 ? shapes : page == 22 ? polygon_topology : page == 19 ? polygon : page == 23 ? import_options : page == 21 ? importing : page == 20 ? holding : page == 18 ? procedural : page == 17 ? cut : page == 16 ? retopo : page == 15 ? masks : page == 14 ? skin
         : page == 13 ? weights : page == 12 ? baking
         : page == 11 ? rig
         : page == 10 ? mesh
         : page == 9 ? avatars
         : page == 8 ? recovery
         : page == 7 ? sculpt
         : page == 1 ? objects
         : page == 2 ? materials
         : page == 3 ? paint
         : page == 4 ? colors
         : page == 6 ? controls
                     : root;
}
std::optional<VrParameter> spatial_parameter(int page, int sector) {
  if(page==24&&sector==4)return VrParameter::MorphWeight;
  if(page==24&&sector==7)return VrParameter::ShapeRestore;
  if(page==12&&sector==9)return VrParameter::BakeColors;
  if(page==19&&sector==6)return VrParameter::PolygonDistance;
  if(page==22&&sector==6)return VrParameter::PolygonCuts;
  if(page==12&&sector==11)return VrParameter::BakeProjection;
  if(page==18 && sector>=0 && sector<6) {
    static const VrParameter params[]={VrParameter::ProceduralPattern,VrParameter::ProceduralFace,VrParameter::ProceduralGrid,VrParameter::ProceduralCoverage,VrParameter::ProceduralDepth,VrParameter::ProceduralSeed};
    return params[sector];
  }
  if(page==23&&sector==6)return VrParameter::ImportUnitScale;
  if(page==17&&sector>=1&&sector<=3)return sector==1?VrParameter::CutOffset:sector==2?VrParameter::CutDirection:VrParameter::CutCapColor;
  if (page == 15) {
    if (sector == 1) return VrParameter::SculptMaskMode;
    if (sector == 2) return VrParameter::SculptMaskStrength;
    if (sector == 10) return VrParameter::SculptPlanes;
  }
  if (page == 16 && sector == 9) return VrParameter::RetopoDistance;
  if(page==12 && sector>=0 && sector<3)return sector==0?VrParameter::BakeSource:sector==1?VrParameter::BakeDistances:VrParameter::BakeQuality;
  if(page==14 && sector>=0 && sector<6) {
    static const std::array<VrParameter,6> parameters={VrParameter::SkinSource,VrParameter::SkinMode,VrParameter::SkinDistances,VrParameter::SkinFalloff,VrParameter::SkinInfluences,VrParameter::SkinPrune};
    return parameters[sector];
  }
  if (page == 13 && sector == 4) return VrParameter::WeightStrength;
  if (page == 13 && sector == 5) return VrParameter::WeightValue;
  if (page == 13 && sector == 10) return VrParameter::BrushRadius;
  if (page == 11 && sector == 0) return VrParameter::RigRotation;
  if (page == 11 && sector == 3) return VrParameter::MorphWeight;
  if (page == 10 && sector == 1)
    return VrParameter::VoxelSize;
  if (page == 10 && sector == 3)
    return VrParameter::UvEdit;
  if (page == 7 && sector == 7)
    return VrParameter::SculptResolution;
  if (page == 7 && sector == 5)
    return VrParameter::SculptOpacity;
  if (page == 0) {
    if (sector == 10)
      return VrParameter::BrushRadius;
    if (sector == 11)
      return VrParameter::Flow;
  }
  if (page == 2) {
    switch (sector) {
    case 0:
      return VrParameter::BaseColor;
    case 4:
      return VrParameter::Metallic;
    case 5:
      return VrParameter::Roughness;
    case 6:
      return VrParameter::Opacity;
    case 7:
      return VrParameter::UvEdit;
    case 11:
      return VrParameter::Flow;
    default:
      break;
    }
  }
  if (page == 6) {
    static const std::array<VrParameter, 12> controls = {
        VrParameter::InkColor,     VrParameter::InkMetallic,
        VrParameter::InkRoughness, VrParameter::InkEmission,
        VrParameter::Mask,         VrParameter::Flow,
        VrParameter::LayerOpacity, VrParameter::BrushRadius,
        VrParameter::None,         VrParameter::None,
        VrParameter::None,         VrParameter::LayerOrder};
    if (sector >= 0 && sector < 12 && controls[sector] != VrParameter::None)
      return controls[sector];
  }
  return {};
}
std::optional<SpatialCommand> spatial_action(int page, int index) {
  if (index < 0 || index >= 12 || page < 0 || page >= spatial_page_count || page == 5 ||
      spatial_parameter(page, index))
    return {};
  if(page==24){
    static const std::array<SpatialCommand,12> shapes={SpatialCommand::ShapeNew,SpatialCommand::ShapeCapture,SpatialCommand::ShapeDuplicate,SpatialCommand::ShapeMirror,SpatialCommand::Undo,SpatialCommand::RigPreviousMorph,SpatialCommand::RigNextMorph,SpatialCommand::Undo,SpatialCommand::Undo,SpatialCommand::Undo,SpatialCommand::Redo,SpatialCommand::ShapeSculpt};
    return index==8?std::nullopt:std::optional<SpatialCommand>(shapes[index]);
  }
  if(page==19){
    static const SpatialCommand commands[]={SpatialCommand::PolygonBegin,SpatialCommand::PolygonVertices,SpatialCommand::PolygonEdges,SpatialCommand::PolygonFaces,SpatialCommand::PolygonMoveMode,SpatialCommand::PolygonAdditive,SpatialCommand::Undo,SpatialCommand::PolygonExtrude,SpatialCommand::Undo,SpatialCommand::Undo,SpatialCommand::PolygonApply,SpatialCommand::PolygonCancel};
    return index==6||index==8||index==9?std::nullopt:std::optional<SpatialCommand>(commands[index]);
  }
  if(page==22){
    static const SpatialCommand commands[]={SpatialCommand::PolygonSelectAll,SpatialCommand::PolygonInset,SpatialCommand::PolygonBridge,SpatialCommand::PolygonDissolve,SpatialCommand::PolygonWeld,SpatialCommand::PolygonCutEdges,SpatialCommand::Undo,SpatialCommand::PolygonConnect,SpatialCommand::Undo,SpatialCommand::PolygonDelete,SpatialCommand::PolygonFlip,SpatialCommand::PolygonCancel};
    return index==6||index==8?std::nullopt:std::optional<SpatialCommand>(commands[index]);
  }
  if(page==18) {
    if(index<6 || index==8)return {};
    static const SpatialCommand commands[]={SpatialCommand::ProceduralApply,SpatialCommand::ProceduralNextSeed,SpatialCommand::Undo,SpatialCommand::Undo,SpatialCommand::Redo,SpatialCommand::CancelWork};
    return commands[index-6];
  }
  if(page==21){
    static const std::array<SpatialCommand,12> actions={SpatialCommand::ImportBrowse,SpatialCommand::ImportPrevious,SpatialCommand::ImportNext,SpatialCommand::ImportOpen,
      SpatialCommand::ImportParent,SpatialCommand::Undo,SpatialCommand::ImportApply,SpatialCommand::ImportCancel,SpatialCommand::Undo,SpatialCommand::Undo,SpatialCommand::Redo,SpatialCommand::Save};
    return index==5||index==8?std::nullopt:std::optional<SpatialCommand>(actions[index]);
  }
  if(page==23){
    static const std::array<SpatialCommand,12> actions={SpatialCommand::ImportFormat,SpatialCommand::ImportUnits,SpatialCommand::ImportAxes,SpatialCommand::ImportNormals,
      SpatialCommand::ImportResolution,SpatialCommand::ImportMaterials,SpatialCommand::Undo,SpatialCommand::ImportDefaults,SpatialCommand::Undo,SpatialCommand::ImportBrowse,SpatialCommand::ImportApply,SpatialCommand::ImportCancel};
    return index==6||index==8?std::nullopt:std::optional<SpatialCommand>(actions[index]);
  }
  if(page==17){
    static const std::array<SpatialCommand,12> cut={SpatialCommand::CutBegin,SpatialCommand::Undo,SpatialCommand::Undo,SpatialCommand::Undo,
      SpatialCommand::CutHand,SpatialCommand::CutFlip,SpatialCommand::CutCap,SpatialCommand::CutApply,SpatialCommand::Undo,
      SpatialCommand::Undo,SpatialCommand::Redo,SpatialCommand::CutCancel};
    if(index<0||index>11||(index>=1&&index<=3)||index==8)return {};
    return cut[index];
  }
  if(page==15) {
    static const std::array<SpatialCommand,12> actions={SpatialCommand::SculptMaskResume,SpatialCommand::Undo,SpatialCommand::Undo,SpatialCommand::SculptMaskFill,SpatialCommand::SculptMaskClear,SpatialCommand::SculptMaskInvert,SpatialCommand::SculptMirrorX,SpatialCommand::SculptMirrorY,SpatialCommand::Undo,SpatialCommand::SculptMirrorZ,SpatialCommand::Undo,SpatialCommand::CancelWork};
    return index==8?std::nullopt:std::optional<SpatialCommand>(actions[index]);
  }
  if(page==14) {
    static const std::array<SpatialCommand,6> actions={SpatialCommand::SkinUseLocks,SpatialCommand::BindSkin,SpatialCommand::Undo,SpatialCommand::Undo,SpatialCommand::Redo,SpatialCommand::CancelWork};
    return index<6 || index==8?std::nullopt:std::optional<SpatialCommand>(actions[index-6]);
  }
  if (page == 16) {
    static const std::array<SpatialCommand,12> retopo = {
      SpatialCommand::RetopoBegin, SpatialCommand::RetopoMode, SpatialCommand::RetopoFace,
      SpatialCommand::RetopoWire, SpatialCommand::RetopoDelete, SpatialCommand::RetopoRelax,
      SpatialCommand::RetopoPin, SpatialCommand::RetopoSeam, SpatialCommand::Undo,
      SpatialCommand::Undo, SpatialCommand::RetopoApply, SpatialCommand::RetopoCancel};
    if (index < 0 || index >= 12 || index == 8 || index == 9) return {};
    return retopo[index];
  }
  if(page==12){
    if(index==9)return {};
    if(index<3 || index==8 || index<0 || index>11)return {};
    static const std::array<SpatialCommand,12> commands={SpatialCommand::BakeSurface,SpatialCommand::BakeSurface,SpatialCommand::BakeSurface,SpatialCommand::BakeSurface,SpatialCommand::ClearBake,SpatialCommand::CancelWork,SpatialCommand::Undo,SpatialCommand::Redo,SpatialCommand::Undo,SpatialCommand::Save,SpatialCommand::Export,SpatialCommand::Center};
    return commands[index];
  }
  if (page == 13) {
    static const std::array<SpatialCommand, 12> weights = {
      SpatialCommand::WeightToggle, SpatialCommand::RigNextJoint, SpatialCommand::RigPreviousJoint,
      SpatialCommand::WeightMode, SpatialCommand::Undo, SpatialCommand::Undo,
      SpatialCommand::WeightSymmetry, SpatialCommand::WeightLock, SpatialCommand::Undo,
      SpatialCommand::WeightLinked, SpatialCommand::Undo, SpatialCommand::Save};
    return index == 8 || index == 11 ? std::nullopt : std::optional<SpatialCommand>(weights[index]);
  }
  if (page == 11) {
    static const std::array<SpatialCommand, 12> rig = {
      SpatialCommand::Undo, SpatialCommand::RigNextJoint, SpatialCommand::RigPreviousJoint,
      SpatialCommand::Undo, SpatialCommand::RigNextMorph, SpatialCommand::RigPreviousMorph,
      SpatialCommand::RigResetPose, SpatialCommand::Undo, SpatialCommand::Undo,
      SpatialCommand::Redo, SpatialCommand::Save, SpatialCommand::ExportAvatar};
    return index == 8 || index == 10 ? std::nullopt : std::optional<SpatialCommand>(rig[index]);
  }
  if(page==20){static const std::array<SpatialCommand,12> actions={SpatialCommand::ObjectGrabMode,SpatialCommand::HoldObject,SpatialCommand::ReleaseObject,SpatialCommand::GhostOthers,SpatialCommand::Visibility,SpatialCommand::Previous,SpatialCommand::Next,SpatialCommand::Center,SpatialCommand::Center,SpatialCommand::Left,SpatialCommand::Right,SpatialCommand::CancelObjectGrab};return index==8?std::nullopt:std::optional<SpatialCommand>(actions[index]);}
  if (page == 9) {
    static const std::array<SpatialCommand,12> avatars = {
      SpatialCommand::CreateFemale, SpatialCommand::CreateMale, SpatialCommand::Center,
      SpatialCommand::Next, SpatialCommand::Previous, SpatialCommand::Center,
      SpatialCommand::DeleteGroup, SpatialCommand::Ungroup, SpatialCommand::Undo,
      SpatialCommand::Undo, SpatialCommand::Redo, SpatialCommand::Save};
    return index == 8 || index == 5 ? std::nullopt : std::optional<SpatialCommand>(avatars[index]);
  }
  if (page == 10) {
    static const std::array<SpatialCommand, 12> mesh = {
        SpatialCommand::MakeMesh, SpatialCommand::Undo, SpatialCommand::EditSource,
        SpatialCommand::Undo, SpatialCommand::Center, SpatialCommand::Undo,
        SpatialCommand::Undo, SpatialCommand::Undo, SpatialCommand::Undo,
        SpatialCommand::Left, SpatialCommand::Right, SpatialCommand::Undo};
    return (index==2 || (index >= 4 && index <= 11)) ? std::nullopt
                                    : std::optional<SpatialCommand>(mesh[index]);
  }
  if (page == 8) {
    static const std::array<SpatialCommand, 12> recovery = {
      SpatialCommand::RecoveryRefresh, SpatialCommand::RecoveryNext,
      SpatialCommand::RecoveryPrevious, SpatialCommand::RecoveryRestore,
      SpatialCommand::Save, SpatialCommand::Center,
      SpatialCommand::Undo, SpatialCommand::Redo, SpatialCommand::Undo,
      SpatialCommand::Export, SpatialCommand::Undo, SpatialCommand::Undo};
    return index == 8 || index > 9 ? std::nullopt
                                   : std::optional<SpatialCommand>(recovery[index]);
  }
  if (page == 7) {
    static const std::array<SpatialCommand, 12> sculpt = {
        SpatialCommand::SculptAdd,      SpatialCommand::SculptNext,
        SpatialCommand::SculptPrevious, SpatialCommand::SculptVisible,
        SpatialCommand::SculptLock,     SpatialCommand::SculptOpacity,
        SpatialCommand::SculptDelete,   SpatialCommand::Undo,
        SpatialCommand::Undo,           SpatialCommand::Redo,
        SpatialCommand::Center,         SpatialCommand::Save};
    return index == 8 || index == 9 || index == 10 || index == 11 ? std::nullopt
                      : std::optional<SpatialCommand>(sculpt[index]);
  }
  if ((page == 0 && (index < 4 || index == 8 || index == 9)) ||
      (page != 0 && index == 8) || (page == 2 && index == 3) ||
      (page == 3 && index == 11) || (page == 1 && (index == 9 || index == 10 || index == 11)))
    return {};
  static const std::array<SpatialCommand, 12> root = {
      SpatialCommand::Undo, SpatialCommand::Undo,   SpatialCommand::Undo,
      SpatialCommand::Undo, SpatialCommand::Undo,   SpatialCommand::Redo,
      SpatialCommand::Save, SpatialCommand::Export, SpatialCommand::Undo,
      SpatialCommand::Undo, SpatialCommand::Larger, SpatialCommand::Smaller};
  static const std::array<SpatialCommand, 12> objects = {
      SpatialCommand::NewSphere, SpatialCommand::NewEmpty,
      SpatialCommand::Duplicate, SpatialCommand::Next,
      SpatialCommand::Previous,  SpatialCommand::Visibility,
      SpatialCommand::Lock,      SpatialCommand::Delete,
      SpatialCommand::Undo,      SpatialCommand::Left,
      SpatialCommand::Right,     SpatialCommand::Center};
  static const std::array<SpatialCommand, 12> materials = {
      SpatialCommand::Warm,    SpatialCommand::Blue,
      SpatialCommand::White,   SpatialCommand::Dark,
      SpatialCommand::MetalUp, SpatialCommand::MetalDown,
      SpatialCommand::RoughUp, SpatialCommand::RoughDown,
      SpatialCommand::Undo,    SpatialCommand::Toon,
      SpatialCommand::Pbr,     SpatialCommand::Opacity};
  static const std::array<SpatialCommand, 12> paint = {
      SpatialCommand::MakeMesh,   SpatialCommand::EditSource,
      SpatialCommand::PaintColor, SpatialCommand::PaintMetal,
      SpatialCommand::PaintRough, SpatialCommand::PaintGlow,
      SpatialCommand::PaintMask,  SpatialCommand::AddLayer,
      SpatialCommand::Undo,       SpatialCommand::NextLayer,
      SpatialCommand::ErasePaint, SpatialCommand::Undo};
  static const std::array<SpatialCommand, 12> colors = {
      SpatialCommand::PaintRed,    SpatialCommand::PaintGreen,
      SpatialCommand::PaintBlue,   SpatialCommand::PaintWhite,
      SpatialCommand::PaintBlack,  SpatialCommand::PaintWarm,
      SpatialCommand::FlowUp,      SpatialCommand::FlowDown,
      SpatialCommand::Undo,        SpatialCommand::LayerVisible,
      SpatialCommand::DeleteLayer, SpatialCommand::LayerOpacity};
  if (page == 6)
    return index == 10
               ? std::optional<SpatialCommand>(SpatialCommand::ErasePaint)
               : std::nullopt;
  // Future registered-page handlers must return above; unknown pages cannot
  // accidentally fall through to the root menu's commands.
  if(page>4)return {};
  return (page == 1   ? objects
          : page == 2 ? materials
          : page == 3 ? paint
          : page == 4 ? colors
                      : root)[index];
}
void SpatialInput::cancel() {
  if (stroke_active()) {
    if (backend.cancel)
      backend.cancel();
    else
      volume_->cancel_stroke();
  }
  device_ = 0;
  menu_ = false;
  page_ = 0;
  parameter_ = VrParameter::None;
  controls_.clear();
  uv_dragging_ = false;
  uv_stick_active_ = uv_gesture_open_ = false;
  uv_pointer_ = 0;
  command_.reset();
  command_device_ = 0;
  devices_.clear();
}
void SpatialInput::uv_gesture_begin() {
  controls_.push_back({VrControlEventType::UvCommand, parameter_,
                       int(UvAction::BeginGesture)});
  uv_gesture_open_ = true;
}
void SpatialInput::uv_gesture_end(bool force) {
  if (uv_gesture_open_ && (force || (!uv_dragging_ && !uv_stick_active_))) {
    controls_.push_back({VrControlEventType::UvCommand, parameter_,
                         int(UvAction::EndGesture)});
    uv_gesture_open_ = false;
  }
}
void SpatialInput::ray_controls(const SpatialSample &s, bool pressed) {
  if (pressed && s.panel_hit && s.panel_v >= 588.f / 640 &&
      s.panel_v <= 630.f / 640) {
    bool cancel = s.panel_u >= 60.f / 640 && s.panel_u <= 280.f / 640;
    bool accept = s.panel_u >= 360.f / 640 && s.panel_u <= 580.f / 640;
    if (accept && control_accept && !control_accept())
      return;
    if (cancel || accept) {
      uv_dragging_ = uv_stick_active_ = uv_gesture_open_ = false;
      uv_pointer_ = 0;
      controls_.push_back(
          {cancel ? VrControlEventType::Cancel : VrControlEventType::Confirm,
           parameter_});
      parameter_ = VrParameter::None;
      page_ = parent_page_;
      selected_ = parent_sector_;
      return;
    }
  }
  if (parameter_ == VrParameter::UvEdit) {
    if (!s.trigger) {
      if (uv_pointer_ == s.id) {
        uv_dragging_ = false;
        uv_pointer_ = 0;
        uv_gesture_end();
      }
      return;
    }
    float x = s.panel_u * 640, y = s.panel_v * 640;
    if (pressed && x >= 8 && x <= 102 &&
        ((y >= 145 && y <= 187) || (y >= 197 && y <= 239))) {
      uv_gesture_end(true);
      controls_.push_back({VrControlEventType::UvCommand, parameter_,
          int(y < 190 ? UvAction::Conformal : UvAction::Stitch), 0});
      return;
    }
    if (pressed && ((y >= 68 && y <= 102) || (y >= 538 && y <= 580)) &&
        x >= 40 && x < 600) {
      int column = int((x - 40) / 94);
      if (column < 6 && (x - 40 - column * 94) < 88) {
        UvAction action;
        float value = 0;
        if (y < 110) {
          if (column < 3) {
            uv_mode_ = column;
            action = UvAction::Tool;
            value = float(column);
          } else {
            action = column == 3   ? UvAction::Rotate
                     : column == 4 ? UvAction::Undo
                                   : UvAction::Redo;
            value = column == 3 ? 1.570796327f : 0;
          }
        } else {
          static const UvAction actions[] = {
              UvAction::VertexMode, UvAction::Pin,  UvAction::Relax,
              UvAction::Pack,       UvAction::Seam, UvAction::Next};
          action = actions[column];
        }
        uv_gesture_end(true);
        controls_.push_back(
            {VrControlEventType::UvCommand, parameter_, int(action), value});
      }
      return;
    }
    if (x >= 110 && x <= 530 && y >= 110 && y <= 530) {
      if (uv_pointer_ && uv_pointer_ != s.id)
        return;
      float u = (x - 110) / 420, v = (y - 110) / 420;
      if (pressed) {
        controls_.push_back({VrControlEventType::UvPick, parameter_, 0, u, v});
        uv_dragging_ = true;
        uv_pointer_ = s.id;
      } else if (uv_dragging_) {
        uv_gesture_begin();
        if (!uv_mode_)
          controls_.push_back({VrControlEventType::UvMove, parameter_, 0,
                               u - uv_x_, v - uv_y_});
        else {
          float delta = (u - uv_x_) - (v - uv_y_) * .1f;
          auto action = uv_mode_ == 1 ? UvAction::Scale : UvAction::Rotate;
          float value =
              uv_mode_ == 1 ? std::exp(delta * 2) : delta * 3.14159265f;
          controls_.push_back(
              {VrControlEventType::UvCommand, parameter_, int(action), value});
        }
      }
      uv_x_ = u;
      uv_y_ = v;
    } else if (uv_pointer_ == s.id) {
      uv_dragging_ = false;
      uv_pointer_ = 0;
      uv_gesture_end();
    }
    return;
  }
  bool color = parameter_ == VrParameter::InkColor ||
               parameter_ == VrParameter::InkEmission ||
               parameter_ == VrParameter::BaseColor || parameter_ == VrParameter::CutCapColor;
  bool multi = parameter_==VrParameter::BakeColors || parameter_==VrParameter::BakeProjection || parameter_==VrParameter::ProceduralGrid || parameter_==VrParameter::ProceduralCoverage || color || parameter_ == VrParameter::SculptPlanes || parameter_ == VrParameter::CutDirection || parameter_ == VrParameter::RigRotation || parameter_==VrParameter::BakeDistances || parameter_==VrParameter::SkinDistances || parameter_==VrParameter::BakeQuality;
  int components = (parameter_==VrParameter::BakeProjection || parameter_==VrParameter::ProceduralGrid || parameter_==VrParameter::BakeQuality || parameter_==VrParameter::CutDirection)?2:parameter_==VrParameter::ProceduralCoverage?3:parameter_==VrParameter::BakeDistances || parameter_==VrParameter::SkinDistances || parameter_ == VrParameter::RigRotation || parameter_ == VrParameter::InkEmission ? 3 : parameter_==VrParameter::BakeColors || color || parameter_ == VrParameter::SculptPlanes ? 4 : 1;
  if (s.trigger && s.panel_hit) {
    int row = 0;
    float y = .46875f;
    if (multi) {
      int base = components == 4 ? 210 : 240, step = components == 4 ? 70 : 80;
      row = int(std::round((s.panel_v * 640 - base) / step));
      row = std::clamp(row, 0, components - 1);
      y = (base + row * step) / 640.f;
    }
    if (std::abs(s.panel_v - y) < .06f && s.panel_u > .12f &&
        s.panel_u < .86f) {
      component_ = row;
      controls_.push_back(
          {VrControlEventType::Absolute, parameter_, component_,
           std::clamp((s.panel_u * 640 - 100) / 420.f, 0.f, 1.f)});
    }
  }
}
void SpatialInput::update(const SpatialSample &s) {
  if (controls_.size() > 64) {
    cancel();
    return;
  }
  bool valid =
      s.id && s.tracked && s.focused && finite(s.tip) && finite(s.normal) &&
      std::isfinite(s.time) && s.time >= 0 && std::isfinite(s.pressure) &&
      s.pressure >= 0 && s.pressure <= 1 && std::isfinite(s.stick_x) &&
      std::isfinite(s.stick_y) && std::abs(s.stick_x) <= 1 &&
      std::abs(s.stick_y) <= 1 &&
      (!s.panel_hit || (std::isfinite(s.panel_u) && std::isfinite(s.panel_v)));
  if (!valid) {
    if (uv_pointer_ == s.id) {
      uv_dragging_ = false;
      uv_pointer_ = 0;
      uv_gesture_end();
    }
    if (s.id == device_)
      cancel();
    else
      devices_.erase(s.id);
    return;
  }
  if (!devices_.contains(s.id) && devices_.size() >= 16)
    throw std::length_error("Too many spatial input sources");
  auto old = devices_.find(s.id);
  if (old != devices_.end()) {
    if (s.sequence <= old->second.sequence || s.time < old->second.time)
      return;
    if (old->second.time >= 0 && s.time - old->second.time > .25) {
      if (device_ == s.id)
        cancel();
      else
        devices_.erase(old);
    }
  }
  auto &d = devices_[s.id];
  double elapsed = d.time >= 0 ? std::clamp(s.time - d.time, 0.0, .1) : 0;
  d.sequence = s.sequence;
  d.time = s.time;
  if (!d.armed) {
    d.pressed = s.trigger;
    d.joystick = s.joystick_press;
    if (!s.trigger && !s.joystick_press) {
      d.armed = true;
      // A command can open a guarded parameter panel. The same observed
      // all-released sample rearms its owner and its panel; otherwise the
      // next real pressure press is silently consumed a second time.
      if(menu_&&parameter_!=VrParameter::None&&device_==s.id&&
         std::abs(s.stick_x)<.25f&&std::abs(s.stick_y)<.25f)control_armed_=true;
    }
    return;
  }
  bool menu_press = s.joystick_press && !d.joystick;
  d.joystick = s.joystick_press;
  // Idle hands cannot acquire an edit lease. A hand held while another edits
  // must release before it can take ownership of a later stroke.
  if (device_ && device_ != s.id) {

    if (menu_ && parameter_ != VrParameter::None && control_armed_ &&
        (s.panel_hit || uv_dragging_)) {
      ray_controls(s, s.trigger && !d.pressed);
      d.pressed = s.trigger;
      return;
    }
    if (s.trigger || s.joystick_press)
      d.armed = false;
    d.pressed = s.trigger;
    return;
  }
  if (menu_ && parameter_ != VrParameter::None) {
    if (menu_press) {
      if (control_accept && !control_accept())
        return;
      uv_dragging_ = uv_stick_active_ = uv_gesture_open_ = false;
      uv_pointer_ = 0;
      controls_.push_back({VrControlEventType::Confirm, parameter_});
      parameter_ = VrParameter::None;
      page_ = parent_page_;
      selected_ = parent_sector_;
      d.pressed = s.trigger;
      return;
    }
    if (!control_armed_) {
      if (std::abs(s.stick_x) < .25f && std::abs(s.stick_y) < .25f &&
          !s.trigger)
        control_armed_ = true;
      d.pressed = s.trigger;
      return;
    }
    if (parameter_ == VrParameter::UvEdit) {
      auto axis = [](float v) {
        return std::abs(v) > .25f
                   ? std::copysign((std::abs(v) - .25f) / .75f, v)
                   : 0.f;
      };
      float dx = float(elapsed) * axis(s.stick_x) * .25f,
            dy = -float(elapsed) * axis(s.stick_y) * .25f;
      uv_stick_active_ = axis(s.stick_x) != 0 || axis(s.stick_y) != 0;
      if (dx || dy) {
        uv_gesture_begin();
        if (!uv_mode_)
          controls_.push_back(
              {VrControlEventType::UvMove, parameter_, 0, dx, dy});
        else {
          float delta =
              float(elapsed) * (axis(s.stick_x) + axis(s.stick_y) * .1f);
          auto action = uv_mode_ == 1 ? UvAction::Scale : UvAction::Rotate;
          float value = uv_mode_ == 1 ? std::exp(delta * .69314718f)
                                      : delta * .523598776f;
          controls_.push_back(
              {VrControlEventType::UvCommand, parameter_, int(action), value});
        }
      }
      ray_controls(s, s.trigger && !d.pressed);
      uv_gesture_end();
      d.pressed = s.trigger;
      return;
    }
    bool color = parameter_ == VrParameter::InkColor ||
                 parameter_ == VrParameter::InkEmission ||
                 parameter_ == VrParameter::BaseColor || parameter_ == VrParameter::CutCapColor;
    bool multi = parameter_==VrParameter::BakeColors || parameter_==VrParameter::BakeProjection || parameter_==VrParameter::ProceduralGrid || parameter_==VrParameter::ProceduralCoverage || color || parameter_ == VrParameter::SculptPlanes || parameter_ == VrParameter::CutDirection || parameter_ == VrParameter::RigRotation || parameter_==VrParameter::BakeDistances || parameter_==VrParameter::SkinDistances || parameter_==VrParameter::BakeQuality;
    int components = (parameter_==VrParameter::BakeProjection || parameter_==VrParameter::ProceduralGrid || parameter_==VrParameter::BakeQuality || parameter_==VrParameter::CutDirection)?2:parameter_==VrParameter::ProceduralCoverage?3:parameter_==VrParameter::BakeDistances || parameter_==VrParameter::SkinDistances || parameter_ == VrParameter::RigRotation || parameter_ == VrParameter::InkEmission ? 3 : parameter_==VrParameter::BakeColors || color || parameter_ == VrParameter::SculptPlanes ? 4 : 1;
    if (multi) {
      if (std::abs(s.stick_y) < .3f)
        y_latch_ = 0;
      if (std::abs(s.stick_y) > .6f && !y_latch_) {
        y_latch_ = 1;
        component_ =
            (component_ + (s.stick_y < 0 ? 1 : components - 1)) % components;
        controls_.push_back(
            {VrControlEventType::Select, parameter_, component_});
      }
    }
    auto axis = [](float v) {
      return std::abs(v) > .25f ? std::copysign((std::abs(v) - .25f) / .75f, v)
                                : 0.f;
    };
    float delta = float(elapsed) *
                  (axis(s.stick_x) + (multi ? 0 : axis(s.stick_y) * .1f));
    if (delta)
      controls_.push_back(
          {VrControlEventType::Delta, parameter_, component_, delta});
    ray_controls(s, s.trigger && !d.pressed);
    d.pressed = s.trigger;
    return;
  }
  if (menu_press) {
    if (stroke_active())
      end_stroke();
    if (!menu_) {
      menu_ = true;
      page_ = polygon_editing ? 19 : cutting ? 17 : retopology ? 16 : 0;
      menu_anchor_ = s.tip;
      device_ = s.id;
      selected_ = polygon_editing ? 0 : cutting ? 1 : retopology ? 1 : int(tool_);
    } else if (page_ == 0 && (selected_ == 8 || selected_ == 9)) {
      page_ = selected_ == 8 ? 1 : 2;
      selected_ = 0;
    } else if (page_ == 7 && selected_ == 9) {
      page_ = 18; selected_ = 0;
    } else if (page_ == 7 && selected_ == 11) {
      page_ = 15; selected_ = 0;
    } else if (page_ == 7 && selected_ == 10) {
      page_ = 8;
      selected_ = 0;
    } else if (page_ == 8 && (selected_ == 10 || selected_ == 11)) {
      page_ = selected_ == 10 ? 1 : 2;
      selected_ = 0;
    } else if (page_ == 2 && selected_ == 3) {
      page_ = 3;
      selected_ = 0;
    } else if(page_==13 && selected_==11) {
      page_=14;selected_=0;
    } else if (page_ == 11 && selected_ == 10) {
      page_ = 13; selected_ = 0;
    } else if (page_ == 9 && selected_ == 5) {
      page_ = 11;
      selected_ = 0;
    } else if (page_ == 1 && selected_ == 10) {
      page_ = 9;
      selected_ = 0;
    } else if (page_ == 1 && selected_ == 11) {
      page_ = 7;
      selected_ = 0;
    } else if (page_ == 1 && selected_ == 9) {
      page_ = 10;
      selected_ = 0;
    } else if(page_==10&&selected_==2){page_=19;selected_=0;
    } else if(page_==19&&selected_==9){page_=22;selected_=0;
    } else if(page_==10&&selected_==9){page_=20;selected_=0;
    } else if (page_ == 10 && selected_ == 10) {
      page_=21;selected_=0;command_=SpatialCommand::ImportBrowse;command_device_=s.id;
    } else if(page_==21&&selected_==5){
      page_=23;selected_=0;
    } else if (page_ == 10 && selected_ == 11) {
      page_ = 16; selected_ = 0;
    } else if(page_==10&&selected_==4){
      page_=17;selected_=0;
    } else if (page_ == 10 && (selected_ == 5 || selected_ == 6 || selected_ == 7)) {
      page_ = selected_ == 5 ? 3 : selected_ == 6 ? 12 : 24;
      selected_ = 0;
    } else if (page_ == 3 && selected_ == 11) {
      page_ = 6;
      selected_ = 0;
    } else if (page_ == 6 && selected_ == 9) {
      page_ = 4;
      selected_ = 0;
    } else if (auto parameter = spatial_parameter(page_, selected_)) {
      if (parameter_allowed && !parameter_allowed(*parameter)) {
        d.pressed = s.trigger;
        return;
      }
      parent_page_ = page_;
      parent_sector_ = selected_;
      parameter_ = *parameter;
      parameter_owner_ = device_;
      uv_mode_ = 0;
      uv_dragging_ = false;
      uv_stick_active_ = uv_gesture_open_ = false;
      uv_pointer_ = 0;
      control_armed_ = false;
      component_ = y_latch_ = 0;
      page_ = 5;
      controls_.push_back({VrControlEventType::Begin, parameter_});
    } else if (page_ != 0 && selected_ == 8) {
      page_ = page_==24 ? 10 : page_==22 ? 19 : page_==19 ? 10 : page_==23 ? 21 : (page_ == 16 || page_ == 17 || page_==20 || page_==21) ? 10 : (page_ == 18 || page_ == 15) ? 7 : page_ == 14 ? 13 : page_ == 13 ? 11
              : page_ == 12 ? 10
              : page_ == 11 ? 9
              : (page_ == 9 || page_ == 10) ? 1
              : page_ == 8 ? 7
              : page_ == 7 ? 1
              : page_ == 4 ? 6
              : page_ == 6 ? 3
              : page_ == 3 ? 2
                           : 0;
      selected_ = int(tool_);
    } else {
      if (page_ == 0 && selected_ < 4) {
        if ((painting || sculpt_mask_edit) && allowed && !allowed(SpatialCommand::EditSource)) {
          d.pressed = s.trigger;
          return;
        }
        tool_ = Tool(selected_);
        if (painting || sculpt_mask_edit)
          command_ = SpatialCommand::EditSource;
      } else {
        auto action = spatial_action(page_, selected_);
        if (!action || (allowed && !allowed(*action))) {
          d.pressed = s.trigger;
          return;
        }
        command_ = action;
      }
      command_device_ = s.id;
      if(command_&&*command_==SpatialCommand::PolygonBegin){parent_page_=page_;parent_sector_=selected_;parameter_owner_=s.id;}
      const bool resume_retopology = page_ == 16 && command_ &&
          (*command_ == SpatialCommand::RetopoBegin || *command_ == SpatialCommand::RetopoMode ||
           *command_ == SpatialCommand::RetopoApply || *command_ == SpatialCommand::RetopoCancel);
      const bool resume_mask = page_ == 15 && command_ && *command_ == SpatialCommand::SculptMaskResume;
      const bool resume_polygon=(page_==19||page_==22)&&command_&&
        (*command_==SpatialCommand::PolygonBegin||*command_==SpatialCommand::PolygonMoveMode||*command_==SpatialCommand::PolygonApply||*command_==SpatialCommand::PolygonCancel);
      if((page_!=19 && page_!=22 && page_!=12 && page_!=14 && page_!=15 && page_!=16 && page_!=18 && page_!=20 && page_!=21 && page_!=23) || resume_retopology || resume_mask || resume_polygon){menu_ = false;device_ = 0;}
      d.armed = false;
    }
    d.pressed = s.trigger;
    return;
  }
  if (menu_) {
    if (s.stick_x * s.stick_x + s.stick_y * s.stick_y > .25f) {
      constexpr float pi = 3.14159265358979323846f;
      float angle = std::atan2(s.stick_y, s.stick_x);
      int index = int(std::floor((angle + pi / 12 + 2 * pi) / (pi / 6))) % 12;
      selected_ = index;
    }
    d.pressed = s.trigger;
    return;
  }
  if (!edit_allowed) {
    d.pressed = s.trigger;
    return;
  }
  if (s.trigger) {
    if (!d.pressed) {
      device_ = s.id;
      if (backend.begin)
        backend.begin();
      else
        volume_->begin_stroke();
      previous_ = s.tip;
    }
    if (device_ == s.id && stroke_active()) {
      Brush brush;
      brush.from = previous_;
      brush.to = s.tip;
      brush.normal = s.normal;
      brush.radius = radius;
      brush.strength = strength;
      brush.pressure = s.pressure;
      brush.blend = std::min(.025f, radius * .25f);
      brush.tool = tool_;
      try {
        if (backend.stamp)
          backend.stamp(brush);
        else
          volume_->stamp(brush);
      } catch (...) {
        cancel();
        throw;
      }
      previous_ = s.tip;
    }
  } else if (d.pressed && device_ == s.id && stroke_active()) {
    end_stroke();
    device_ = 0;
  }
  d.pressed = s.trigger;
}
} // namespace dome
