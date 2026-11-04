// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "document-transaction.hpp"
#include "text-utf8.hpp"
#include <cstdlib>
namespace dome {
ProjectionToolSettings Document::projection_settings() const {
  std::lock_guard lock(model_mutex_);
  return state_.projection?state_.projection->settings:ProjectionToolSettings{};
}
void Document::configure_projection(ProjectionToolSettings settings,AssetSource expected) {
  settings.validate();std::lock_guard lock(model_mutex_);require_authority_locked(expected);
  if(expected.generation&&(expected.generation!=state_.generation||expected.revision!=state_.revision||expected.object!=scene_.active().id))
    throw std::logic_error("The projection target changed; reopen the image tool");
  if(stopping_.load())throw std::logic_error("Document is stopping");
  if(native_stroke_||desktop_||action_active_locked()||control_before_||(state_.projection&&state_.projection->placing))
    throw std::logic_error("Finish the current gesture before changing projection settings");
  if(state_.projection&&state_.projection->loading)throw std::logic_error("Finish loading the image before changing projection settings");
  if(settings.enabled&&(!state_.projection||!state_.projection->image))throw std::logic_error("Load an image before enabling projection");
  if(settings.enabled&&state_.shape_sculpt)throw std::logic_error("Leave shapekey sculpting before image painting");
  auto next=state_.projection?std::make_shared<ProjectionView>(*state_.projection):std::make_shared<ProjectionView>();
  next->settings=std::move(settings);next->error.clear();
  Transaction transaction(*this);state_.projection=std::move(next);publish_locked(false,false);transaction.commit(false);
}
void Document::load_projection_image(const std::filesystem::path &path,std::optional<ProjectionCamera> camera,AssetSource expected) {
  std::lock_guard lock(model_mutex_);load_projection_locked(path,std::move(camera),expected);
}
void Document::load_projection_locked(const std::filesystem::path &path,std::optional<ProjectionCamera> camera,AssetSource expected) {
  require_authority_locked(expected);
  if(expected.generation&&(expected.generation!=state_.generation||expected.revision!=state_.revision||expected.object!=scene_.active().id))
    throw std::logic_error("The projection target changed; choose the image again");
  if(stopping_.load())throw std::logic_error("Document is stopping");
  if(native_stroke_||desktop_||action_active_locked()||control_before_||state_.asset_busy||(state_.projection&&state_.projection->placing))
    throw std::logic_error("Finish the current operation before loading a projection image");
  auto task=std::make_shared<ProjectionLoad>();task->kind=ProjectionLoad::Kind::Image;task->path=path;
  task->source={state_.generation,state_.revision,scene_.active().id,state_.edit_authority};
  auto next=state_.projection?std::make_shared<ProjectionView>(*state_.projection):std::make_shared<ProjectionView>();
  task->settings=next->settings;if(camera)task->settings.camera=*camera;task->settings.validate();
  next->loading=true;next->browsing=false;next->error.clear();
  FileJob job{};job.projection_load=task;job.generation=state_.generation;
  std::lock_guard queue(file_mutex_);if(files_stopping_||files_.size()>=8)throw std::runtime_error("File work queue is full");
  Transaction transaction(*this);task->request=transaction.advance_projection_request();next->request=task->request;
  files_.push_back(std::move(job));
  try{projection_load_=task;state_.projection=std::move(next);publish_locked(false,false);transaction.commit(false);}
  catch(...){files_.pop_back();throw;}
  file_changed_.notify_all();
}
void Document::browse_projection_images(std::filesystem::path path) {
  std::lock_guard lock(model_mutex_);
  if(path.empty())path=state_.projection&&!state_.projection->directory.empty()?state_.projection->directory:
      !path_.empty()?path_.parent_path():std::getenv("HOME")?std::getenv("HOME"):".";
  browse_projection_locked(path);
}
void Document::browse_projection_locked(const std::filesystem::path &path) {
  if(stopping_.load())throw std::logic_error("Document is stopping");
  if(state_.projection&&state_.projection->placing)throw std::logic_error("Finish placing the projector before browsing images");
  auto task=std::make_shared<ProjectionLoad>();task->kind=ProjectionLoad::Kind::Directory;task->path=path;
  task->source={state_.generation,state_.revision,scene_.active().id,state_.edit_authority};
  auto next=state_.projection?std::make_shared<ProjectionView>(*state_.projection):std::make_shared<ProjectionView>();
  next->loading=false;next->browsing=true;next->error.clear();next->panel=ProjectionPanel::Browser;
  FileJob job{};job.projection_load=task;job.generation=state_.generation;
  std::lock_guard queue(file_mutex_);if(files_stopping_||files_.size()>=8)throw std::runtime_error("File work queue is full");
  Transaction transaction(*this);task->request=transaction.advance_projection_request();next->request=task->request;
  files_.push_back(std::move(job));
  try{projection_load_=task;state_.projection=std::move(next);publish_locked(false,false);transaction.commit(false);}
  catch(...){files_.pop_back();throw;}
  file_changed_.notify_all();
}
void Document::cancel_projection_load() {
  std::lock_guard lock(model_mutex_);cancel_projection_load_locked();
}
void Document::cancel_projection_load_locked() {
  if(!projection_load_)return;
  // Explicit cancellation must reach the worker even if updating its tool view
  // subsequently runs out of memory. New-job admission remains transactional.
  projection_load_->cancelled.store(true);
  auto next=state_.projection?std::make_shared<ProjectionView>(*state_.projection):std::make_shared<ProjectionView>();
  next->loading=next->browsing=false;next->error.clear();
  Transaction transaction(*this);next->request=transaction.advance_projection_request();projection_load_.reset();state_.projection=std::move(next);state_.message="Image load cancelled";
  publish_locked(false,false);transaction.commit(false);file_changed_.notify_all();
}
bool Document::wait_projection_image(std::chrono::milliseconds timeout) const {
  std::shared_ptr<ProjectionLoad> task;{std::lock_guard lock(model_mutex_);task=projection_load_;}
  if(!task)return true;
  std::unique_lock lock(file_mutex_);return file_changed_.wait_for(lock,timeout,[&]{
    if(!task->done.load())return false;
    const auto state=published_.load();
    return state->generation!=task->source.generation||!state->projection||state->projection->request!=task->request||
        (!state->projection->loading&&!state->projection->browsing);
  });
}
void Document::projection_file(const FileJob &job) {
  const auto task=job.projection_load;
  auto cancelled=[&]{
    if(stopping_.load()||task->cancelled.load()||projection_request_.load()!=task->request)return true;
    auto state=published_.load();
    return state->generation!=task->source.generation || (task->kind==ProjectionLoad::Kind::Image &&
        (state->revision!=task->source.revision||state->active_object!=task->source.object||state->edit_authority!=task->source.edit_authority));
  };
  std::shared_ptr<const ProjectionToolImage> image;
  std::vector<ProjectionEntry> entries;std::filesystem::path directory;
  if(!cancelled()) {
    if(task->kind==ProjectionLoad::Kind::Image) {
      auto name=task->path.filename().string();
      if(!text::valid_utf8(name))throw std::invalid_argument("Projection image filename must be valid UTF-8");
      auto pixels=std::make_shared<const Image>(load_projection_png(task->path,{},cancelled));
      auto next=std::make_shared<ProjectionToolImage>();next->pixels=std::move(pixels);next->name=text::utf8_prefix(name,240);image=std::move(next);
    }else {
      directory=std::filesystem::canonical(task->path);
      entries=scan_projection_directory(directory,cancelled);
    }
  }
  std::lock_guard lock(model_mutex_);
  if(stopping_.load()||projection_load_!=task||task->source.generation!=state_.generation||!state_.projection)return;
  auto next=std::make_shared<ProjectionView>(*state_.projection);next->loading=next->browsing=false;next->error.clear();
  if(cancelled()) {
    if(!task->cancelled.load())next->error="The image target changed; load the image again";
  }else if(task->kind==ProjectionLoad::Kind::Image) {
    next->image=std::move(image);next->settings=task->settings;
    next->settings.enabled=scene_.active().mesh_active&&!state_.shape_sculpt;next->settings.preview=true;next->panel=ProjectionPanel::Tools;
  }else {next->directory=std::move(directory);next->entries=std::move(entries);next->selected=0;}
  Transaction transaction(*this);state_.projection=std::move(next);projection_load_.reset();state_.message=cancelled()?"Image load cancelled":task->kind==ProjectionLoad::Kind::Image?"Projection image ready":"Choose a PNG image";publish_locked(false,false);transaction.commit(false);
}

bool Document::projection_command_enabled_locked(SpatialCommand command) const {
  if(state_.expressions&&command==SpatialCommand::ProjectionPlace)return false;
  if(stopping_.load()||native_stroke_||desktop_||control_before_||action_active_locked())return false;
  const auto v=state_.projection;
  if(command==SpatialCommand::ProjectionBrowse)return !v||!v->placing;
  if(v&&(v->loading||v->browsing)&&command==SpatialCommand::ProjectionPlace)return true;
  if(!v||v->loading||v->browsing)return false;
  if(v->placing)return command==SpatialCommand::ProjectionPlace;
  if(command==SpatialCommand::ProjectionPlace)return v->image&&!state_.object_grabbing&&!state_.asset_busy;
  if(command==SpatialCommand::ProjectionToggle)return v->image&&scene_.active().mesh_active&&!state_.shape_sculpt;
  if((command==SpatialCommand::ProjectionPrevious||command==SpatialCommand::ProjectionNext)&&v->panel==ProjectionPanel::Browser)return !v->entries.empty();
  if(command==SpatialCommand::ProjectionOpen&&v->panel==ProjectionPanel::Browser)return v->selected<v->entries.size();
  return true;
}
void Document::projection_action(SpatialCommand command){
  if(!projection_command(command))throw std::invalid_argument("Expected image projection action");
  std::lock_guard lock(model_mutex_);projection_command_locked(command);
}
void Document::projection_command_locked(SpatialCommand command){
  if(!projection_command_enabled_locked(command))return;
  if(command==SpatialCommand::ProjectionPlace&&state_.projection&&(state_.projection->loading||state_.projection->browsing)){cancel_projection_load_locked();return;}
  if(command==SpatialCommand::ProjectionBrowse){
    auto path=state_.projection&&!state_.projection->directory.empty()?state_.projection->directory:!path_.empty()?path_.parent_path():std::getenv("HOME")?std::getenv("HOME"):".";
    browse_projection_locked(path);return;
  }
  const auto &old=*state_.projection;
  if(old.panel==ProjectionPanel::Browser&&command==SpatialCommand::ProjectionOpen){
    const auto &entry=old.entries.at(old.selected);auto path=old.directory/entry.name;
    if(entry.directory)browse_projection_locked(path);else load_projection_locked(path,{},{});
    return;
  }
  if(old.panel==ProjectionPanel::Browser&&command==SpatialCommand::ProjectionParent){browse_projection_locked(old.directory.parent_path());return;}
  auto next=std::make_shared<ProjectionView>(old);
  Transaction transaction(*this);
  if(command==SpatialCommand::ProjectionPrevious||command==SpatialCommand::ProjectionNext){
    const int direction=command==SpatialCommand::ProjectionNext?1:-1;
    if(next->panel==ProjectionPanel::Browser)next->selected=(next->selected+next->entries.size()+direction)%next->entries.size();
    else {next->panel=ProjectionPanel::Parameters;next->parameter=(next->parameter+projection_parameter_count+direction)%projection_parameter_count;}
  }else if(command==SpatialCommand::ProjectionOpen)next->panel=next->panel==ProjectionPanel::Parameters?ProjectionPanel::Tools:ProjectionPanel::Parameters;
  else if(command==SpatialCommand::ProjectionToggle)next->settings.enabled=!next->settings.enabled;
  else if(command==SpatialCommand::ProjectionUsage)next->settings.image.usage=next->settings.image.usage==ProjectionUsage::Color?ProjectionUsage::Stencil:ProjectionUsage::Color;
  else if(command==SpatialCommand::ProjectionParent)next->settings.preview=!next->settings.preview;
  else if(command==SpatialCommand::ProjectionIncrease||command==SpatialCommand::ProjectionDecrease){next->panel=ProjectionPanel::Parameters;adjust_projection_parameter(next->settings,next->parameter,command==SpatialCommand::ProjectionIncrease?1:-1);}
  else if(command==SpatialCommand::ProjectionPlace){
    if(next->placing){next->placing=false;next->placement_before.reset();input_.retain_page(25,next->placement_owner?next->placement_owner:1,7);}
    else {next->placement_before=next->settings;next->placing=true;next->placement_armed=next->placement_pressed=false;next->placement_owner=input_.command_device()?input_.command_device():1;input_.cancel();contact_rearmed_={};}
  }else throw std::invalid_argument("Unknown projection action");
  next->settings.validate();next->error.clear();state_.projection=std::move(next);publish_locked(false,false);transaction.commit(false);
}
bool Document::process_projection_spatial_locked(const SpatialFrame &f){
  if(!state_.projection||!state_.projection->placing)return false;
  auto next=std::make_shared<ProjectionView>(*state_.projection);
  Transaction transaction(*this);
  const size_t hand=next->placement_owner?next->placement_owner-1:0;
  const auto &h=f.hands.at(hand);
  const bool valid=f.focused&&f.head_valid&&ob::finite(f.head)&&std::isfinite(f.time)&&f.time>=0&&h.tracked.valid&&ob::finite(h.tracked.grip)&&std::isfinite(h.tracked.trigger);
  bool finish=false,cancel=!valid||h.joystick;
  if(valid&&!cancel){
    if(h.tracked.trigger<=.02f){if(next->placement_pressed)finish=true;else next->placement_armed=true;}
    else if(next->placement_armed){next->placement_pressed=true;next->settings.camera.pose=ob::compose(ob::inverse(state_.canvas),h.tracked.grip);}
  }
  if(cancel||finish){
    if(cancel&&next->placement_before)next->settings=*next->placement_before;
    next->placing=false;next->placement_before.reset();contact_rearmed_={};
    state_.selection_release={true,true};
    if(valid){input_.retain_page(25,uint32_t(hand+1),7);state_.panel={h.tracked.grip.position,f.head.orientation,1};state_.panel.position=state_.panel.position+ob::rotate(f.head.orientation,{0,.08f,-.18f});}
    else input_.cancel();
  }
  state_.hands=valid?f.hands:std::array<SpatialHand,2>{};
  state_.projection=std::move(next);publish_locked(false,false);transaction.commit(false);return true;
}
} // namespace dome
