#ifndef _EX_H
#define _EX_H

#include <Evas.h>
#include <Ecore.h>
#include <Ecore_X.h>
#include <Ecore_File.h>
#include <Ecore_X_Cursor.h>
#include <Epsilon.h>
#include <etk/Etk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

#include "config.h"

#if HAVE_ENGRAVE
#include <Engrave.h>
#endif

#define ETK_DEFAULT_ICON_SET_FILE ETK_PACKAGE_DATA_DIR "/stock_icons/default.edj"

#define EX_DEFAULT_BLUR_THRESH 2
#define EX_DEFAULT_SHARPEN_THRESH 2
#define EX_DEFAULT_BRIGHTEN_THRESH 100
#define EX_DEFAULT_SLIDE_INTERVAL 5.0
#define EX_DEFAULT_COMMENTS_VISIBLE 0
	    

typedef struct _Exhibit Exhibit;
typedef struct _Ex_Options Ex_Options;
typedef struct _Ex_Tab Ex_Tab;
typedef struct _Ex_Thumb Ex_Thumb;
typedef struct _Ex_Filedialog Ex_Filedialog;

typedef enum _Ex_Images
{
   EX_IMAGE_FIT_TO_WINDOW = -1,
   EX_IMAGE_ONE_TO_ONE = -2,
   EX_IMAGE_ZOOM_IN = -3,
   EX_IMAGE_ZOOM_OUT = -4
} Ex_Images;

struct _Ex_Options
{
   char  *app1;
   char  *app2;
   char  *app3;
   char  *app4;
   
   char  *fav_path;
   
   double blur_thresh;
   double sharpen_thresh;
   double brighten_thresh;
   
   double slide_interval;
   
   int    comments_visible;

	 Ex_Images im_view;
};

struct _Ex_Tab
{
   char          *dir;
   char           cur_path[PATH_MAX];
   char           cur_file[PATH_MAX];   
   int            num;

   Etk_Bool       fit_window;   
   
   Evas_List     *images;
   Evas_List     *dirs;
  
   Etk_Widget    *image;   
   Etk_Widget    *dtree;
   Etk_Widget    *itree;   
   Etk_Widget    *scrolled_view;
   Etk_Widget    *alignment;   
   
   Etk_Tree_Col  *dcol;
   Etk_Tree_Col  *icol;
   
   Exhibit       *e;
};


struct _Exhibit
{
   Etk_Widget    *vbox;
   Etk_Widget    *hbox;
   Etk_Widget    *menu_bar;
   Etk_Widget    *statusbar[4];
   Etk_Widget    *notebook;
   Etk_Widget    *table;
   Etk_Widget    *hpaned;
   Etk_Widget    *vpaned;
   Etk_Widget    *entry[2];
   Etk_Widget    *zoom_in[2];
   Etk_Widget    *zoom_out[2];
   Etk_Widget    *fit[2];
   Etk_Widget    *original[2];
   Etk_Widget    *sort;
   Etk_Widget    *sizebar;
   Etk_Widget    *resbar;
   Etk_Widget    *zoombar;
   Etk_Widget    *menu;
   Etk_Widget    *win;

   struct {
      Etk_Bool    visible;
      Etk_Widget *vbox;
      Etk_Widget *frame;
      Etk_Widget *table;
      Etk_Widget *entry;
      Etk_Widget *save;
      Etk_Widget *revert;
      Etk_Widget *vbox2;
      Etk_Widget *hbox;
      char       *text;
   } comment;
   
   char           fav_path[PATH_MAX];

   int            zoom;
   int            brightness;
   int            contrast;

   Evas_List     *tabs;
   Ex_Tab        *cur_tab;
   
   Ex_Options     *options;   
   
   struct {
      int down;
      int x;
      int y;
   } mouse;
   
   struct {
      double       interval;
      Ecore_Timer *timer;
      Etk_Bool     active;
   } slideshow;   
};

struct _Ex_Thumb
{
   Exhibit  *e;
   char     *name;
   char     *image;
   Etk_Bool  selected;
   Epsilon  *ep;
};

struct _Ex_Filedialog
{
   Etk_Widget *win;
   Etk_Widget *filechooser;
   Etk_Widget *entry;
   Etk_Image  *im;   
};

#define WINDOW_TITLE "Exhibit"
#define WINDOW_WIDTH 700
#define WINDOW_HEIGHT 500
#define ZOOM_MAX 16
#define ZOOM_MIN -16

#ifndef DATA64
#define DATA64 unsigned long long
#define DATA32 unsigned int
#define DATA16 unsigned short
#define DATA8  unsigned char
#endif

#define E_FREE(ptr) do { if(ptr) { free(ptr); ptr = NULL; } } while(0);

#include "exhibit_menus.h"
#include "exhibit_file.h"
#include "exhibit.h"
#include "exhibit_image.h"
#include "exhibit_main.h"
#include "exhibit_menus.h"
#include "exhibit_sort.h"
#include "exhibit_thumb.h"
#include "exhibit_tab.h"
#include "exhibit_slideshow.h"
#include "exhibit_favorites.h"
#include "exhibit_options.h"
#include "exhibit_comment.h"

#endif
