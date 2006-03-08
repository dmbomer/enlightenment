/*
 * vim:ts=8:sw=3:sts=8:noexpandtab:cino=>5n-3f0^-2{2
 */
#include "config.h"
#include "Ecore.h"
#include "ecore_private.h"
#include "ecore_evas_private.h"
#include "Ecore_Evas.h"
#ifdef BUILD_ECORE_X
#include "Ecore_X.h"
#endif

#ifdef BUILD_ECORE_X
static int _ecore_evas_init_count = 0;

static int _ecore_evas_fps_debug = 0;

static Ecore_Evas *ecore_evases = NULL;
static Evas_Hash *ecore_evases_hash = NULL;
static Ecore_Event_Handler *ecore_evas_event_handlers[16];
static Ecore_Idle_Enterer *ecore_evas_idle_enterer = NULL;

#ifdef BUILD_ECORE_EVAS_GL
static Ecore_X_Window
_ecore_evas_x_gl_window_new(Ecore_Evas *ee, Ecore_X_Window parent, int x, int y, int w, int h, int override)
{
   Evas_Engine_Info_GL_X11 *einfo;
   Ecore_X_Window win;
   
   einfo = (Evas_Engine_Info_GL_X11 *)evas_engine_info_get(ee->evas);
   if (einfo)
     {
	XSetWindowAttributes attr;
	int screen;

	/* FIXME: this is inefficient as its a round trip */
	screen = DefaultScreen(ecore_x_display_get());
	if (ScreenCount(ecore_x_display_get()) > 1)
	  {
	     Ecore_X_Window *roots;
	     int num, i;
	     
	     num = 0;
	     roots = ecore_x_window_root_list(&num);
	     if (roots)
	       {
		  XWindowAttributes at;
		  
		  if (XGetWindowAttributes(ecore_x_display_get(),
					   parent, &at))
		    {
		       for (i = 0; i < num; i++)
			 {
			    if (at.root == roots[i])
			      {
				 screen = i;
				 break;
			      }
			 }
		    }
		  free(roots);
	       }
	  }
	attr.backing_store = NotUseful;
	attr.override_redirect = override;
	attr.colormap = einfo->func.best_colormap_get(ecore_x_display_get(), screen);
	attr.border_pixel = 0;
	attr.background_pixmap = None;
	attr.event_mask =
	  KeyPressMask | KeyReleaseMask |
	  ExposureMask | ButtonPressMask | ButtonReleaseMask | 
	  EnterWindowMask | LeaveWindowMask |
	  PointerMotionMask | StructureNotifyMask | VisibilityChangeMask |
	  FocusChangeMask | PropertyChangeMask | ColormapChangeMask;
	attr.bit_gravity = ForgetGravity;
	
	win = 
	  XCreateWindow(ecore_x_display_get(), 
			parent,
			x, y, 
			w, h, 0,
			einfo->func.best_depth_get(ecore_x_display_get(), screen),
			InputOutput,
			einfo->func.best_visual_get(ecore_x_display_get(), screen),
			CWBackingStore | CWColormap |
			CWBackPixmap | CWBorderPixel |
			CWBitGravity | CWEventMask |
			CWOverrideRedirect,
			&attr);
	einfo->info.display  = ecore_x_display_get();
	einfo->info.visual   = einfo->func.best_visual_get(ecore_x_display_get(), screen);
	einfo->info.colormap = einfo->func.best_colormap_get(ecore_x_display_get(), screen);
	einfo->info.drawable = win;
	einfo->info.depth    = einfo->func.best_depth_get(ecore_x_display_get(), screen);
	evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
     }
   return win;
}
#endif

static void
_ecore_evas_x_render(Ecore_Evas *ee)
{
#ifdef BUILD_ECORE_EVAS_BUFFER
   Evas_List *ll;
#endif
   
   if (ee->delete_idle_enterer) return;
#ifdef BUILD_ECORE_EVAS_BUFFER
   for (ll = ee->sub_ecore_evas; ll; ll = ll->next)
     {
	Ecore_Evas *ee2;
	
	ee2 = ll->data;
	if (ee2->delete_idle_enterer) continue;
	if (ee2->func.fn_pre_render) ee2->func.fn_pre_render(ee2);
	_ecore_evas_buffer_render(ee2);
	if (ee2->func.fn_post_render) ee2->func.fn_post_render(ee2);
     }
#endif	
   if (ee->func.fn_pre_render) ee->func.fn_pre_render(ee);
   if (ee->prop.avoid_damage)
     {
	Evas_List *updates, *l;
	
	updates = evas_render_updates(ee->evas);
#if 0
	for (l = updates; l; l = l->next)
	  {
	     Evas_Rectangle *r;
	     
	     r = l->data;
	     printf("DMG render [%p] %ix%i, [%i %i %ix%i]\n",
		    ee, ee->w, ee->h, r->x, r->y, r->w, r->h);
	  }
#endif		       
	if (ee->engine.x.using_bg_pixmap)
	  {
	     if (updates)
	       {
		  for (l = updates; l; l = l->next)
		    {
		       Evas_Rectangle *r;
		       
		       r = l->data;
		       ecore_x_window_area_clear(ee->engine.x.win, r->x, r->y, r->w, r->h);
		    }
		  if ((ee->shaped) && (updates))
		    ecore_x_window_shape_mask_set(ee->engine.x.win, ee->engine.x.mask);
	       }
	     if (updates) evas_render_updates_free(updates);
	  }
	else
	  {
	     for (l = updates; l; l = l->next)
	       {
		  Evas_Rectangle *r;
		  XRectangle xr;
		  Region tmpr;
		  
		  if (!ee->engine.x.damages)
		    ee->engine.x.damages = XCreateRegion();
		  r = l->data;
		  tmpr = XCreateRegion();
		  if (ee->rotation == 0)
		    {
		       xr.x = r->x;
		       xr.y = r->y;
		       xr.width = r->w;
		       xr.height = r->h;
		    }
		  else if (ee->rotation == 90)
		    {
		       xr.x = r->y;
		       xr.y = ee->h - r->x - r->w;
		       xr.width = r->h;
		       xr.height = r->w;
		    }
		  else if (ee->rotation == 180)
		    {
		       xr.x = ee->w - r->x - r->w;
		       xr.y = ee->h - r->y - r->h;
		       xr.width = r->w;
		       xr.height = r->h;
		    }
		  else if (ee->rotation == 270)
		    {
		       xr.x = ee->w - r->y - r->h;
		       xr.y = r->x;
		       xr.width = r->h;
		       xr.height = r->w;
		    }
		  XUnionRectWithRegion(&xr, ee->engine.x.damages, tmpr);
		  XDestroyRegion(ee->engine.x.damages);
		  ee->engine.x.damages = tmpr;
	       }
	     if (ee->engine.x.damages)
	       {
		  if ((ee->shaped) && (updates))
		    ecore_x_window_shape_mask_set(ee->engine.x.win, ee->engine.x.mask);
		  XSetRegion(ecore_x_display_get(), ee->engine.x.gc, ee->engine.x.damages);
		  /* debug rendering */
		  /*		  
		   XSetForeground(ecore_x_display_get(), ee->engine.x.gc, rand());
		   XFillRectangle(ecore_x_display_get(), ee->engine.x.win, ee->engine.x.gc,
		   0, 0, ee->w, ee->h);
		   XSync(ecore_x_display_get(), False);
		   usleep(20000);
		   XSync(ecore_x_display_get(), False);
		   */
		  ecore_x_pixmap_paste(ee->engine.x.pmap, ee->engine.x.win, ee->engine.x.gc,
				       0, 0, ee->w, ee->h, 0, 0);
		  XDestroyRegion(ee->engine.x.damages);
		  ee->engine.x.damages = 0;
	       }
	     if (updates) evas_render_updates_free(updates);
	  }
     }
   else if ((ee->visible) || 
	    ((ee->should_be_visible) && (ee->prop.fullscreen)) ||
	    ((ee->should_be_visible) && (ee->prop.override)))
     {
	if (ee->shaped)
	  {
	     Evas_List *updates;
	     
	     updates = evas_render_updates(ee->evas);
	     if (updates)
	       {
		  ecore_x_window_shape_mask_set(ee->engine.x.win, ee->engine.x.mask);
		  evas_render_updates_free(updates);
	       }
	  }
	else
	  {
	     Evas_List *updates;
	     
	     updates = evas_render_updates(ee->evas);
	     if (updates)
	       {
#if 0
		  Evas_List *l;
		  
		  printf("RENDER [%p] [%i] %ix%i\n",
			 ee, ee->visible, ee->w, ee->h);
		  for (l = updates; l; l = l->next)
		    {
		       Evas_Rectangle *r;
		       
		       r = l->data;
		       printf("   render [%i %i %ix%i]\n",
			      r->x, r->y, r->w, r->h);
		    }
#endif		       
		  evas_render_updates_free(updates);
	       }
	  }
     }
   if (ee->func.fn_post_render) ee->func.fn_post_render(ee);
}

static void
_ecore_evas_x_mouse_move_process(Ecore_Evas *ee, int x, int y, unsigned int timestamp)
{
   ee->mouse.x = x;
   ee->mouse.y = y;
   if (ee->prop.cursor.object)
     {
	evas_object_show(ee->prop.cursor.object);
	if (ee->rotation == 0)
	  evas_object_move(ee->prop.cursor.object, 
			   x - ee->prop.cursor.hot.x,
			   y - ee->prop.cursor.hot.y);
	else if (ee->rotation == 90)
	  evas_object_move(ee->prop.cursor.object, 
			   ee->h - y - 1 - ee->prop.cursor.hot.x,
			   x - ee->prop.cursor.hot.y);
	else if (ee->rotation == 180)
	  evas_object_move(ee->prop.cursor.object, 
			   ee->w - x - 1 - ee->prop.cursor.hot.x,
			   ee->h - y - 1 - ee->prop.cursor.hot.y);
	else if (ee->rotation == 270)
	  evas_object_move(ee->prop.cursor.object, 
			   y - ee->prop.cursor.hot.x,
			   ee->w - x - 1 - ee->prop.cursor.hot.y);
     }
   if (ee->rotation == 0)
     evas_event_feed_mouse_move(ee->evas, x, y, timestamp, NULL);
   else if (ee->rotation == 90)
     evas_event_feed_mouse_move(ee->evas, ee->h - y - 1, x, timestamp, NULL);
   else if (ee->rotation == 180)
     evas_event_feed_mouse_move(ee->evas, ee->w - x - 1, ee->h - y - 1, timestamp, NULL);
   else if (ee->rotation == 270)
     evas_event_feed_mouse_move(ee->evas, y, ee->w - x - 1, timestamp, NULL);
}

static char *
_ecore_evas_x_winid_str_get(Ecore_X_Window win)
{
   const char *vals = "qWeRtYuIoP5-$&<~";
   static char id[9];
   unsigned int val;
   
   val = (unsigned int)win;
   id[0] = vals[(val >> 28) & 0xf];
   id[1] = vals[(val >> 24) & 0xf];
   id[2] = vals[(val >> 20) & 0xf];
   id[3] = vals[(val >> 16) & 0xf];
   id[4] = vals[(val >> 12) & 0xf];
   id[5] = vals[(val >>  8) & 0xf];
   id[6] = vals[(val >>  4) & 0xf];
   id[7] = vals[(val      ) & 0xf];
   id[8] = 0;
   return id;
}

static Ecore_Evas *
_ecore_evas_x_match(Ecore_X_Window win)
{
   Ecore_Evas *ee;
   
   ee = evas_hash_find(ecore_evases_hash, _ecore_evas_x_winid_str_get(win));
   if ((ee) && (ee->delete_idle_enterer)) return NULL;
   return ee;
}

static void
_ecore_evas_x_resize_shape(Ecore_Evas *ee)
{
   /* BLAH */
   if (!strcmp(ee->driver, "software_x11"))
     {   
#ifdef BUILD_ECORE_X
	Evas_Engine_Info_Software_X11 *einfo;
	
	einfo = (Evas_Engine_Info_Software_X11 *)evas_engine_info_get(ee->evas);
	if (einfo)
	  {
	     GC gc;
	     XGCValues gcv;
	     
	     if (ee->engine.x.mask) ecore_x_pixmap_del(ee->engine.x.mask);
	     ee->engine.x.mask = ecore_x_pixmap_new(ee->engine.x.win, ee->w, ee->h, 1);
	     gcv.foreground = 0;
	     gc = XCreateGC(ecore_x_display_get(), ee->engine.x.mask,
			    GCForeground,
			    &gcv);
	     XFillRectangle(ecore_x_display_get(), ee->engine.x.mask, gc,
			    0, 0, ee->w, ee->h);
	     XFreeGC(ecore_x_display_get(), gc);
	     einfo->info.mask = ee->engine.x.mask;
	     evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
	     evas_damage_rectangle_add(ee->evas, 0, 0, ee->w, ee->h);
	     
	  }
#endif	
     }
   else if (!strcmp(ee->driver, "xrender_x11"))
     {   
#ifdef BUILD_ECORE_X
	Evas_Engine_Info_XRender_X11 *einfo;
	
	einfo = (Evas_Engine_Info_XRender_X11 *)evas_engine_info_get(ee->evas);
	if (einfo)
	  {
	     GC gc;
	     XGCValues gcv;
	     
	     if (ee->engine.x.mask) ecore_x_pixmap_del(ee->engine.x.mask);
	     ee->engine.x.mask = ecore_x_pixmap_new(ee->engine.x.win, ee->w, ee->h, 1);
	     gcv.foreground = 0;
	     gc = XCreateGC(ecore_x_display_get(), ee->engine.x.mask,
			    GCForeground,
			    &gcv);
	     XFillRectangle(ecore_x_display_get(), ee->engine.x.mask, gc,
			    0, 0, ee->w, ee->h);
	     XFreeGC(ecore_x_display_get(), gc);
	     einfo->info.mask = ee->engine.x.mask;
	     evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
	     evas_damage_rectangle_add(ee->evas, 0, 0, ee->w, ee->h);
	     
	  }
#endif	
     }
}

static void
_ecore_evas_x_modifier_locks_update(Ecore_Evas *ee, int modifiers)
{
   if (modifiers & ECORE_X_MODIFIER_SHIFT)
     evas_key_modifier_on(ee->evas, "Shift");
   else
     evas_key_modifier_off(ee->evas, "Shift");
   if (modifiers & ECORE_X_MODIFIER_CTRL)
     evas_key_modifier_on(ee->evas, "Control");
   else
     evas_key_modifier_off(ee->evas, "Control");
   if (modifiers & ECORE_X_MODIFIER_ALT)
     evas_key_modifier_on(ee->evas, "Alt");
   else
     evas_key_modifier_off(ee->evas, "Alt");
   if (modifiers & ECORE_X_MODIFIER_WIN)
     {
	evas_key_modifier_on(ee->evas, "Super");
	evas_key_modifier_on(ee->evas, "Hyper");
     }
   else
     {
	evas_key_modifier_off(ee->evas, "Super");
	evas_key_modifier_off(ee->evas, "Hyper");
     }
   if (modifiers & ECORE_X_LOCK_SCROLL)
     evas_key_lock_on(ee->evas, "Scroll_Lock");
   else
     evas_key_lock_off(ee->evas, "Scroll_Lock");
   if (modifiers & ECORE_X_LOCK_NUM)
     evas_key_lock_on(ee->evas, "Num_Lock");
   else
     evas_key_lock_off(ee->evas, "Num_Lock");
   if (modifiers & ECORE_X_LOCK_CAPS)
     evas_key_lock_on(ee->evas, "Caps_Lock");
   else
     evas_key_lock_off(ee->evas, "Caps_Lock");   
}

static int
_ecore_evas_x_event_key_down(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Key_Down *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if ((!ee) || (ee->ignore_events)) return 1; /* pass on event */
   _ecore_evas_x_modifier_locks_update(ee, e->modifiers);
   evas_event_feed_key_down(ee->evas, e->keyname, e->keysymbol, e->key_compose, NULL, e->time, NULL);
   return 1;
}

static int
_ecore_evas_x_event_key_up(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Key_Up *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if ((!ee) || (ee->ignore_events)) return 1; /* pass on event */
   _ecore_evas_x_modifier_locks_update(ee, e->modifiers);
   evas_event_feed_key_up(ee->evas, e->keyname, e->keysymbol, e->key_compose, NULL, e->time, NULL);
   return 1;
}

static int
_ecore_evas_x_event_mouse_button_down(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Mouse_Button_Down *e;
   Evas_Button_Flags flags = EVAS_BUTTON_NONE;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if ((!ee) || (ee->ignore_events)) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   _ecore_evas_x_modifier_locks_update(ee, e->modifiers);
   if (e->double_click) flags |= EVAS_BUTTON_DOUBLE_CLICK;
   if (e->triple_click) flags |= EVAS_BUTTON_TRIPLE_CLICK;
   evas_event_feed_mouse_down(ee->evas, e->button, flags, e->time, NULL);
   return 1;
}

static int
_ecore_evas_x_event_mouse_button_up(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Mouse_Button_Up *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if ((!ee) || (ee->ignore_events)) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   _ecore_evas_x_modifier_locks_update(ee, e->modifiers);   
   evas_event_feed_mouse_up(ee->evas, e->button, EVAS_BUTTON_NONE, e->time, NULL);
   return 1;
}

static int
_ecore_evas_x_event_mouse_wheel(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Mouse_Wheel *e;

   e = event;
   ee = _ecore_evas_x_match(e->win);
   if ((!ee) || (ee->ignore_events)) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   _ecore_evas_x_modifier_locks_update(ee, e->modifiers);
   evas_event_feed_mouse_wheel(ee->evas, e->direction, e->z, e->time, NULL);

   return 1;
}

static int
_ecore_evas_x_event_mouse_move(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Mouse_Move *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if ((!ee) || (ee->ignore_events)) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   _ecore_evas_x_modifier_locks_update(ee, e->modifiers);
   _ecore_evas_x_mouse_move_process(ee, e->x, e->y, e->time);
   return 1;
}

static int
_ecore_evas_x_event_mouse_in(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Mouse_In *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if ((!ee) || (ee->ignore_events)) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
/*   
     {   
	time_t t;
	char *ct;
	
	const char *modes[] = {
	   "MODE_NORMAL",
	     "MODE_WHILE_GRABBED",
	     "MODE_GRAB",
	     "MODE_UNGRAB"
	};
	const char *details[] = {
	   "DETAIL_ANCESTOR",
	     "DETAIL_VIRTUAL",
	     "DETAIL_INFERIOR",
	     "DETAIL_NON_LINEAR",
	     "DETAIL_NON_LINEAR_VIRTUAL",
	     "DETAIL_POINTER",
	     "DETAIL_POINTER_ROOT",
	     "DETAIL_DETAIL_NONE"
	};
	t = time(NULL);
	ct = ctime(&t);
	ct[strlen(ct) - 1] = 0;
	printf("@@ ->IN 0x%x 0x%x %s md=%s dt=%s\n",
	       e->win, e->event_win,
	       ct,
	       modes[e->mode],
	       details[e->detail]);
     }
 */
// disable. causes mroe problems than it fixes   
//   if ((e->mode == ECORE_X_EVENT_MODE_GRAB) || 
//       (e->mode == ECORE_X_EVENT_MODE_UNGRAB))
//     return 0;
/* if (e->mode != ECORE_X_EVENT_MODE_NORMAL) return 0; */
   if (ee->func.fn_mouse_in) ee->func.fn_mouse_in(ee);
   _ecore_evas_x_modifier_locks_update(ee, e->modifiers);
   evas_event_feed_mouse_in(ee->evas, e->time, NULL);
   _ecore_evas_x_mouse_move_process(ee, e->x, e->y, e->time);
   return 1;
}

static int
_ecore_evas_x_event_mouse_out(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Mouse_Out *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if ((!ee) || (ee->ignore_events)) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
/*   
     {   
	time_t t;
	char *ct;
	
	const char *modes[] = {
	   "MODE_NORMAL",
	     "MODE_WHILE_GRABBED",
	     "MODE_GRAB",
	     "MODE_UNGRAB"
	};
	const char *details[] = {
	   "DETAIL_ANCESTOR",
	     "DETAIL_VIRTUAL",
	     "DETAIL_INFERIOR",
	     "DETAIL_NON_LINEAR",
	     "DETAIL_NON_LINEAR_VIRTUAL",
	     "DETAIL_POINTER",
	     "DETAIL_POINTER_ROOT",
	     "DETAIL_DETAIL_NONE"
	};
	t = time(NULL);
	ct = ctime(&t);
	ct[strlen(ct) - 1] = 0;
	printf("@@ ->OUT 0x%x 0x%x %s md=%s dt=%s\n",
	       e->win, e->event_win,
	       ct,
	       modes[e->mode],
	       details[e->detail]);
     }
 */ 
   if ((e->mode == ECORE_X_EVENT_MODE_GRAB) ||
       (e->mode == ECORE_X_EVENT_MODE_UNGRAB))
     return 0;
/* if (e->mode != ECORE_X_EVENT_MODE_NORMAL) return 0; */
   _ecore_evas_x_modifier_locks_update(ee, e->modifiers);   
   _ecore_evas_x_mouse_move_process(ee, e->x, e->y, e->time);
   evas_event_feed_mouse_out(ee->evas, e->time, NULL);
   if (ee->func.fn_mouse_out) ee->func.fn_mouse_out(ee);
   if (ee->prop.cursor.object) evas_object_hide(ee->prop.cursor.object);
   return 1;
}

static int
_ecore_evas_x_event_window_focus_in(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Window_Focus_In *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if ((!ee) || (ee->ignore_events)) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   ee->prop.focused = 1;
   if (ee->func.fn_focus_in) ee->func.fn_focus_in(ee);
   return 1;
}

static int
_ecore_evas_x_event_window_focus_out(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Window_Focus_Out *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if ((!ee) || (ee->ignore_events)) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   if (ee->prop.fullscreen)
     ecore_x_window_focus(ee->engine.x.win);
   ee->prop.focused = 0;
   if (ee->func.fn_focus_out) ee->func.fn_focus_out(ee);
   return 1;
}

static int
_ecore_evas_x_event_window_damage(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Window_Damage *e;

   e = event;
   ee = _ecore_evas_x_match(e->win);
   if (!ee) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   if (ee->engine.x.using_bg_pixmap) return 1;
   if (ee->prop.avoid_damage)
     {
	XRectangle xr;
	Region tmpr;
	
	if (!ee->engine.x.damages) ee->engine.x.damages = XCreateRegion();
	tmpr = XCreateRegion();
	xr.x = e->x;
	xr.y = e->y;
	xr.width = e->w;
	xr.height = e->h;
	XUnionRectWithRegion(&xr, ee->engine.x.damages, tmpr);
	XDestroyRegion(ee->engine.x.damages);
	ee->engine.x.damages = tmpr;
     }
   else
     {
	if (ee->rotation == 0)
	  evas_damage_rectangle_add(ee->evas, 
				    e->x, 
				    e->y, 
				    e->w, e->h);
	else if (ee->rotation == 90)
	  evas_damage_rectangle_add(ee->evas, 
				    ee->h - e->y - e->h, 
				    e->x, 
				    e->h, e->w);
	else if (ee->rotation == 180)
	  evas_damage_rectangle_add(ee->evas, 
				    ee->w - e->x - e->w, 
				    ee->h - e->y - e->h, 
				    e->w, e->h);
	else if (ee->rotation == 270)
	  evas_damage_rectangle_add(ee->evas, 
				    e->y, 
				    ee->w - e->x - e->w, 
				    e->h, e->w);
     }
   return 1;
}

static int
_ecore_evas_x_event_window_destroy(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Window_Destroy *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if (!ee) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   if (ee->func.fn_destroy) ee->func.fn_destroy(ee);
   ecore_evas_free(ee);
   return 1;
}

static int
_ecore_evas_x_event_window_configure(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Window_Configure *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if (!ee) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   if (ee->engine.x.direct_resize) return 1;
 
   if ((e->from_wm) || (ee->prop.override))
     {
	if ((ee->x != e->x) || (ee->y != e->y))
	  {
	     ee->x = e->x;
	     ee->y = e->y;
	     if (ee->func.fn_move) ee->func.fn_move(ee);	     
	  }
     }
   if ((ee->w != e->w) || (ee->h != e->h))
     {
	ee->w = e->w;
	ee->h = e->h;
	if ((ee->rotation == 90) || (ee->rotation == 270))
	  {
	     evas_output_size_set(ee->evas, ee->h, ee->w);
	     evas_output_viewport_set(ee->evas, 0, 0, ee->h, ee->w);
	  }
	else
	  {
	     evas_output_size_set(ee->evas, ee->w, ee->h);
	     evas_output_viewport_set(ee->evas, 0, 0, ee->w, ee->h);
	  }
	if (ee->prop.avoid_damage)
	  {
	     ecore_evas_avoid_damage_set(ee, 0);
	     ecore_evas_avoid_damage_set(ee, 1);
	  }
	if (ee->shaped)
	  _ecore_evas_x_resize_shape(ee);
	if ((ee->expecting_resize.w > 0) &&
	    (ee->expecting_resize.h > 0))
	  {
	     if ((ee->expecting_resize.w == ee->w) &&
		 (ee->expecting_resize.h == ee->h))
	       _ecore_evas_x_mouse_move_process(ee, ee->mouse.x, ee->mouse.y,
						ecore_x_current_time_get());
	     ee->expecting_resize.w = 0;
	     ee->expecting_resize.h = 0;
	  }
	if (ee->func.fn_resize) ee->func.fn_resize(ee);	
     }
   return 1;
}

static int
_ecore_evas_x_event_window_delete_request(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Window_Delete_Request *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if (!ee) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   if (ee->func.fn_delete_request) ee->func.fn_delete_request(ee);
   return 1;
}

static int
_ecore_evas_x_event_window_show(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Window_Show *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if (!ee) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   if (ee->visible) return 0; /* dont pass it on */
   ee->visible = 1;
   if (ee->func.fn_show) ee->func.fn_show(ee);
   return 1;
}

static int
_ecore_evas_x_event_window_hide(void *data __UNUSED__, int type __UNUSED__, void *event)
{
   Ecore_Evas *ee;
   Ecore_X_Event_Window_Hide *e;
   
   e = event;
   ee = _ecore_evas_x_match(e->win);
   if (!ee) return 1; /* pass on event */
   if (e->win != ee->engine.x.win) return 1;
   if (!ee->visible) return 0; /* dont pass it on */
   ee->visible = 0;
   if (ee->func.fn_hide) ee->func.fn_hide(ee);
   return 1;
}

/* FIXME, should be in idler */
static void
_ecore_evas_x_size_pos_hints_update(Ecore_Evas *ee)
{
   ecore_x_icccm_size_pos_hints_set(ee->engine.x.win,
				    ee->prop.request_pos /*request_pos */,
				    ECORE_X_GRAVITY_NW /* gravity */,
				    ee->prop.min.w /* min_w */,
				    ee->prop.min.h /* min_h */,
				    ee->prop.max.w /* max_w */,
				    ee->prop.max.h /* max_h */,
				    ee->prop.base.w /* base_w */,
				    ee->prop.base.h /* base_h */,
				    ee->prop.step.w /* step_x */,
				    ee->prop.step.h /* step_y */,
				    0 /* min_aspect */,
				    0 /* max_aspect */);
}

/* FIXME, should be in idler */
static void
_ecore_evas_x_state_update(Ecore_Evas *ee)
{
   Ecore_X_Window_State state[10];
   int num;

   num = 0;

   /*
   if (bd->client.netwm.state.modal)
     state[num++] = ECORE_X_WINDOW_STATE_MODAL;
   */
   if (ee->engine.x.state.sticky)
     state[num++] = ECORE_X_WINDOW_STATE_STICKY;
   /*
   if (bd->client.netwm.state.maximized_v)
     state[num++] = ECORE_X_WINDOW_STATE_MAXIMIZED_VERT;
   if (bd->client.netwm.state.maximized_h)
     state[num++] = ECORE_X_WINDOW_STATE_MAXIMIZED_HORZ;
   if (bd->client.netwm.state.shaded)
     state[num++] = ECORE_X_WINDOW_STATE_SHADED;
   if (bd->client.netwm.state.skip_taskbar)
     state[num++] = ECORE_X_WINDOW_STATE_SKIP_TASKBAR;
   if (bd->client.netwm.state.skip_pager)
     state[num++] = ECORE_X_WINDOW_STATE_SKIP_PAGER;
   if (bd->client.netwm.state.hidden)
     state[num++] = ECORE_X_WINDOW_STATE_HIDDEN;
   if (bd->client.netwm.state.fullscreen)
     state[num++] = ECORE_X_WINDOW_STATE_FULLSCREEN;
   */
   if (ee->engine.x.state.above)
     state[num++] = ECORE_X_WINDOW_STATE_ABOVE;
   if (ee->engine.x.state.below)
     state[num++] = ECORE_X_WINDOW_STATE_BELOW;

   ecore_x_netwm_window_state_set(ee->engine.x.win, state, num);
}

static void
_ecore_evas_x_layer_update(Ecore_Evas *ee)
{
   if (ee->should_be_visible)
     {
	/* We need to send a netwm request to the wm */
	/* FIXME: Do we have to remove old state before adding new? */
	if (ee->prop.layer < 3)
	  {
	     if (ee->engine.x.state.above)
	       {
		  ee->engine.x.state.above = 0;
		  ecore_x_netwm_state_request_send(ee->engine.x.win,
						   ee->engine.x.win_root,
						   ECORE_X_WINDOW_STATE_ABOVE, -1, 0);
	       }
	     if (!ee->engine.x.state.below)
	       {
		  ee->engine.x.state.below = 1;
		  ecore_x_netwm_state_request_send(ee->engine.x.win,
						   ee->engine.x.win_root,
						   ECORE_X_WINDOW_STATE_BELOW, -1, 1);
	       }
	  }
	else if (ee->prop.layer > 5)
	  {
	     if (ee->engine.x.state.below)
	       {
		  ee->engine.x.state.below = 0;
		  ecore_x_netwm_state_request_send(ee->engine.x.win,
						   ee->engine.x.win_root,
						   ECORE_X_WINDOW_STATE_BELOW, -1, 0);
	       }
	     if (!ee->engine.x.state.above)
	       {
		  ee->engine.x.state.above = 1;
		  ecore_x_netwm_state_request_send(ee->engine.x.win,
						   ee->engine.x.win_root,
						   ECORE_X_WINDOW_STATE_ABOVE, -1, 1);
	       }
	  }
	else
	  {
	     if (ee->engine.x.state.below)
	       {
		  ee->engine.x.state.below = 0;
		  ecore_x_netwm_state_request_send(ee->engine.x.win,
						   ee->engine.x.win_root,
						   ECORE_X_WINDOW_STATE_BELOW, -1, 0);
	       }
	     if (ee->engine.x.state.above)
	       {
		  ee->engine.x.state.above = 0;
		  ecore_x_netwm_state_request_send(ee->engine.x.win,
						   ee->engine.x.win_root,
						   ECORE_X_WINDOW_STATE_ABOVE, -1, 0);
	       }
	  }
     }
   else
     {
	/* Just set the state */
	if (ee->prop.layer < 3)
	  {
	     if ((ee->engine.x.state.above) || (!ee->engine.x.state.below))
	       {
		  ee->engine.x.state.above = 0;
		  ee->engine.x.state.below = 1;
		  _ecore_evas_x_state_update(ee);
	       }
	  }
	else if (ee->prop.layer > 5)
	  {
	     if ((!ee->engine.x.state.above) || (ee->engine.x.state.below))
	       {
		  ee->engine.x.state.above = 1;
		  ee->engine.x.state.below = 0;
		  _ecore_evas_x_state_update(ee);
	       }
	  }
	else
	  {
	     if ((ee->engine.x.state.above) || (ee->engine.x.state.below))
	       {
		  ee->engine.x.state.above = 0;
		  ee->engine.x.state.below = 0;
		  _ecore_evas_x_state_update(ee);
	       }
	  }
     }
   /* FIXME: Set gnome layer */
}

static int
_ecore_evas_x_idle_enter(void *data __UNUSED__)
{
   Ecore_List2 *l;
   double t1 = 0.0;
   double t2 = 0.0;

   if (_ecore_evas_fps_debug)
     {
	t1 = ecore_time_get();
     }
   for (l = (Ecore_List2 *)ecore_evases; l; l = l->next)
     {
	Ecore_Evas *ee;
	
	ee = (Ecore_Evas *)l;
	_ecore_evas_x_render(ee);
     }
   ecore_x_flush();
   if (_ecore_evas_fps_debug)
     {
	t2 = ecore_time_get();
	_ecore_evas_fps_debug_rendertime_add(t2 - t1);
     }
   return 1;
}

static int
_ecore_evas_x_init(void)
{
   _ecore_evas_init_count++;
   if (_ecore_evas_init_count > 1) return _ecore_evas_init_count;
   if (getenv("ECORE_EVAS_FPS_DEBUG")) _ecore_evas_fps_debug = 1;
   ecore_evas_idle_enterer = ecore_idle_enterer_add(_ecore_evas_x_idle_enter, NULL);
   ecore_evas_event_handlers[0]  = ecore_event_handler_add(ECORE_X_EVENT_KEY_DOWN, _ecore_evas_x_event_key_down, NULL);
   ecore_evas_event_handlers[1]  = ecore_event_handler_add(ECORE_X_EVENT_KEY_UP, _ecore_evas_x_event_key_up, NULL);
   ecore_evas_event_handlers[2]  = ecore_event_handler_add(ECORE_X_EVENT_MOUSE_BUTTON_DOWN, _ecore_evas_x_event_mouse_button_down, NULL);
   ecore_evas_event_handlers[3]  = ecore_event_handler_add(ECORE_X_EVENT_MOUSE_BUTTON_UP, _ecore_evas_x_event_mouse_button_up, NULL);
   ecore_evas_event_handlers[4]  = ecore_event_handler_add(ECORE_X_EVENT_MOUSE_MOVE, _ecore_evas_x_event_mouse_move, NULL);
   ecore_evas_event_handlers[5]  = ecore_event_handler_add(ECORE_X_EVENT_MOUSE_IN, _ecore_evas_x_event_mouse_in, NULL);
   ecore_evas_event_handlers[6]  = ecore_event_handler_add(ECORE_X_EVENT_MOUSE_OUT, _ecore_evas_x_event_mouse_out, NULL);
   ecore_evas_event_handlers[7]  = ecore_event_handler_add(ECORE_X_EVENT_WINDOW_FOCUS_IN, _ecore_evas_x_event_window_focus_in, NULL);
   ecore_evas_event_handlers[8]  = ecore_event_handler_add(ECORE_X_EVENT_WINDOW_FOCUS_OUT, _ecore_evas_x_event_window_focus_out, NULL);
   ecore_evas_event_handlers[9]  = ecore_event_handler_add(ECORE_X_EVENT_WINDOW_DAMAGE, _ecore_evas_x_event_window_damage, NULL);
   ecore_evas_event_handlers[10] = ecore_event_handler_add(ECORE_X_EVENT_WINDOW_DESTROY, _ecore_evas_x_event_window_destroy, NULL);
   ecore_evas_event_handlers[11] = ecore_event_handler_add(ECORE_X_EVENT_WINDOW_CONFIGURE, _ecore_evas_x_event_window_configure, NULL);
   ecore_evas_event_handlers[12] = ecore_event_handler_add(ECORE_X_EVENT_WINDOW_DELETE_REQUEST, _ecore_evas_x_event_window_delete_request, NULL);
   ecore_evas_event_handlers[13] = ecore_event_handler_add(ECORE_X_EVENT_WINDOW_SHOW, _ecore_evas_x_event_window_show, NULL);
   ecore_evas_event_handlers[14] = ecore_event_handler_add(ECORE_X_EVENT_WINDOW_HIDE, _ecore_evas_x_event_window_hide, NULL);
   ecore_evas_event_handlers[15] = ecore_event_handler_add(ECORE_X_EVENT_MOUSE_WHEEL, _ecore_evas_x_event_mouse_wheel, NULL);
   if (_ecore_evas_fps_debug) _ecore_evas_fps_debug_init();
   return _ecore_evas_init_count;
}

static void
_ecore_evas_x_free(Ecore_Evas *ee)
{
   ecore_x_window_del(ee->engine.x.win);
   if (ee->engine.x.pmap) ecore_x_pixmap_del(ee->engine.x.pmap);
   if (ee->engine.x.mask) ecore_x_pixmap_del(ee->engine.x.mask);
   if (ee->engine.x.gc) ecore_x_gc_del(ee->engine.x.gc);
   if (ee->engine.x.damages) XDestroyRegion(ee->engine.x.damages);
   ee->engine.x.pmap = 0;
   ee->engine.x.mask = 0;
   ee->engine.x.gc = 0;
   ee->engine.x.damages = 0;
   ecore_evases_hash = evas_hash_del(ecore_evases_hash, _ecore_evas_x_winid_str_get(ee->engine.x.win), ee);
   while (ee->engine.x.win_extra)
     {
	Ecore_X_Window *winp;
	
	winp = ee->engine.x.win_extra->data;
	ee->engine.x.win_extra = evas_list_remove_list(ee->engine.x.win_extra, ee->engine.x.win_extra);
	ecore_evases_hash = evas_hash_del(ecore_evases_hash, _ecore_evas_x_winid_str_get(*winp), ee);
	free(winp);
     }
   ecore_evases = _ecore_list2_remove(ecore_evases, ee);
   _ecore_evas_x_shutdown();
   ecore_x_shutdown();
}

static void
_ecore_evas_x_callback_delete_request_set(Ecore_Evas *ee, void (*func) (Ecore_Evas *ee))
{
   if (func)
     ecore_x_icccm_protocol_set(ee->engine.x.win, ECORE_X_WM_PROTOCOL_DELETE_REQUEST, 1);
   else
     ecore_x_icccm_protocol_set(ee->engine.x.win, ECORE_X_WM_PROTOCOL_DELETE_REQUEST, 0);
   ee->func.fn_delete_request = func;
}

static void
_ecore_evas_x_move(Ecore_Evas *ee, int x, int y)
{
   if (ee->engine.x.direct_resize)
     {
	if (!ee->engine.x.managed)
	  {
	     if ((x != ee->x) || (y != ee->y))
	       {
		  ecore_x_window_move(ee->engine.x.win, x, y);
		  if (!ee->should_be_visible)
		    {
		       /* We need to request pos */
		       ee->prop.request_pos = 1;
		       _ecore_evas_x_size_pos_hints_update(ee);
		    }
		  if (ee->func.fn_move) ee->func.fn_move(ee);
	       }
	  }
     }
   else
     {
	ecore_x_window_move(ee->engine.x.win, x, y);
	if (!ee->should_be_visible)
	  {
	     /* We need to request pos */
	     ee->prop.request_pos = 1;
	     _ecore_evas_x_size_pos_hints_update(ee);
	  }
     }
}

static void
_ecore_evas_x_managed_move(Ecore_Evas *ee, int x, int y)
{
   if (ee->engine.x.direct_resize)
     {
	ee->engine.x.managed = 1;
	if ((x != ee->x) || (y != ee->y))
	  {
	     ee->x = x;
	     ee->y = y;
	     if (ee->func.fn_move) ee->func.fn_move(ee);
	  }
     }
}

static void
_ecore_evas_x_resize(Ecore_Evas *ee, int w, int h)
{
   if (ee->engine.x.direct_resize)
     {
	if ((ee->w != w) || (ee->h != h))
	  {
	     ecore_x_window_resize(ee->engine.x.win, w, h);
	     ee->w = w;
	     ee->h = h;
	     if ((ee->rotation == 90) || (ee->rotation == 270))
	       {
		  evas_output_size_set(ee->evas, ee->h, ee->w);
		  evas_output_viewport_set(ee->evas, 0, 0, ee->h, ee->w);
	       }
	     else
	       {
		  evas_output_size_set(ee->evas, ee->w, ee->h);
		  evas_output_viewport_set(ee->evas, 0, 0, ee->w, ee->h);
	       }
	     if (ee->prop.avoid_damage)
	       {
		  ecore_evas_avoid_damage_set(ee, 0);
		  ecore_evas_avoid_damage_set(ee, 1);
	       }
	     if (ee->shaped)
	       {
		  _ecore_evas_x_resize_shape(ee);
	       }
	     if (ee->func.fn_resize) ee->func.fn_resize(ee);
	  }
     }
   else
     ecore_x_window_resize(ee->engine.x.win, w, h);
}

static void
_ecore_evas_x_move_resize(Ecore_Evas *ee, int x, int y, int w, int h)
{
   if (ee->engine.x.direct_resize)
     {
	if ((ee->w != w) || (ee->h != h) || (x != ee->x) || (y != ee->y))
	  {
	     int change_size = 0, change_pos = 0;
	     
	     if ((ee->w != w) || (ee->h != h)) change_size = 1;
	     if (!ee->engine.x.managed)
	       {
		  if ((x != ee->x) || (y != ee->y)) change_pos = 1;
	       }
	     ecore_x_window_move_resize(ee->engine.x.win, x, y, w, h);
	     if (!ee->engine.x.managed)
	       {
		  ee->x = x;
		  ee->y = y;
	       }
	     ee->w = w;
	     ee->h = h;
	     if ((ee->rotation == 90) || (ee->rotation == 270))
	       {
		  evas_output_size_set(ee->evas, ee->h, ee->w);
		  evas_output_viewport_set(ee->evas, 0, 0, ee->h, ee->w);
	       }
	     else
	       {
		  evas_output_size_set(ee->evas, ee->w, ee->h);
		  evas_output_viewport_set(ee->evas, 0, 0, ee->w, ee->h);
	       }
	     if (ee->prop.avoid_damage)
	       {
		  ecore_evas_avoid_damage_set(ee, 0);
		  ecore_evas_avoid_damage_set(ee, 1);
	       }
	     if (ee->shaped)
	       {
		  _ecore_evas_x_resize_shape(ee);
	       }
	     if (change_pos)
	       {
		  if (ee->func.fn_move) ee->func.fn_move(ee);
	       }
	     if (change_size)
	       {
		  if (ee->func.fn_resize) ee->func.fn_resize(ee);
	       }
	  }
     }
   else
     ecore_x_window_move_resize(ee->engine.x.win, x, y, w, h);
}

static void
_ecore_evas_x_rotation_set(Ecore_Evas *ee, int rotation)
{
   int rot_dif;
   
   if (ee->rotation == rotation) return;   
   if (!strcmp(ee->driver, "gl_x11")) return;
   rot_dif = ee->rotation - rotation;
   if (rot_dif < 0) rot_dif = -rot_dif;
   if (!strcmp(ee->driver, "software_x11"))
     {   
#ifdef BUILD_ECORE_X
	Evas_Engine_Info_Software_X11 *einfo;
	
	einfo = (Evas_Engine_Info_Software_X11 *)evas_engine_info_get(ee->evas);
	if (!einfo) return;
	if (rot_dif != 180)
	  {
	     int minw, minh, maxw, maxh, basew, baseh, stepw, steph;
	     
	     einfo->info.rotation = rotation;
	     evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
	     if (!ee->prop.fullscreen)
	       {
		  ecore_x_window_resize(ee->engine.x.win, ee->h, ee->w);
		  ee->expecting_resize.w = ee->h;
		  ee->expecting_resize.h = ee->w;
	       }
	     else
	       {
		  int w, h;
		  
		  ecore_x_window_size_get(ee->engine.x.win, &w, &h);
		  ecore_x_window_resize(ee->engine.x.win, h, w);
		  if ((rotation == 0) || (rotation == 180))
		    {
		       evas_output_size_set(ee->evas, ee->w, ee->h);
		       evas_output_viewport_set(ee->evas, 0, 0, ee->w, ee->h);
		    }
		  else
		    {
		       evas_output_size_set(ee->evas, ee->h, ee->w);
		       evas_output_viewport_set(ee->evas, 0, 0, ee->h, ee->w);
		    }
		  if (ee->func.fn_resize) ee->func.fn_resize(ee);	
	       }
	     ecore_evas_size_min_get(ee, &minw, &minh);
	     ecore_evas_size_max_get(ee, &maxw, &maxh);
	     ecore_evas_size_base_get(ee, &basew, &baseh);
	     ecore_evas_size_step_get(ee, &stepw, &steph);
	     ee->rotation = rotation;
	     ecore_evas_size_min_set(ee, minh, minw);
	     ecore_evas_size_max_set(ee, maxh, maxw);
	     ecore_evas_size_base_set(ee, baseh, basew);
	     ecore_evas_size_step_set(ee, steph, stepw);
	     _ecore_evas_x_mouse_move_process(ee, ee->mouse.x, ee->mouse.y,
					      ecore_x_current_time_get());
	  }
	else
	  {
	     einfo->info.rotation = rotation;
	     evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
	     ee->rotation = rotation;
	     _ecore_evas_x_mouse_move_process(ee, ee->mouse.x, ee->mouse.y,
					      ecore_x_current_time_get());
	     if (ee->func.fn_resize) ee->func.fn_resize(ee);	
	  }
	if ((ee->rotation == 90) || (ee->rotation == 270))
	  evas_damage_rectangle_add(ee->evas, 0, 0, ee->h, ee->w);
	else
	  evas_damage_rectangle_add(ee->evas, 0, 0, ee->w, ee->h);
#endif	
     }
}

static void
_ecore_evas_x_shaped_set(Ecore_Evas *ee, int shaped)
{
   if (((ee->shaped) && (shaped)) || ((!ee->shaped) && (!shaped)))
     return;
   if (!strcmp(ee->driver, "software_x11"))
     {
#ifdef BUILD_ECORE_X
	Evas_Engine_Info_Software_X11 *einfo;
	
	ee->shaped = shaped;
	einfo = (Evas_Engine_Info_Software_X11 *)evas_engine_info_get(ee->evas);
	if (einfo)
	  {
	     if (ee->shaped)
	       {
		  GC gc;
		  XGCValues gcv;
		  
		  ee->engine.x.mask = ecore_x_pixmap_new(ee->engine.x.win, ee->w, ee->h, 1);
		  gcv.foreground = 0;
		  gc = XCreateGC(ecore_x_display_get(), ee->engine.x.mask, 
				 GCForeground,
				 &gcv);
		  XFillRectangle(ecore_x_display_get(), ee->engine.x.mask, gc,
				 0, 0, ee->w, ee->h);
		  XFreeGC(ecore_x_display_get(), gc);
		  einfo->info.mask = ee->engine.x.mask;
		  evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
		  evas_damage_rectangle_add(ee->evas, 0, 0, ee->w, ee->h);
	       }
	     else
	       {
		  if (ee->engine.x.mask) ecore_x_pixmap_del(ee->engine.x.mask);
		  ee->engine.x.mask = 0;
		  einfo->info.mask = 0;
		  evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
		  ecore_x_window_shape_mask_set(ee->engine.x.win, 0);
	       }
	  }
#endif	
     }
   else if (!strcmp(ee->driver, "xrender_x11"))
     {
#ifdef BUILD_ECORE_EVAS_XRENDER
	Evas_Engine_Info_XRender_X11 *einfo;
	
	ee->shaped = shaped;
	einfo = (Evas_Engine_Info_XRender_X11 *)evas_engine_info_get(ee->evas);
	if (einfo)
	  {
	     if (ee->shaped)
	       {
		  GC gc;
		  XGCValues gcv;
		  
		  ee->engine.x.mask = ecore_x_pixmap_new(ee->engine.x.win, ee->w, ee->h, 1);
		  gcv.foreground = 0;
		  gc = XCreateGC(ecore_x_display_get(), ee->engine.x.mask, 
				 GCForeground,
				 &gcv);
		  XFillRectangle(ecore_x_display_get(), ee->engine.x.mask, gc,
				 0, 0, ee->w, ee->h);
		  XFreeGC(ecore_x_display_get(), gc);
		  einfo->info.mask = ee->engine.x.mask;
		  evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
		  evas_damage_rectangle_add(ee->evas, 0, 0, ee->w, ee->h);
	       }
	     else
	       {
		  if (ee->engine.x.mask) ecore_x_pixmap_del(ee->engine.x.mask);
		  ee->engine.x.mask = 0;
		  einfo->info.mask = 0;
		  evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
		  ecore_x_window_shape_mask_set(ee->engine.x.win, 0);
	       }
	  }
#endif	
     }
}

static void
_ecore_evas_x_alpha_set(Ecore_Evas *ee, int alpha)
{
   if (((ee->alpha) && (alpha)) || ((!ee->alpha) && (!alpha)))
     return;
   if (!strcmp(ee->driver, "xrender_x11"))
     {
#ifdef BUILD_ECORE_EVAS_XRENDER
	Evas_Engine_Info_XRender_X11 *einfo;
	
	ee->shaped = 0;
	ee->alpha = alpha;
	einfo = (Evas_Engine_Info_XRender_X11 *)evas_engine_info_get(ee->evas);
	if (einfo)
	  {
	     XWindowAttributes att;
	     
	     ecore_x_window_del(ee->engine.x.win);
	     ecore_evases_hash = evas_hash_del(ecore_evases_hash, _ecore_evas_x_winid_str_get(ee->engine.x.win), ee);
	     if (ee->alpha)
	       {
		  if (ee->prop.override)
		    ee->engine.x.win = ecore_x_window_override_argb_new(ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h);
		  else
		    ee->engine.x.win = ecore_x_window_argb_new(ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h);
		  einfo->info.destination_alpha = 1;
	       }
	     else
	       {
		  if (ee->prop.override)
		    ee->engine.x.win = ecore_x_window_override_new(ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h);
		  else
		    ee->engine.x.win = ecore_x_window_new(ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h);
		  einfo->info.destination_alpha = 0;
	       }
	     XGetWindowAttributes(ecore_x_display_get(), ee->engine.x.win, &att);
	     einfo->info.visual = att.visual;
	     if (ee->engine.x.mask) ecore_x_pixmap_del(ee->engine.x.mask);
	     ee->engine.x.mask = 0;
	     einfo->info.mask = 0;
	     einfo->info.drawable = ee->engine.x.win;
	     evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
	     evas_damage_rectangle_add(ee->evas, 0, 0, ee->w, ee->h);
	     ecore_x_window_shape_mask_set(ee->engine.x.win, 0);
	     ecore_evases_hash = evas_hash_add(ecore_evases_hash, _ecore_evas_x_winid_str_get(ee->engine.x.win), ee);
	     if (ee->visible) ecore_x_window_show(ee->engine.x.win);
	     if (ee->prop.focused) ecore_x_window_focus(ee->engine.x.win);
	  }
#endif	
     }
}

static void
_ecore_evas_x_show(Ecore_Evas *ee)
{
   ee->should_be_visible = 1;
   if (ee->prop.avoid_damage)
     _ecore_evas_x_render(ee);
   ecore_x_window_show(ee->engine.x.win);
   if (ee->prop.fullscreen)
     ecore_x_window_focus(ee->engine.x.win);
}

static void
_ecore_evas_x_hide(Ecore_Evas *ee)
{
   ecore_x_window_hide(ee->engine.x.win);
   ee->should_be_visible = 0;
}

static void
_ecore_evas_x_raise(Ecore_Evas *ee)
{
   if (!ee->prop.fullscreen)
     ecore_x_window_raise(ee->engine.x.win);
   else
     ecore_x_window_raise(ee->engine.x.win);
}

static void
_ecore_evas_x_lower(Ecore_Evas *ee)
{
   if (!ee->prop.fullscreen)
     ecore_x_window_lower(ee->engine.x.win);
   else
     ecore_x_window_lower(ee->engine.x.win);
}

static void
_ecore_evas_x_title_set(Ecore_Evas *ee, const char *t)
{
   if (ee->prop.title) free(ee->prop.title);
   ee->prop.title = NULL;
   if (t) ee->prop.title = strdup(t);
   ecore_x_icccm_title_set(ee->engine.x.win, ee->prop.title);
   ecore_x_netwm_name_set(ee->engine.x.win, ee->prop.title);
}

static void
_ecore_evas_x_name_class_set(Ecore_Evas *ee, const char *n, const char *c)
{
   if (ee->prop.name) free(ee->prop.name);
   if (ee->prop.clas) free(ee->prop.clas);
   ee->prop.name = NULL;
   ee->prop.clas = NULL;
   ee->prop.name = strdup(n);
   ee->prop.clas = strdup(c);
   ecore_x_icccm_name_class_set(ee->engine.x.win, ee->prop.name, ee->prop.clas);
}

static void
_ecore_evas_x_size_min_set(Ecore_Evas *ee, int w, int h)
{
   if (w < 0) w = 0;
   if (h < 0) h = 0;
   if ((ee->prop.min.w == w) && (ee->prop.min.h == h)) return;
   ee->prop.min.w = w;
   ee->prop.min.h = h;
   _ecore_evas_x_size_pos_hints_update(ee);
}

static void
_ecore_evas_x_size_max_set(Ecore_Evas *ee, int w, int h)
{
   if (w < 0) w = 0;
   if (h < 0) h = 0;
   if ((ee->prop.max.w == w) && (ee->prop.max.h == h)) return;
   ee->prop.max.w = w;
   ee->prop.max.h = h;
   _ecore_evas_x_size_pos_hints_update(ee);
}

static void
_ecore_evas_x_size_base_set(Ecore_Evas *ee, int w, int h)
{
   if (w < 0) w = 0;
   if (h < 0) h = 0;
   if ((ee->prop.base.w == w) && (ee->prop.base.h == h)) return;
   ee->prop.base.w = w;
   ee->prop.base.h = h;
   _ecore_evas_x_size_pos_hints_update(ee);
}

static void
_ecore_evas_x_size_step_set(Ecore_Evas *ee, int w, int h)
{
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   if ((ee->prop.step.w == w) && (ee->prop.step.h == h)) return;
   ee->prop.step.w = w;
   ee->prop.step.h = h;
   _ecore_evas_x_size_pos_hints_update(ee);
}

static void
_ecore_evas_x_cursor_set(Ecore_Evas *ee, const char *file, int layer, int hot_x, int hot_y)
{
   int x, y;
   
   if (!file)
     {
	if (ee->prop.cursor.object) evas_object_del(ee->prop.cursor.object);
	if (ee->prop.cursor.file) free(ee->prop.cursor.file);
	ee->prop.cursor.object = NULL;
	ee->prop.cursor.file = NULL;
	ee->prop.cursor.layer = 0;
	ee->prop.cursor.hot.x = 0;
	ee->prop.cursor.hot.y = 0;
	ecore_x_window_cursor_show(ee->engine.x.win, 1);
	return;
     }
   ecore_x_window_cursor_show(ee->engine.x.win, 0);
   if (!ee->prop.cursor.object) ee->prop.cursor.object = evas_object_image_add(ee->evas);
   if (ee->prop.cursor.file) free(ee->prop.cursor.file);
   ee->prop.cursor.file = strdup(file);
   ee->prop.cursor.layer = layer;
   ee->prop.cursor.hot.x = hot_x;
   ee->prop.cursor.hot.y = hot_y;   
   evas_pointer_output_xy_get(ee->evas, &x, &y);
   evas_object_layer_set(ee->prop.cursor.object, ee->prop.cursor.layer);
   evas_object_color_set(ee->prop.cursor.object, 255, 255, 255, 255);
   evas_object_move(ee->prop.cursor.object, 
		    x - ee->prop.cursor.hot.x,
		    y - ee->prop.cursor.hot.y);
   evas_object_image_file_set(ee->prop.cursor.object, ee->prop.cursor.file, NULL);
   evas_object_image_size_get(ee->prop.cursor.object, &x, &y);
   evas_object_resize(ee->prop.cursor.object, x, y);
   evas_object_image_fill_set(ee->prop.cursor.object, 0, 0, x, y);
   evas_object_pass_events_set(ee->prop.cursor.object, 1);
   if (evas_pointer_inside_get(ee->evas))
     evas_object_show(ee->prop.cursor.object);
}

/*
 * @param ee
 * @param layer If < 3, @a ee will be put below all other windows.
 *              If > 5, @a ee will be "always-on-top"
 *              If = 4, @a ee will be put in the default layer.
 *              Acceptable values range from 1 to 255 (0 reserved for
 *              desktop windows)
 */
static void
_ecore_evas_x_layer_set(Ecore_Evas *ee, int layer)
{
   if (ee->prop.layer == layer) return;

   /* FIXME: Should this logic be here? */
   if (layer < 1)
     layer = 1;
   else if (layer > 255)
     layer = 255;

   ee->prop.layer = layer;
   _ecore_evas_x_layer_update(ee);
}

static void
_ecore_evas_x_focus_set(Ecore_Evas *ee, int on __UNUSED__)
{
   ecore_x_window_focus(ee->engine.x.win);
}

static void
_ecore_evas_x_iconified_set(Ecore_Evas *ee, int on)
{
//   if (((ee->prop.iconified) && (on)) ||
//       ((!ee->prop.iconified) && (!on))) return;
   ee->prop.iconified = on;
   if (on)
     {
	ecore_x_icccm_hints_set(ee->engine.x.win,
				1 /* accepts_focus */,
				ECORE_X_WINDOW_STATE_HINT_ICONIC /* initial_state */,
				0 /* icon_pixmap */,
				0 /* icon_mask */,
				0 /* icon_window */,
				0 /* window_group */,
				0 /* is_urgent */);
	ecore_x_icccm_iconic_request_send(ee->engine.x.win, ee->engine.x.win_root);
     }
   else
     {
     	ecore_x_icccm_hints_set(ee->engine.x.win,
				1 /* accepts_focus */,
				ECORE_X_WINDOW_STATE_HINT_NORMAL /* initial_state */,
				0 /* icon_pixmap */,
				0 /* icon_mask */,
				0 /* icon_window */,
				0 /* window_group */,
				0 /* is_urgent */);
	ecore_evas_show(ee);
     }
}

static void
_ecore_evas_x_borderless_set(Ecore_Evas *ee, int on)
{
   if (((ee->prop.borderless) && (on)) ||
       ((!ee->prop.borderless) && (!on))) return;
   ee->prop.borderless = on;
   ecore_x_mwm_borderless_set(ee->engine.x.win, ee->prop.borderless);
}

/* FIXME: This function changes the initial state of the ee
 * whilest the iconic function changes the current state! */
static void
_ecore_evas_x_withdrawn_set(Ecore_Evas *ee, int withdrawn)
{
   Ecore_X_Window_State_Hint hint;

   if ((ee->prop.withdrawn && withdrawn) ||
      (!ee->prop.withdrawn && !withdrawn)) return;

   ee->prop.withdrawn = withdrawn;
   if (withdrawn)
     hint = ECORE_X_WINDOW_STATE_HINT_WITHDRAWN;
   else
     hint = ECORE_X_WINDOW_STATE_HINT_NORMAL;

   ecore_x_icccm_hints_set(ee->engine.x.win,
			   1 /* accepts_focus */,
			   hint /* initial_state */,
			   0 /* icon_pixmap */,
			   0 /* icon_mask */,
			   0 /* icon_window */,
			   0 /* window_group */,
			   0 /* is_urgent */);
}

static void
_ecore_evas_x_sticky_set(Ecore_Evas *ee, int sticky)
{
   if ((ee->prop.sticky && sticky) ||
      (!ee->prop.sticky && !sticky)) return;

   ee->prop.sticky = sticky;
   ee->engine.x.state.sticky = sticky;
   if (ee->should_be_visible)
     ecore_x_netwm_state_request_send(ee->engine.x.win, ee->engine.x.win_root,
				      ECORE_X_WINDOW_STATE_STICKY, -1, sticky);
   else
     _ecore_evas_x_state_update(ee);
}

static void
_ecore_evas_x_ignore_events_set(Ecore_Evas *ee, int ignore)
{
   if ((ee->ignore_events && ignore) ||
       (!ee->ignore_events && !ignore)) return;

   if (ignore)
     {
	ee->ignore_events = 1;
	if (ee->engine.x.win)
	  ecore_x_window_ignore_set(ee->engine.x.win, 1);
     }
   else
     {
	ee->ignore_events = 0;
	if (ee->engine.x.win)
	  ecore_x_window_ignore_set(ee->engine.x.win, 0);
     }
}

static void
_ecore_evas_x_reinit_win(Ecore_Evas *ee)
{
   if (!strcmp(ee->driver, "software_x11"))
     {
#ifdef BUILD_ECORE_X
	Evas_Engine_Info_Software_X11 *einfo;
	
	einfo = (Evas_Engine_Info_Software_X11 *)evas_engine_info_get(ee->evas);
	if (einfo)
	  {
	     einfo->info.drawable = ee->engine.x.win;
	     evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
	  }
#endif	
     }
   else if (!strcmp(ee->driver, "xrender_x11"))
     {
#ifdef BUILD_ECORE_EVAS_XRENDER
	Evas_Engine_Info_XRender_X11 *einfo;
	
	einfo = (Evas_Engine_Info_XRender_X11 *)evas_engine_info_get(ee->evas);
	if (einfo)
	  {
	     einfo->info.drawable = ee->engine.x.win;
	     evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
	  }
#endif	
     }
   else if (!strcmp(ee->driver, "gl_x11"))
     {
#ifdef BUILD_ECORE_EVAS_GL
	Evas_Engine_Info_GL_X11 *einfo;
	
	einfo = (Evas_Engine_Info_GL_X11 *)evas_engine_info_get(ee->evas);
	if (einfo)
	  {
	     einfo->info.drawable = ee->engine.x.win;
	     evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
	  }
#endif	
     }
}

static void
_ecore_evas_x_override_set(Ecore_Evas *ee, int on)
{
   if (((ee->prop.override) && (on)) ||
       ((!ee->prop.override) && (!on))) return;
   ecore_x_window_hide(ee->engine.x.win);
   ecore_x_window_del(ee->engine.x.win);
   ecore_evases_hash = evas_hash_del(ecore_evases_hash, _ecore_evas_x_winid_str_get(ee->engine.x.win), ee);
   if (on)
     {
	if (ee->alpha)
	  ee->engine.x.win = ecore_x_window_override_argb_new(ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h);
	else
	  ee->engine.x.win = ecore_x_window_override_new(ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h);
     }
   else
     {
	Ecore_X_Window_State_Hint hint;
	
	if (ee->prop.withdrawn)
	  hint = ECORE_X_WINDOW_STATE_HINT_WITHDRAWN;
	else
	  hint = ECORE_X_WINDOW_STATE_HINT_NORMAL;
	
#ifdef BUILD_ECORE_EVAS_GL 
	if (!strcmp(ee->driver, "gl_x11"))
	  ee->engine.x.win = _ecore_evas_x_gl_window_new(ee, ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h, 0);
	else
#endif	  
	if (ee->alpha)
	  ee->engine.x.win = ecore_x_window_argb_new(ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h);
	else
	  ee->engine.x.win = ecore_x_window_new(ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h);
	ecore_x_icccm_title_set(ee->engine.x.win, ee->prop.title);
	ecore_x_netwm_name_set(ee->engine.x.win, ee->prop.title);
	ecore_x_icccm_name_class_set(ee->engine.x.win, ee->prop.name, ee->prop.clas);
	if (ee->func.fn_delete_request)
	  ecore_x_icccm_protocol_set(ee->engine.x.win, ECORE_X_WM_PROTOCOL_DELETE_REQUEST, 1);
	_ecore_evas_x_size_pos_hints_update(ee);
	ecore_x_mwm_borderless_set(ee->engine.x.win, ee->prop.borderless);
	_ecore_evas_x_layer_update(ee);
	ecore_x_icccm_hints_set(ee->engine.x.win, 1 /* accepts_focus */,
				hint /* initial_state */, 0 /* icon_pixmap */, 0 /* icon_mask */,
				0 /* icon_window */, 0 /* window_group */, 0 /* is_urgent */);
	_ecore_evas_x_state_update(ee);
     }
   _ecore_evas_x_reinit_win(ee);
   ecore_evases_hash = evas_hash_add(ecore_evases_hash, _ecore_evas_x_winid_str_get(ee->engine.x.win), ee);
   if (ee->visible) ecore_x_window_show(ee->engine.x.win);
   if (ee->prop.focused) ecore_x_window_focus(ee->engine.x.win);
   ee->prop.override = on;
}

static void
_ecore_evas_x_fullscreen_set(Ecore_Evas *ee, int on)
{
   if (((ee->prop.fullscreen) && (on)) ||
       ((!ee->prop.fullscreen) && (!on))) return;
   ecore_x_window_hide(ee->engine.x.win);
   ecore_x_window_del(ee->engine.x.win);
   ecore_evases_hash = evas_hash_del(ecore_evases_hash, _ecore_evas_x_winid_str_get(ee->engine.x.win), ee);
   if (on)
     {
	int rw, rh;
	
	ecore_x_window_size_get(0, &rw, &rh);
#ifdef BUILD_ECORE_EVAS_GL 
	if (!strcmp(ee->driver, "gl_x11"))
	  ee->engine.x.win = _ecore_evas_x_gl_window_new(ee, ee->engine.x.win_root, 0, 0, rw, rh, 1);
	else
#endif	  
	if (ee->alpha)
	  ee->engine.x.win = ecore_x_window_override_argb_new(ee->engine.x.win_root, 0, 0, rw, rh);
	else
	  ee->engine.x.win = ecore_x_window_override_new(ee->engine.x.win_root, 0, 0, rw, rh);
	ecore_x_window_raise(ee->engine.x.win);
	ecore_x_window_show(ee->engine.x.win);
	ecore_x_window_focus(ee->engine.x.win);
	ee->engine.x.px = ee->x;
	ee->engine.x.py = ee->y;
	ee->engine.x.pw = ee->w;
	ee->engine.x.ph = ee->h;
	ee->x = 0;
	ee->y = 0;
	ee->w = rw;
	ee->h = rh;
     }
   else
     {
	ee->x = ee->engine.x.px;
	ee->y = ee->engine.x.py;
	ee->w = ee->engine.x.pw;
	ee->h = ee->engine.x.ph;
#ifdef BUILD_ECORE_EVAS_GL 
	if (!strcmp(ee->driver, "gl_x11"))
	  ee->engine.x.win = _ecore_evas_x_gl_window_new(ee, ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h, 0);
	else
#endif	  
	if (ee->alpha)
	  ee->engine.x.win = ecore_x_window_argb_new(ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h);
	else
	  ee->engine.x.win = ecore_x_window_new(ee->engine.x.win_root, ee->x, ee->y, ee->w, ee->h);
	ecore_x_window_move_resize(ee->engine.x.win, ee->engine.x.px, ee->engine.x.py, ee->engine.x.pw, ee->engine.x.ph);
     }
   ecore_evases_hash = evas_hash_add(ecore_evases_hash, _ecore_evas_x_winid_str_get(ee->engine.x.win), ee);
   if (ee->should_be_visible)
     {
	ecore_x_window_show(ee->engine.x.win);
	ecore_x_window_focus(ee->engine.x.win);
     }
   ecore_x_window_move_resize(ee->engine.x.win, 0, 0, ee->w, ee->h);
   if ((ee->rotation == 90) || (ee->rotation == 270))
     {
	evas_output_size_set(ee->evas, ee->h, ee->w);
	evas_output_viewport_set(ee->evas, 0, 0, ee->h, ee->w);
     }
   else
     {
	evas_output_size_set(ee->evas, ee->w, ee->h);
	evas_output_viewport_set(ee->evas, 0, 0, ee->w, ee->h);
     }
   if (ee->prop.avoid_damage)
     {
	ecore_evas_avoid_damage_set(ee, 0);
	ecore_evas_avoid_damage_set(ee, 1);
     }
   if (ee->shaped)
     _ecore_evas_x_resize_shape(ee);
/*   
   if ((ee->expecting_resize.w > 0) &&
       (ee->expecting_resize.h > 0))
     {
	if ((ee->expecting_resize.w == ee->w) &&
	    (ee->expecting_resize.h == ee->h))
	  _ecore_evas_x_mouse_move_process(ee, ee->mouse.x, ee->mouse.y,
					   ecore_x_current_time_get());
	ee->expecting_resize.w = 0;
	ee->expecting_resize.h = 0;
     }
 */
   _ecore_evas_x_reinit_win(ee);
   ee->prop.fullscreen = on;
   if (ee->func.fn_resize) ee->func.fn_resize(ee);	
}

static void
_ecore_evas_x_avoid_damage_set(Ecore_Evas *ee, int on)
{
   Evas_Engine_Info_Software_X11 *einfo;

   if (((ee->prop.avoid_damage) && (on)) ||
       ((!ee->prop.avoid_damage) && (!on)))
     return;
   if (!strcmp(ee->driver, "gl_x11")) return;
   ee->prop.avoid_damage = on;   
   einfo = (Evas_Engine_Info_Software_X11 *)evas_engine_info_get(ee->evas);
   if (einfo)
     {
	if (ee->prop.avoid_damage)
	  {
	     ee->engine.x.pmap = ecore_x_pixmap_new(ee->engine.x.win, ee->w, ee->h, 0);
	     ee->engine.x.gc = ecore_x_gc_new(ee->engine.x.pmap);
	     einfo->info.drawable = ee->engine.x.pmap;
	     evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
	     if ((ee->rotation == 90) || (ee->rotation == 270))
	       evas_damage_rectangle_add(ee->evas, 0, 0, ee->h, ee->w);
	     else
	       evas_damage_rectangle_add(ee->evas, 0, 0, ee->w, ee->h);
	     if (ee->engine.x.direct_resize)
	       {
/* Turn this off for now 		  
		  ee->engine.x.using_bg_pixmap = 1;
		  ecore_x_window_pixmap_set(ee->engine.x.win, ee->engine.x.pmap);
 */
	       }
	  }
	else
	  {
	     if (ee->engine.x.pmap) ecore_x_pixmap_del(ee->engine.x.pmap);
	     if (ee->engine.x.gc) ecore_x_gc_del(ee->engine.x.gc);
	     if (ee->engine.x.using_bg_pixmap)
	       {
		  ecore_x_window_pixmap_set(ee->engine.x.win, 0);
		  ee->engine.x.using_bg_pixmap = 0;
	       }
	     ee->engine.x.pmap = 0;
	     ee->engine.x.gc = 0;
	     einfo->info.drawable = ee->engine.x.win;
	     evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
	  }
     }
}

int
_ecore_evas_x_shutdown(void)
{
   _ecore_evas_init_count--;
   if (_ecore_evas_init_count == 0)
     {
	int i;
   
	while (ecore_evases) _ecore_evas_free(ecore_evases);
	for (i = 0; i < 16; i++)
	  ecore_event_handler_del(ecore_evas_event_handlers[i]);
	ecore_idle_enterer_del(ecore_evas_idle_enterer);
	ecore_evas_idle_enterer = NULL;
	if (_ecore_evas_fps_debug) _ecore_evas_fps_debug_shutdown();
     }
   if (_ecore_evas_init_count < 0) _ecore_evas_init_count = 0;
   return _ecore_evas_init_count;
}

static const Ecore_Evas_Engine_Func _ecore_x_engine_func =
{
   _ecore_evas_x_free,
     NULL,
     NULL,
     NULL,
     NULL,
     _ecore_evas_x_callback_delete_request_set,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     _ecore_evas_x_move,
     _ecore_evas_x_managed_move,
     _ecore_evas_x_resize,
     _ecore_evas_x_move_resize,
     _ecore_evas_x_rotation_set,
     _ecore_evas_x_shaped_set,
     _ecore_evas_x_show,
     _ecore_evas_x_hide,
     _ecore_evas_x_raise,
     _ecore_evas_x_lower,
     _ecore_evas_x_title_set,
     _ecore_evas_x_name_class_set,
     _ecore_evas_x_size_min_set,
     _ecore_evas_x_size_max_set,
     _ecore_evas_x_size_base_set,
     _ecore_evas_x_size_step_set,
     _ecore_evas_x_cursor_set,
     _ecore_evas_x_layer_set,
     _ecore_evas_x_focus_set,
     _ecore_evas_x_iconified_set,
     _ecore_evas_x_borderless_set,
     _ecore_evas_x_override_set,
     NULL,
     _ecore_evas_x_fullscreen_set,
     _ecore_evas_x_avoid_damage_set,
     _ecore_evas_x_withdrawn_set,
     _ecore_evas_x_sticky_set,
     _ecore_evas_x_ignore_events_set,
     _ecore_evas_x_alpha_set
};
#endif

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI Ecore_Evas *
ecore_evas_software_x11_new(const char *disp_name, Ecore_X_Window parent, 
			    int x, int y, int w, int h)
{
#ifdef BUILD_ECORE_X
   Evas_Engine_Info_Software_X11 *einfo;
   Ecore_Evas *ee;
   int argb = 0;
   int rmethod;
   static int redraw_debug = -1;

   rmethod = evas_render_method_lookup("software_x11");
   if (!rmethod) return NULL;
   if (!ecore_x_init(disp_name)) return NULL;
   ee = calloc(1, sizeof(Ecore_Evas));
   if (!ee) return NULL;
   
   ECORE_MAGIC_SET(ee, ECORE_MAGIC_EVAS);
   
   _ecore_evas_x_init();
   
   ee->engine.func = (Ecore_Evas_Engine_Func *)&_ecore_x_engine_func;
   
   ee->driver = "software_x11";
   if (disp_name) ee->name = strdup(disp_name);

   if (w < 1) w = 1;
   if (h < 1) h = 1;
   ee->x = x;
   ee->y = y;
   ee->w = w;
   ee->h = h;
   
   ee->prop.max.w = 32767;
   ee->prop.max.h = 32767;
   ee->prop.layer = 4;
   ee->prop.request_pos = 0;
   
   /* init evas here */
   ee->evas = evas_new();
   evas_data_attach_set(ee->evas, ee);
   evas_output_method_set(ee->evas, rmethod);
   evas_output_size_set(ee->evas, w, h);
   evas_output_viewport_set(ee->evas, 0, 0, w, h);

   ee->engine.x.win_root = parent;
   if (parent != 0)
     {
	if (ecore_x_window_argb_get(parent))
	  {
	     ee->engine.x.win = ecore_x_window_argb_new(parent, x, y, w, h);
	     argb = 1;
	  }
	else
	  ee->engine.x.win = ecore_x_window_new(parent, x, y, w, h);
     }
   else
     ee->engine.x.win = ecore_x_window_new(parent, x, y, w, h);
   if (getenv("DESKTOP_STARTUP_ID"))
     {
	ecore_x_netwm_startup_id_set(ee->engine.x.win,
				     getenv("DESKTOP_STARTUP_ID"));
	/* NB: on linux this may simply empty the env as opposed to completely
	 * unset it to being empty - unsure as solartis libc crashes looking 
	 * for the '=' char */
	putenv("DESKTOP_STARTUP_ID=");
     }
   einfo = (Evas_Engine_Info_Software_X11 *)evas_engine_info_get(ee->evas);
   if (einfo)
     {
	int screen;

	/* FIXME: this is inefficient as its a round trip */
	screen = DefaultScreen(ecore_x_display_get());
	if (ScreenCount(ecore_x_display_get()) > 1)
	  {
	     Ecore_X_Window *roots;
	     int num, i;
	     
	     num = 0;
	     roots = ecore_x_window_root_list(&num);
	     if (roots)
	       {
		  XWindowAttributes at;
		  
		  if (XGetWindowAttributes(ecore_x_display_get(),
					   parent, &at))
		    {
		       for (i = 0; i < num; i++)
			 {
			    if (at.root == roots[i])
			      {
				 screen = i;
				 break;
			      }
			 }
		    }
		  free(roots);
	       }
	  }
	
	if (redraw_debug < 0)
	  {
	     if (getenv("REDRAW_DEBUG"))
	       redraw_debug = atoi(getenv("REDRAW_DEBUG"));
	     else
	       redraw_debug = 0;
	  }
	einfo->info.display  = ecore_x_display_get();
	einfo->info.drawable = ee->engine.x.win;
	if (argb)
	  {
	     XWindowAttributes at;
	     
	     if (XGetWindowAttributes(ecore_x_display_get(), ee->engine.x.win,
				      &at))
	       {
		  einfo->info.visual   = at.visual;
		  einfo->info.colormap = at.colormap;
		  einfo->info.depth    = at.depth;
		  einfo->info.destination_alpha = 1;
	       }
	  }
	else
	  {
	     einfo->info.visual   = DefaultVisual(ecore_x_display_get(), screen);
	     einfo->info.colormap = DefaultColormap(ecore_x_display_get(), screen);
	     einfo->info.depth    = DefaultDepth(ecore_x_display_get(), screen);
	     einfo->info.destination_alpha = 0;
	  }
	einfo->info.rotation = 0;
	einfo->info.debug    = redraw_debug;
	evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
     }
   evas_key_modifier_add(ee->evas, "Shift");
   evas_key_modifier_add(ee->evas, "Control");
   evas_key_modifier_add(ee->evas, "Alt");
   evas_key_modifier_add(ee->evas, "Meta");
   evas_key_modifier_add(ee->evas, "Hyper");
   evas_key_modifier_add(ee->evas, "Super");
   evas_key_lock_add(ee->evas, "Caps_Lock");
   evas_key_lock_add(ee->evas, "Num_Lock");
   evas_key_lock_add(ee->evas, "Scroll_Lock");

   ecore_evases = _ecore_list2_prepend(ecore_evases, ee);
   ecore_evases_hash = evas_hash_add(ecore_evases_hash, _ecore_evas_x_winid_str_get(ee->engine.x.win), ee);
   return ee;
#else
   return NULL;
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI Ecore_X_Window
ecore_evas_software_x11_window_get(Ecore_Evas *ee)
{
#ifdef BUILD_ECORE_X
   return ee->engine.x.win;
#else
   return 0;
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI Ecore_X_Window
ecore_evas_software_x11_subwindow_get(Ecore_Evas *ee)
{
#ifdef BUILD_ECORE_X
   return ee->engine.x.win;
#else
   return 0;
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI void
ecore_evas_software_x11_direct_resize_set(Ecore_Evas *ee, int on)
{
#ifdef BUILD_ECORE_X
   ee->engine.x.direct_resize = on;
   if (ee->prop.avoid_damage)
     {
	if (ee->engine.x.direct_resize)
	  {
/* turn this off for now	     
	     ee->engine.x.using_bg_pixmap = 1;
	     ecore_x_window_pixmap_set(ee->engine.x.win, ee->engine.x.pmap);
 */
	  }
	else
	  {
	     ee->engine.x.using_bg_pixmap = 0;
	     ecore_x_window_pixmap_set(ee->engine.x.win, 0);
	     ecore_x_window_area_expose(ee->engine.x.win, 0, 0, ee->w, ee->h);
	  }
     }
#else
   return;
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI int
ecore_evas_software_x11_direct_resize_get(Ecore_Evas *ee)
{
#ifdef BUILD_ECORE_X
   return ee->engine.x.direct_resize;
#else
   return 0;
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI void
ecore_evas_software_x11_extra_event_window_add(Ecore_Evas *ee, Ecore_X_Window win)
{
#ifdef BUILD_ECORE_X
   Ecore_X_Window *winp;
   
   winp = malloc(sizeof(Ecore_X_Window));
   if (winp)
     {
	*winp = win;
	ee->engine.x.win_extra = evas_list_append(ee->engine.x.win_extra, winp);
	ecore_evases_hash = evas_hash_add(ecore_evases_hash, _ecore_evas_x_winid_str_get(win), ee);
     }
#else
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI Ecore_Evas *
ecore_evas_gl_x11_new(const char *disp_name, Ecore_X_Window parent, 
		      int x, int y, int w, int h)
{
#ifdef BUILD_ECORE_EVAS_GL 
   Evas_Engine_Info_GL_X11 *einfo;
   Ecore_Evas *ee;
   int rmethod;

   rmethod = evas_render_method_lookup("gl_x11");
   if (!rmethod) return NULL;
   if (!ecore_x_init(disp_name)) return NULL;
   ee = calloc(1, sizeof(Ecore_Evas));
   if (!ee) return NULL;
   
   ECORE_MAGIC_SET(ee, ECORE_MAGIC_EVAS);
   
   _ecore_evas_x_init();
   
   ee->engine.func = (Ecore_Evas_Engine_Func *)&_ecore_x_engine_func;
   
   ee->driver = "gl_x11";
   if (disp_name) ee->name = strdup(disp_name);

   if (w < 1) w = 1;
   if (h < 1) h = 1;
   ee->x = x;
   ee->y = y;
   ee->w = w;
   ee->h = h;
   
   ee->prop.max.w = 32767;
   ee->prop.max.h = 32767;
   ee->prop.layer = 4;
   ee->prop.request_pos = 0;
   
   /* init evas here */
   ee->evas = evas_new();
   evas_data_attach_set(ee->evas, ee);
   evas_output_method_set(ee->evas, rmethod);
   evas_output_size_set(ee->evas, w, h);
   evas_output_viewport_set(ee->evas, 0, 0, w, h);

   if (parent == 0) parent = DefaultRootWindow(ecore_x_display_get());
   ee->engine.x.win_root = parent;
   einfo = (Evas_Engine_Info_GL_X11 *)evas_engine_info_get(ee->evas);
   if (einfo)
     {
	ee->engine.x.win = _ecore_evas_x_gl_window_new(ee, ee->engine.x.win_root, x, y, w, h, 0);
	evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
     }
   if (getenv("DESKTOP_STARTUP_ID"))
     {
	ecore_x_netwm_startup_id_set(ee->engine.x.win,
				     getenv("DESKTOP_STARTUP_ID"));
	/* NB: on linux this may simply empty the env as opposed to completely
	 * unset it to being empty - unsure as solartis libc crashes looking 
	 * for the '=' char */
	putenv("DESKTOP_STARTUP_ID=");
     }
   evas_key_modifier_add(ee->evas, "Shift");
   evas_key_modifier_add(ee->evas, "Control");
   evas_key_modifier_add(ee->evas, "Alt");
   evas_key_modifier_add(ee->evas, "Meta");
   evas_key_modifier_add(ee->evas, "Hyper");
   evas_key_modifier_add(ee->evas, "Super");
   evas_key_lock_add(ee->evas, "Caps_Lock");
   evas_key_lock_add(ee->evas, "Num_Lock");
   evas_key_lock_add(ee->evas, "Scroll_Lock");

   ecore_evases = _ecore_list2_prepend(ecore_evases, ee);
   ecore_evases_hash = evas_hash_add(ecore_evases_hash, _ecore_evas_x_winid_str_get(ee->engine.x.win), ee);
   return ee;
#else
   return NULL;
#endif
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI Ecore_X_Window
ecore_evas_gl_x11_window_get(Ecore_Evas *ee)
{
#ifdef BUILD_ECORE_X
   return ee->engine.x.win;
#else
   return 0;
#endif
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI Ecore_X_Window
ecore_evas_gl_x11_subwindow_get(Ecore_Evas *ee)
{
#ifdef BUILD_ECORE_X
   return ee->engine.x.win;
#else
   return 0;
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI void
ecore_evas_gl_x11_direct_resize_set(Ecore_Evas *ee, int on)
{
#ifdef BUILD_ECORE_X
   ee->engine.x.direct_resize = on;
#else
   return;
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI int
ecore_evas_gl_x11_direct_resize_get(Ecore_Evas *ee)
{
#ifdef BUILD_ECORE_X
   return ee->engine.x.direct_resize;
#else
   return 0;
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI void
ecore_evas_gl_x11_extra_event_window_add(Ecore_Evas *ee, Ecore_X_Window win)
{
   ecore_evas_software_x11_extra_event_window_add(ee, win);
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI Ecore_Evas *
ecore_evas_xrender_x11_new(const char *disp_name, Ecore_X_Window parent, 
		      int x, int y, int w, int h)
{
#ifdef BUILD_ECORE_EVAS_XRENDER
   Evas_Engine_Info_XRender_X11 *einfo;
   Ecore_Evas *ee;
   int rmethod;

   rmethod = evas_render_method_lookup("xrender_x11");
   if (!rmethod) return NULL;
   if (!ecore_x_init(disp_name)) return NULL;
   ee = calloc(1, sizeof(Ecore_Evas));
   if (!ee) return NULL;
   
   ECORE_MAGIC_SET(ee, ECORE_MAGIC_EVAS);
   
   _ecore_evas_x_init();
   
   ee->engine.func = (Ecore_Evas_Engine_Func *)&_ecore_x_engine_func;
   
   ee->driver = "xrender_x11";
   if (disp_name) ee->name = strdup(disp_name);

   if (w < 1) w = 1;
   if (h < 1) h = 1;
   ee->x = x;
   ee->y = y;
   ee->w = w;
   ee->h = h;
   
   ee->prop.max.w = 32767;
   ee->prop.max.h = 32767;
   ee->prop.layer = 4;
   ee->prop.request_pos = 0;
   
   /* init evas here */
   ee->evas = evas_new();
   evas_data_attach_set(ee->evas, ee);
   evas_output_method_set(ee->evas, rmethod);
   evas_output_size_set(ee->evas, w, h);
   evas_output_viewport_set(ee->evas, 0, 0, w, h);
   
   ee->engine.x.win_root = parent;
   ee->engine.x.win = ecore_x_window_new(parent, x, y, w, h);
   if (getenv("DESKTOP_STARTUP_ID"))
     {
	ecore_x_netwm_startup_id_set(ee->engine.x.win,
				     getenv("DESKTOP_STARTUP_ID"));
	/* NB: on linux this may simply empty the env as opposed to completely
	 * unset it to being empty - unsure as solartis libc crashes looking 
	 * for the '=' char */
	putenv("DESKTOP_STARTUP_ID=");
     }
   einfo = (Evas_Engine_Info_XRender_X11 *)evas_engine_info_get(ee->evas);
   if (einfo)
     {
	int screen;

	/* FIXME: this is inefficient as its a round trip */
	screen = DefaultScreen(ecore_x_display_get());
	if (ScreenCount(ecore_x_display_get()) > 1)
	  {
	     Ecore_X_Window *roots;
	     int num, i;
	     
	     num = 0;
	     roots = ecore_x_window_root_list(&num);
	     if (roots)
	       {
		  XWindowAttributes at;
		  
		  if (XGetWindowAttributes(ecore_x_display_get(),
					   parent, &at))
		    {
		       for (i = 0; i < num; i++)
			 {
			    if (at.root == roots[i])
			      {
				 screen = i;
				 break;
			      }
			 }
		    }
		  free(roots);
	       }
	  }
	einfo->info.display  = ecore_x_display_get();
	einfo->info.visual   = DefaultVisual(ecore_x_display_get(), screen);
	einfo->info.drawable = ee->engine.x.win;
	evas_engine_info_set(ee->evas, (Evas_Engine_Info *)einfo);
     }
   evas_key_modifier_add(ee->evas, "Shift");
   evas_key_modifier_add(ee->evas, "Control");
   evas_key_modifier_add(ee->evas, "Alt");
   evas_key_modifier_add(ee->evas, "Meta");
   evas_key_modifier_add(ee->evas, "Hyper");
   evas_key_modifier_add(ee->evas, "Super");
   evas_key_lock_add(ee->evas, "Caps_Lock");
   evas_key_lock_add(ee->evas, "Num_Lock");
   evas_key_lock_add(ee->evas, "Scroll_Lock");

   ecore_evases = _ecore_list2_prepend(ecore_evases, ee);
   ecore_evases_hash = evas_hash_add(ecore_evases_hash, _ecore_evas_x_winid_str_get(ee->engine.x.win), ee);
   return ee;
#else
   return NULL;
#endif
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI Ecore_X_Window
ecore_evas_xrender_x11_window_get(Ecore_Evas *ee)
{
#ifdef BUILD_ECORE_X 
   return ee->engine.x.win;
#else
   return 0;
#endif
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI Ecore_X_Window
ecore_evas_xrender_x11_subwindow_get(Ecore_Evas *ee)
{
#ifdef BUILD_ECORE_X
   return ee->engine.x.win;
#else
   return 0;
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI void
ecore_evas_xrender_x11_direct_resize_set(Ecore_Evas *ee, int on)
{
#ifdef BUILD_ECORE_X
   ee->engine.x.direct_resize = on;
#else
   return;
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI int
ecore_evas_xrender_x11_direct_resize_get(Ecore_Evas *ee)
{
#ifdef BUILD_ECORE_X
   return ee->engine.x.direct_resize;
#else
   return 0;
#endif   
}

/**
 * To be documented.
 *
 * FIXME: To be fixed.
 */
EAPI void
ecore_evas_xrender_x11_extra_event_window_add(Ecore_Evas *ee, Ecore_X_Window win)
{
   ecore_evas_software_x11_extra_event_window_add(ee, win);
}

