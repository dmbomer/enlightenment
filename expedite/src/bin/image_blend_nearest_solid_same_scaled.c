#undef FNAME
#undef NAME
#undef ICON

/* metadata */
#define FNAME image_blend_nearest_solid_same_scaled_start
#define NAME "Image Blend Nearest Solid Same Scaled"
#define ICON "flower.png"

#ifndef PROTO
# ifndef UI
#  include "main.h"

/* standard var */
static int done = 0;
/* private data */
static Evas_Object *o_images[OBNUM];

/* setup */
static void _setup(void)
{
   int i;
   Evas_Object *o;
   for (i = 0; i < OBNUM; i++)
     {
	o = evas_object_image_add(evas);
	o_images[i] = o;
        evas_object_image_file_set(o, build_path("image.png"), NULL);
	evas_object_image_smooth_scale_set(o, 0);
	evas_object_show(o);
     }
   done = 0;
}

/* cleanup */
static void _cleanup(void)
{
   int i;
   for (i = 0; i < OBNUM; i++) evas_object_del(o_images[i]);
}

/* loop - do things */
static void _loop(double t, int f)
{
   int i;
   Evas_Coord x, y, w, h, w0, h0;
   for (i = 0; i < OBNUM; i++)
     {
        w0 = 80;
	h0 = 80;
	w = 40 + ((i % 3) * (w0 / 2));
	h = 40 + ((i % 3) * (h0 / 2));
	x = (win_w / 2) - (w / 2);
	x += sin((double)(f + (i * 13)) / (36.7 * SLOW)) * (w0 / 2);
	y = (win_h / 2) - (h / 2);
	y += cos((double)(f + (i * 28)) / (43.8 * SLOW)) * (h0 / 2);
	evas_object_move(o_images[i], x, y);
	evas_object_resize(o_images[i], w, h);
	evas_object_image_fill_set(o_images[i], 0, 0, w, h);
     }
   FPS_STD(NAME);
}

/* prepend special key handlers if interactive (before STD) */
static void _key(char *key)
{
   KEY_STD;
}












/* template stuff - ignore */
# endif
#endif

#ifdef UI
_ui_menu_item_add(ICON, NAME, FNAME);
#endif

#ifdef PROTO
void FNAME(void);
#endif

#ifndef PROTO
# ifndef UI
void FNAME(void)
{
   ui_func_set(_key, _loop);
   _setup();
}
# endif
#endif
#undef FNAME
#undef NAME
#undef ICON
