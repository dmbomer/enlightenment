#include <stdlib.h>
#include <Edb.h>
#include <gdk/gdkx.h>
#include "preferences.h"
#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "bits.h"

extern gint       render_method;
extern gint       zoom_method;
extern GtkWidget *main_win;
extern char       etcher_config[4096];

GtkWidget *dialog;
GtkWidget *color_dialog;

/* previous context */
Imlib_Image im_old;
Pixmap pm_old;

Window win;
DATA8  colors[3] = {255, 255, 255};
DATA8  colors_old[3];
char   etcher_config[4096];

static void pref_update_preview(void);
static void pref_colors_ok(GtkWidget *widget, gpointer data);
static void pref_colors_cancel(GtkWidget *widget,  gpointer data);

static void 
pref_update_preview(void)
{
  static Pixmap pm;
  static gboolean first = TRUE;
  static Imlib_Image grid_im = NULL;
  static Imlib_Color_Modifier colormod = NULL;
  static int w, h, ww, hh;

  DATA8 rt[256], gt[256], bt[256], at[256];
  Imlib_Image im;
  int i, d;

  GtkWidget * wid;

  if (first)
    {
      first = FALSE;

      wid = gtk_object_get_data(GTK_OBJECT(dialog), "pref_da");
      grid_im = imlib_load_image(PACKAGE_DATA_DIR"/pixmaps/tile.png");
      d = imlib_get_visual_depth(imlib_context_get_display(), imlib_context_get_visual());
      colormod = imlib_create_color_modifier();

      imlib_context_set_image(grid_im);
      
      ww = imlib_image_get_width();
      hh = imlib_image_get_height();	
      w  = wid->allocation.width;
      h  = wid->allocation.height;

      pm = XCreatePixmap(imlib_context_get_display(), win, w, h, d);
    }
      
  imlib_context_set_image(grid_im);
  imlib_context_set_drawable(pm);    
  imlib_context_set_color_modifier(colormod);   
  imlib_get_color_modifier_tables(rt, gt, bt, at);

  im = imlib_clone_image();
  imlib_context_set_image(im);

  for (i = 0; i<256; i++)
    {
      rt[i] = (DATA8)(i * ((double)colors[0] / 256));
      gt[i] = (DATA8)(i * ((double)colors[1] / 256));
      bt[i] = (DATA8)(i * ((double)colors[2] / 256));
      at[i] = 255;
    }

  imlib_set_color_modifier_tables(rt, gt, bt, at);
  imlib_apply_color_modifier();
  
  imlib_render_image_on_drawable(0, 0);

  XSetWindowBackgroundPixmap(imlib_context_get_display(), win, pm);
  XClearWindow(imlib_context_get_display(), win);
  imlib_free_image();
}


static void
pref_colors_ok(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(color_dialog);
}


static void 
pref_colors_cancel(GtkWidget *widget,  gpointer data)
{
  colors[0] = colors_old[0];
  colors[1] = colors_old[1];
  colors[2] = colors_old[2];

  pref_update_preview();

  gtk_widget_hide(color_dialog);
}


int
pref_init(void)
{
  int config_ok = 0, config_zoom_method, config_render_method;
  int col_r, col_g, col_b;

  g_snprintf(etcher_config, sizeof(etcher_config), "%s/.etcher.db", getenv("HOME"));
  E_DB_INT_GET(etcher_config, "/display/zoom_method", 
	       config_zoom_method, config_ok);

  E_DB_INT_GET(etcher_config, "/display/render_method", 
	       config_render_method, config_ok);

  if (!config_ok)
    return 0;

  render_method = config_render_method;
  zoom_method = config_zoom_method;

  E_DB_INT_GET(etcher_config, "/grid/r", col_r, config_ok);
  if (config_ok)
    colors[0] = (DATA8)col_r;
  E_DB_INT_GET(etcher_config, "/grid/g", col_g, config_ok);
  if (config_ok)
    colors[1] = (DATA8)col_g;
  E_DB_INT_GET(etcher_config, "/grid/b", col_b, config_ok);
  if (config_ok)
    colors[2] = (DATA8)col_b;

  return 1;
}


void
pref_preferences1_activate               (GtkMenuItem     *menuitem,
					  gpointer         user_data)
{
   GtkWidget *wid;
   
   dialog = create_preferences();
   
   if (render_method == 0)
     {
	wid = gtk_object_get_data(GTK_OBJECT(dialog), "render1");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wid), 1);
     }
   else
     {
	wid = gtk_object_get_data(GTK_OBJECT(dialog), "render2");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wid), 1);
     }

   if (zoom_method == 0)
     {
	wid = gtk_object_get_data(GTK_OBJECT(dialog), "zoom1");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wid), 1);
     }
   else
     {
	wid = gtk_object_get_data(GTK_OBJECT(dialog), "zoom2");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wid), 1);
     }
   
   wid = gtk_object_get_data(GTK_OBJECT(dialog), "pref_da");
   gtk_widget_realize(wid);
   win = GDK_WINDOW_XWINDOW(wid->window);

   pref_update_preview();

   gtk_widget_show(dialog);
}



void
pref_ok_clicked                          (GtkButton       *button,
					  gpointer         user_data)
{
   GtkWidget *top, *w;
   
   top = gtk_widget_get_toplevel(GTK_WIDGET(button));
   w = gtk_object_get_data(GTK_OBJECT(top), "render1");
   if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
      render_method = 0;
   else
      render_method = 1;
   w = gtk_object_get_data(GTK_OBJECT(top), "zoom1");
   if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
      zoom_method = 0;
   else
      zoom_method = 1;
   
   E_DB_INT_SET(etcher_config, "/display/render_method", (int)render_method);
   E_DB_INT_SET(etcher_config, "/display/zoom_method", (int)zoom_method);
   e_db_flush();

   imlib_context_set_image(im_old);
   imlib_context_set_drawable(pm_old);

   gtk_widget_destroy(top);
}


void
pref_cancel_clicked                      (GtkButton       *button,
					  gpointer         user_data)
{
   GtkWidget *top;

   imlib_context_set_image(im_old);
   imlib_context_set_drawable(pm_old);

   top = gtk_widget_get_toplevel(GTK_WIDGET(button));
   gtk_widget_destroy(top);
}


void
pref_tintcolor_button_clicked            (GtkButton       *button,
					  gpointer         user_data)
{
  GtkWidget * colorsel;

  if (!color_dialog)
    {
      color_dialog = gtk_color_selection_dialog_new("Select Tint Color");
    }
  
  colorsel = GTK_COLOR_SELECTION_DIALOG(color_dialog)->colorsel;
  gtk_signal_connect(GTK_OBJECT(colorsel), "color_changed",
		     (GtkSignalFunc)pref_color_changed, (gpointer)colorsel);
  gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(color_dialog)->ok_button), "clicked",
		     (GtkSignalFunc)pref_colors_ok, (gpointer)NULL);
  gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(color_dialog)->cancel_button), "clicked",
		     (GtkSignalFunc)pref_colors_cancel, (gpointer)NULL);

  colors_old[0] = colors[0];
  colors_old[1] = colors[1];
  colors_old[2] = colors[2];

  gtk_widget_show(color_dialog);
}


void
pref_color_changed(GtkWidget *widget, GtkColorSelection * colorsel)
{
  gdouble color[3];
  
  gtk_color_selection_get_color(colorsel, color);

  colors[0] = (DATA8)(255 * color[0]);
  colors[1] = (DATA8)(255 * color[1]);
  colors[2] = (DATA8)(255 * color[2]);

  E_DB_INT_SET(etcher_config, "/grid/r", colors[0]);
  E_DB_INT_SET(etcher_config, "/grid/g", colors[1]);
  E_DB_INT_SET(etcher_config, "/grid/b", colors[2]);
  e_db_flush();

  pref_update_preview();
}
