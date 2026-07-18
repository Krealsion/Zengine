#include "config.h"
#include "engine/config_manager.h"
double Zen::Config::graphics::get_ui_scale() {
  return Zen::ConfigManager::instance().get<double>("graphics.ui_scale");}


bool Zen::Config::graphics::get_vsync() {
  return Zen::ConfigManager::instance().get<bool>("graphics.vsync");}


int64_t Zen::Config::ui::get_base_padding() {
  return Zen::ConfigManager::instance().get<int64_t>("ui.base_padding");}


bool Zen::Config::ui::get_enable_animations() {
  return Zen::ConfigManager::instance().get<bool>("ui.enable_animations");}


double Zen::Config::ui::get_font_size() {
  return Zen::ConfigManager::instance().get<double>("ui.font_size");}


std::string Zen::Config::ui::button::get_default_text() {
  return Zen::ConfigManager::instance().get<std::string>("ui.button.default_text");}


