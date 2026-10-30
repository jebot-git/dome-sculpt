// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "text-utf8.hpp"
#include <cstdlib>
namespace dome {
ImportSettings Document::import_settings() const {std::lock_guard lock(model_mutex_);return import_view_?import_view_->settings:import_settings_;}
void Document::configure_import(ImportSettings settings) {
  settings.validate();std::lock_guard lock(model_mutex_);
  auto view=import_view_?std::make_shared<ImportView>(*import_view_):nullptr;
  if(view)view->settings=settings;
  import_settings_=settings;if(view)import_view_=std::move(view);publish_locked(false,false);
}
void Document::browse_import_locked(const std::filesystem::path &path) {
  if(stopping_.load())throw std::logic_error("Document is stopping");
  auto view=std::make_shared<ImportView>();view->directory=path;view->settings=import_view_?import_view_->settings:import_settings_;
  view->request=import_request_.load()+1;view->busy=true;
  FileJob job{};job.path=path;job.generation=state_.generation;job.discover_import=true;job.import_request=view->request;
  {std::lock_guard lock(file_mutex_);
    if(files_stopping_||files_.size()>=8)throw std::runtime_error("File work queue is full");
    files_.push_back(std::move(job));
  }
  import_request_.store(view->request);import_view_=std::move(view);publish_locked(false,false);file_changed_.notify_one();
}
void Document::open_import_browser(std::filesystem::path path) {
  std::lock_guard lock(model_mutex_);
  if(path.empty()) {
    if(import_view_)path=import_view_->directory;
    else if(!path_.empty())path=path_.parent_path();
    else if(const char *home=std::getenv("HOME"))path=home;
    else path=".";
  }
  browse_import_locked(path);
}
void Document::discover_import_file(const FileJob &job) {
  auto cancelled=[&]{return stopping_.load() || import_request_.load()!=job.import_request;};
  auto result=scan_import_directory(job.path,cancelled);
  std::lock_guard lock(model_mutex_);
  if(cancelled() || !import_view_ || state_.generation!=job.generation)return;
  result.request=job.import_request;result.settings=import_view_->settings;
  import_view_=std::make_shared<const ImportView>(std::move(result));publish_locked(false,false);
}
void Document::import_listing_error(const FileJob &job,const std::exception &error) {
  std::lock_guard lock(model_mutex_);
  if(stopping_.load() || !import_view_ || import_request_.load()!=job.import_request || state_.generation!=job.generation)return;
  auto view=std::make_shared<ImportView>(*import_view_);view->busy=false;view->error=text::utf8_prefix(error.what(),512);
  import_view_=std::move(view);publish_locked(false,false);
}
bool Document::import_command_enabled_locked(SpatialCommand c) const {
  if(stopping_.load())return false;
  if(c==SpatialCommand::ImportCancel)return bool(import_view_);
  if(control_before_)return false;
  if(c==SpatialCommand::ImportBrowse)return !import_view_ || !import_view_->busy;
  if(!import_view_ || import_view_->busy)return false;
  if(c==SpatialCommand::ImportApply)return !state_.asset_busy && !state_.recovery_busy && !control_before_ &&
      !action_active_locked() && !state_.retopology && !state_.cut &&
      import_view_->selected<import_view_->entries.size() && !import_view_->entries[import_view_->selected].directory;
  if(c==SpatialCommand::ImportOpen)return import_view_->selected<import_view_->entries.size() && import_view_->entries[import_view_->selected].directory;
  if(c==SpatialCommand::ImportPrevious || c==SpatialCommand::ImportNext)return !import_view_->entries.empty();
  return true;
}
void Document::import_command_locked(SpatialCommand c) {
  if(!import_command_enabled_locked(c))return;
  if(c==SpatialCommand::ImportCancel){if(state_.controls.parameter==VrParameter::ImportUnitScale)finish_control_locked(false);++import_request_;import_view_.reset();return;}
  if(c==SpatialCommand::ImportBrowse){
    std::filesystem::path path=import_view_?import_view_->directory:!path_.empty()?path_.parent_path():std::getenv("HOME")?std::getenv("HOME"):".";
    browse_import_locked(path);return;
  }
  if(c==SpatialCommand::ImportOpen){browse_import_locked(import_view_->directory/import_view_->entries[import_view_->selected].name);return;}
  if(c==SpatialCommand::ImportParent){browse_import_locked(import_view_->directory.parent_path());return;}
  if(c==SpatialCommand::ImportApply){auto path=import_view_->directory/import_view_->entries[import_view_->selected].name;import_asset_locked(path,import_view_->settings);return;}
  auto next=std::make_shared<ImportView>(*import_view_);next->error.clear();
  if(c==SpatialCommand::ImportPrevious)next->selected=(next->selected+next->entries.size()-1)%next->entries.size();
  else if(c==SpatialCommand::ImportNext)next->selected=(next->selected+1)%next->entries.size();
  else if(c==SpatialCommand::ImportUnits)next->settings.choices.units=ImportUnits((int(next->settings.choices.units)+1)%5);
  else if(c==SpatialCommand::ImportFormat)next->settings.choices.format=ImportSourceFormat((int(next->settings.choices.format)+1)%5);
  else if(c==SpatialCommand::ImportMaterials)next->settings.choices.materials=!next->settings.choices.materials;
  else if(c==SpatialCommand::ImportDefaults)next->settings={};
  else if(c==SpatialCommand::ImportAxes)next->settings.choices.axes=ImportAxes((int(next->settings.choices.axes)+1)%3);
  else if(c==SpatialCommand::ImportNormals)next->settings.choices.normals=ImportNormals((int(next->settings.choices.normals)+1)%3);
  else if(c==SpatialCommand::ImportResolution)next->settings.paint_resolution=next->settings.paint_resolution==2048?64:next->settings.paint_resolution*2;
  else throw std::invalid_argument("Unknown import action");
  next->settings.validate();import_view_=std::move(next);
}
void Document::import_action(SpatialCommand command) {
  if(!import_command(command))throw std::invalid_argument("Expected an import action");
  std::lock_guard lock(model_mutex_);import_command_locked(command);publish_locked(false,false);
}
} // namespace dome
