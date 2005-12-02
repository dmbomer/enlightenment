/** @file etk_filechooser_widget.c */
#include "etk_filechooser_widget.h"
#include <stdlib.h>
#include <string.h>
#include <Ecore_Data.h>
#include <Ecore_File.h>
#include "etk_signal.h"
#include "etk_signal_callback.h"
#include "etk_utils.h"
#include "etk_button.h"
#include "etk_tree.h"
#include "etk_paned.h"
#include "etk_box.h"
#include "etk_hbox.h"
#include "etk_vbox.h"
#include "etk_entry.h"

/**
 * @addtogroup Etk_Filechooser_Widget
 * @{
 */

enum _Etk_Filechooser_Widget_Signal_Id
{
   ETK_FILECHOOSER_WIDGET_NUM_SIGNALS
};

enum _Etk_Filechooser_Widget_Property_Id
{
   ETK_FILECHOOSER_WIDGET_PATH_PROPERTY
};

typedef struct _Etk_Filechooser_Widget_Icons
{
   void *extension;
   char *icon;
} Etk_Filechooser_Widget_Icons;

static void _etk_filechooser_widget_constructor(Etk_Filechooser_Widget *filechooser_widget);
static void _etk_filechooser_widget_destructor(Etk_Filechooser_Widget *filechooser_widget);
static void _etk_filechooser_widget_property_set(Etk_Object *object, int property_id, Etk_Property_Value *value);
static void _etk_filechooser_widget_property_get(Etk_Object *object, int property_id, Etk_Property_Value *value);
static void _etk_filechooser_widget_dir_row_selected_cb(Etk_Object *object, Etk_Tree_Row *row, void *data);

static Etk_Filechooser_Widget_Icons _etk_file_chooser_icons[] =
{
   { "jpg", "/home/simon/etk_viewer/data/image.png" },
   { "jpeg", "/home/simon/etk_viewer/data/image.png" },
   { "png", "/home/simon/etk_viewer/data/image.png" },
   { "bmp", "/home/simon/etk_viewer/data/image.png" },
   { "gif", "/home/simon/etk_viewer/data/image.png" },
   { "mp3", "/home/simon/etk_viewer/data/audio.png" },
   { "ogg", "/home/simon/etk_viewer/data/audio.png" },
   { "wav", "/home/simon/etk_viewer/data/audio.png" },
   { "avi", "/home/simon/etk_viewer/data/video.png" },
   { "mpg", "/home/simon/etk_viewer/data/video.png" },
   { "mpeg", "/home/simon/etk_viewer/data/video.png" },
   { "gz", "/home/simon/etk_viewer/data/package.png" },
   { "tgz", "/home/simon/etk_viewer/data/package.png" },
   { "bz2", "/home/simon/etk_viewer/data/package.png" },
   { "tbz2", "/home/simon/etk_viewer/data/package.png" },
   { "zip", "/home/simon/etk_viewer/data/package.png" },
   { "rar", "/home/simon/etk_viewer/data/package.png" },
};
static int _etk_file_chooser_num_icons = sizeof(_etk_file_chooser_icons) / sizeof (_etk_file_chooser_icons[0]);
static Etk_Signal *_etk_filechooser_widget_signals[ETK_FILECHOOSER_WIDGET_NUM_SIGNALS];

/**************************
 *
 * Implementation
 *
 **************************/

/**
 * @brief Gets the type of an Etk_Filechooser_Widget
 * @return Returns the type on an Etk_Filechooser_Widget
 */
Etk_Type *etk_filechooser_widget_type_get()
{
   static Etk_Type *filechooser_widget_type = NULL;

   if (!filechooser_widget_type)
   {
      filechooser_widget_type = etk_type_new("Etk_Filechooser_Widget", ETK_BIN_TYPE, sizeof(Etk_Filechooser_Widget), ETK_CONSTRUCTOR(_etk_filechooser_widget_constructor), ETK_DESTRUCTOR(_etk_filechooser_widget_destructor));

      //_etk_filechooser_widget_signals[ETK_FILECHOOSER_WIDGET_TEXT_POPPED_SIGNAL] = etk_signal_new("text_popped", filechooser_widget_type, -1, etk_marshaller_VOID__INT_POINTER, NULL, NULL);
      
      etk_type_property_add(filechooser_widget_type, "path", ETK_FILECHOOSER_WIDGET_PATH_PROPERTY, ETK_PROPERTY_STRING, ETK_PROPERTY_READABLE_WRITABLE, etk_property_value_string(NULL));
      
      filechooser_widget_type->property_set = _etk_filechooser_widget_property_set;
      filechooser_widget_type->property_get = _etk_filechooser_widget_property_get;
   }

   return filechooser_widget_type;
}

/**
 * @brief Creates a new status bar
 * @return Returns the new status bar widget
 */
Etk_Widget *etk_filechooser_widget_new()
{
   return etk_widget_new(ETK_FILECHOOSER_WIDGET_TYPE, NULL);
}

/* TODO */
void etk_filechooser_widget_current_folder_set(Etk_Filechooser_Widget *filechooser_widget, const char *folder)
{
   Ecore_List *files;
   char *file_name;
   char *file_path = NULL;
   time_t mod_time;
   struct tm *mod_time2;
   char mod_time_string[128];
   
   if (!filechooser_widget)
      return;
   if (!folder && !(folder = getenv("HOME")))
      return;
   if (!(files = ecore_file_ls(folder)))
      return;
   
   free(filechooser_widget->current_folder);
   filechooser_widget->current_folder = strdup(folder);
   etk_tree_clear(ETK_TREE(filechooser_widget->dir_tree));
   etk_tree_clear(ETK_TREE(filechooser_widget->files_tree));
   /* TODO: fix */
   etk_tree_append(ETK_TREE(filechooser_widget->dir_tree), filechooser_widget->dir_col, "/home/simon/etk_viewer/data/up.png", "..", NULL);
   
   ecore_list_goto_first(files);
   while ((file_name = ecore_list_next(files)))
   {
      if (file_name[0] == '.')
         continue;
      
      free(file_path);
      file_path = malloc(strlen(folder) + strlen(file_name) + 2);
      sprintf(file_path, "%s/%s", folder, file_name);
      
      if (!ecore_file_is_dir(file_path))
         continue;
      
      mod_time = ecore_file_mod_time(file_path);
      mod_time2 = gmtime(&mod_time);
      strftime(mod_time_string, 128, "%x", mod_time2);
      
      etk_tree_append(ETK_TREE(filechooser_widget->dir_tree), filechooser_widget->dir_col, "/home/simon/etk_viewer/data/dir.png", file_name, NULL);
      etk_tree_append(ETK_TREE(filechooser_widget->files_tree),
         filechooser_widget->files_name_col, "/home/simon/etk_viewer/data/dir.png", file_name,
         filechooser_widget->files_date_col, mod_time_string, NULL);
   }
   
   ecore_list_goto_first(files);
   while ((file_name = ecore_list_next(files)))
   {
      const char *ext;
      char *icon = NULL;
      int i;
      
      if (file_name[0] == '.')
         continue;
      
      free(file_path);
      file_path = malloc(strlen(folder) + strlen(file_name) + 2);
      sprintf(file_path, "%s/%s", folder, file_name);
      
      if (ecore_file_is_dir(file_path))
         continue;
      
      if ((ext = strrchr(file_name, '.')) && (ext = ext + 1))
      {
         for (i = 0; i < _etk_file_chooser_num_icons; i++)
         {
            if (strcasecmp(ext, _etk_file_chooser_icons[i].extension) == 0)
            {
               icon = _etk_file_chooser_icons[i].icon;
               break;
            }
         }
      }
      mod_time = ecore_file_mod_time(file_path);
      mod_time2 = gmtime(&mod_time);
      strftime(mod_time_string, 128, "%x", mod_time2);
      
      etk_tree_append(ETK_TREE(filechooser_widget->files_tree),
         filechooser_widget->files_name_col, icon ? icon : "/home/simon/etk_viewer/data/file.png", file_name,
         filechooser_widget->files_date_col, mod_time_string, NULL);
   }
   
   free(file_path);
   ecore_list_destroy(files);
}

/**************************
 *
 * Etk specific functions
 *
 **************************/

/* Initializes the members */
static void _etk_filechooser_widget_constructor(Etk_Filechooser_Widget *filechooser_widget)
{
   Etk_Widget *hpaned, *vpaned;
   
   if (!filechooser_widget)
      return;
   
   hpaned = etk_hpaned_new();
   etk_widget_visibility_locked_set(hpaned, TRUE);
   etk_container_add(ETK_CONTAINER(filechooser_widget), hpaned);
   etk_widget_show(hpaned);
   
   vpaned = etk_vpaned_new();
   etk_widget_visibility_locked_set(vpaned, TRUE);
   etk_paned_add1(ETK_PANED(hpaned), vpaned);
   etk_widget_show(vpaned);
   
   filechooser_widget->dir_tree = etk_tree_new();
   etk_widget_visibility_locked_set(filechooser_widget->dir_tree, TRUE);
   etk_widget_size_request_set(filechooser_widget->dir_tree, 180, 240);
   etk_paned_add1(ETK_PANED(vpaned), filechooser_widget->dir_tree);
   filechooser_widget->dir_col = etk_tree_col_new(ETK_TREE(filechooser_widget->dir_tree), "Directories", ETK_TREE_COL_ICON_TEXT, 120);
   etk_tree_build(ETK_TREE(filechooser_widget->dir_tree));
   etk_widget_show(filechooser_widget->dir_tree);
   etk_signal_connect("row_selected", ETK_OBJECT(filechooser_widget->dir_tree), ETK_CALLBACK(_etk_filechooser_widget_dir_row_selected_cb), filechooser_widget);
   
   filechooser_widget->fav_tree = etk_tree_new();
   etk_widget_visibility_locked_set(filechooser_widget->fav_tree, TRUE);
   etk_widget_size_request_set(filechooser_widget->fav_tree, 180, 120);
   etk_paned_add2(ETK_PANED(vpaned), filechooser_widget->fav_tree);
   filechooser_widget->fav_col = etk_tree_col_new(ETK_TREE(filechooser_widget->fav_tree), "Favorites", ETK_TREE_COL_ICON_TEXT, 120);
   etk_tree_build(ETK_TREE(filechooser_widget->fav_tree));
   etk_widget_show(filechooser_widget->fav_tree);
   
   etk_tree_append(ETK_TREE(filechooser_widget->fav_tree), filechooser_widget->fav_col, "/home/simon/etk_viewer/data/root.png", "Root", NULL);
   etk_tree_append(ETK_TREE(filechooser_widget->fav_tree), filechooser_widget->fav_col, "/home/simon/etk_viewer/data/home.png", "Home", NULL);
   etk_tree_append(ETK_TREE(filechooser_widget->fav_tree), filechooser_widget->fav_col, "/home/simon/etk_viewer/data/dir.png", "Musiques", NULL);
   etk_tree_append(ETK_TREE(filechooser_widget->fav_tree), filechooser_widget->fav_col, "/home/simon/etk_viewer/data/dir.png", "Videos", NULL);
   etk_tree_append(ETK_TREE(filechooser_widget->fav_tree), filechooser_widget->fav_col, "/home/simon/etk_viewer/data/dir.png", "Images", NULL);
   
   filechooser_widget->files_tree = etk_tree_new();
   etk_widget_visibility_locked_set(filechooser_widget->files_tree, TRUE);
   etk_widget_size_request_set(filechooser_widget->files_tree, 400, 120);
   etk_paned_add2(ETK_PANED(hpaned), filechooser_widget->files_tree);
   filechooser_widget->files_name_col = etk_tree_col_new(ETK_TREE(filechooser_widget->files_tree), "Filename", ETK_TREE_COL_ICON_TEXT, 300);
   filechooser_widget->files_date_col = etk_tree_col_new(ETK_TREE(filechooser_widget->files_tree), "Date", ETK_TREE_COL_TEXT, 60);
   etk_tree_build(ETK_TREE(filechooser_widget->files_tree));
   etk_widget_show(filechooser_widget->files_tree);
   
   
   filechooser_widget->current_folder = NULL;
   /* Go to home */
   etk_filechooser_widget_current_folder_set(ETK_FILECHOOSER_WIDGET(filechooser_widget), NULL);
}

/* Destroys the status bar */
static void _etk_filechooser_widget_destructor(Etk_Filechooser_Widget *filechooser_widget)
{
   if (!filechooser_widget)
      return;
   free(filechooser_widget->current_folder);
}

/* Sets the property whose id is "property_id" to the value "value" */
static void _etk_filechooser_widget_property_set(Etk_Object *object, int property_id, Etk_Property_Value *value)
{
   Etk_Filechooser_Widget *filechooser_widget;

   if (!(filechooser_widget = ETK_FILECHOOSER_WIDGET(object)) || !value)
      return;

   switch (property_id)
   {/*
      case ETK_FILECHOOSER_WIDGET_HAS_RESIZE_GRIP_PROPERTY:
         etk_filechooser_widget_has_resize_grip_set(filechooser_widget, etk_property_value_bool_get(value));
         break;*/
      default:
         break;
   }
}

/* Gets the value of the property whose id is "property_id" */
static void _etk_filechooser_widget_property_get(Etk_Object *object, int property_id, Etk_Property_Value *value)
{
   Etk_Filechooser_Widget *filechooser_widget;

   if (!(filechooser_widget = ETK_FILECHOOSER_WIDGET(object)) || !value)
      return;

   switch (property_id)
   {/*
      case ETK_FILECHOOSER_WIDGET_HAS_RESIZE_GRIP_PROPERTY:
         etk_property_value_bool_set(value, filechooser_widget->has_resize_grip);
         break;*/
      default:
         break;
   }
}

/**************************
 *
 * Callbacks and handlers
 *
 **************************/

/* Called when a row of the "directories" tree is selected */
static void _etk_filechooser_widget_dir_row_selected_cb(Etk_Object *object, Etk_Tree_Row *row, void *data)
{
   Etk_Filechooser_Widget *filechooser_widget;
   char *selected_dir;
   char *new_dir;
   
   if (!(filechooser_widget = ETK_FILECHOOSER_WIDGET(data)))
      return;
   etk_tree_row_fields_get(row, filechooser_widget->dir_col, NULL, &selected_dir, NULL);
   
   new_dir = malloc(strlen(filechooser_widget->current_folder) + strlen(selected_dir) + 2);
   sprintf(new_dir, "%s/%s", filechooser_widget->current_folder, selected_dir);
   etk_filechooser_widget_current_folder_set(filechooser_widget, new_dir);
   free(new_dir);
}

/**************************
 *
 * Private functions
 *
 **************************/

/** @} */
