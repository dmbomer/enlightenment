#include "engage.h"
#include "config.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

OD_Options      options;

void
od_config_init()
{
  ecore_config_default_int("engage.options.width", 1024);
  ecore_config_default_int("engage.options.height", 100);
  options.icon_path = PACKAGE_DATA_DIR "/icons/";
  ecore_config_default_int_bound("engage.options.mode", OM_ONTOP, 0, 1, 1);

  ecore_config_default_int("engage.options.size", 32);
  ecore_config_default_int("engage.options.spacing", 4);
  ecore_config_default_float("engage.options.zoom_factor", 2.0);
  ecore_config_default_int("engage.options.arrow_size", 6);
  ecore_config_default_float("engage.options.zoom_duration", 0.1);

  options.tt_txt_color = 0xffffffff;
  options.tt_shd_color = 0x7f000000;
  options.bg_fore = 0x7f000000;
  options.bg_back = 0x7fffffff;
  ecore_config_default_string("engage.options.tt_fa", "Vera");
  ecore_config_default_int("engage.options.tt_fs", 8);
  ecore_config_default_float("engage.options.icon_appear_duration", 0.1);

  ecore_config_load();
  options.width = ecore_config_get_int("engage.options.width");
  options.height = ecore_config_get_int("engage.options.height");
  options.mode = ecore_config_get_int("engage.options.mode");

  options.size = ecore_config_get_int("engage.options.size");
  options.spacing = ecore_config_get_int("engage.options.spacing");
  options.zoomfactor = ecore_config_get_float("engage.options.zoom_factor");
  options.arrow_size = ecore_config_get_int("engage.options.arrow_size");
  options.dock_zoom_duration =
    ecore_config_get_float("engage.options.zoom_duration");

  options.tt_fa = ecore_config_get_string("engage.options.tt_fa");
  options.tt_fs = ecore_config_get_int("engage.options.tt_fs");
  options.icon_appear_duration =
    ecore_config_get_float("engage.options.icon_appear_duration");

}
