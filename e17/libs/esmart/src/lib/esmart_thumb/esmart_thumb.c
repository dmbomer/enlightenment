#define X_DISPLAY_MISSING 1
#include <Imlib2.h>
#include <Epsilon.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Esmart_Thumb.h"
#include "../../config.h"

static void _e_thumb_add (Evas_Object * o);
static void _e_thumb_del (Evas_Object * o);
static void _e_thumb_show (Evas_Object * o);
static void _e_thumb_hide (Evas_Object * o);
static void _e_thumb_raise (Evas_Object * o);
static void _e_thumb_lower (Evas_Object * o);
static void _e_thumb_clip_unset (Evas_Object * o);
static void _e_thumb_layer_set (Evas_Object * o, int layer);
static void _e_thumb_move (Evas_Object * o, Evas_Coord x, Evas_Coord y);
static void _e_thumb_resize (Evas_Object * o, Evas_Coord w, Evas_Coord h);
static void _e_thumb_clip_set (Evas_Object * o, Evas_Object * clip);
static void _e_thumb_stack_above (Evas_Object * o, Evas_Object * above);
static void _e_thumb_stack_below (Evas_Object * o, Evas_Object * below);
static void _e_thumb_color_set (Evas_Object * o, int r, int g, int b, int a);

Evas_Object *
esmart_thumb_new (Evas * evas, const char *file)
{
  char buf[PATH_MAX];
  Evas_Object *result = NULL;
  if (file)
    {
      Esmart_Thumb *e = NULL;
      static Evas_Smart *s = NULL;

      if (!s)
	{
	  s = evas_smart_new ("Esmart_Thumb",
			      _e_thumb_add,
			      _e_thumb_del,
			      _e_thumb_layer_set,
			      _e_thumb_raise,
			      _e_thumb_lower,
			      _e_thumb_stack_above,
			      _e_thumb_stack_below,
			      _e_thumb_move,
			      _e_thumb_resize,
			      _e_thumb_show,
			      _e_thumb_hide,
			      _e_thumb_color_set,
			      _e_thumb_clip_set, _e_thumb_clip_unset, NULL);
	}
      result = evas_object_smart_add (evas, s);

      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (result)))
	{
	  if (!realpath (file, buf))
	    snprintf (buf, PATH_MAX, "%s", file);

	  if ((e->e = epsilon_new (buf)))
	    {
	      if (epsilon_exists (e->e) == EPSILON_FAIL)
		{
		  esmart_thumb_free (result);
		  result = NULL;
		}
	      else if ((e->info = epsilon_info_get (e->e)))
		{
		  Imlib_Image im = NULL;
		  if ((im = imlib_load_image (epsilon_thumb_file_get (e->e))))
		    {
		      imlib_context_set_image (im);
		      if (epsilon_info_exif_get (e->info))
			{
			  switch (epsilon_info_exif_props_as_int_get
				  (e->info, 0x0112))
			    {
			    case 3:
			      imlib_image_orientate (2);
			      break;
			    case 6:
			      imlib_image_orientate (1);
			      break;
			    case 8:
			      imlib_image_orientate (3);
			      break;
			    default:
			      break;
			    }
			}
		      e->tw = imlib_image_get_width ();
		      e->th = imlib_image_get_height ();
		      evas_object_image_size_set (e->image, (int) e->tw,
						  (int) e->th);
		      evas_object_image_fill_set (e->image, (Evas_Coord) 0,
						  (Evas_Coord) 0,
						  (Evas_Coord) e->tw,
						  (Evas_Coord) e->th);
		      evas_object_image_data_copy_set (e->image,
						       imlib_image_get_data
						       ());
		      imlib_free_image_and_decache ();
		    }
		  else
		    {
		      esmart_thumb_free (result);
		      result = NULL;
		    }
		}
	      else
		{
		  evas_object_image_file_set (e->image,
					      epsilon_thumb_file_get (e->e),
					      NULL);
		  if (!evas_object_image_load_error_get (e->image))
		    {
		      evas_object_image_size_get (e->image, &e->tw, &e->th);
		    }
		  else
		    {
		      esmart_thumb_free (result);
		      result = NULL;
		    }
		}

	    }
	}
    }
  return (result);
}

void
esmart_thumb_free (Evas_Object * o)
{
  if (o)
    evas_object_del (o);
}
const char *
esmart_thumb_file_get (Evas_Object * o)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	if (e->e)
	  return (e->e->src);
    }
  return (NULL);
}

void
esmart_thumb_geometry_get (Evas_Object * o, int *w, int *h)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  if (!e->info)
	    e->info = epsilon_info_get (e->e);
	  if (w)
	    *w = e->info->w;
	  if (h)
	    *h = e->info->h;
	}
    }
}
Evas_Object *
esmart_thumb_evas_object_get (Evas_Object * o, int orient)
{
  Evas_Object *result = NULL;
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  int iw = 0, ih = 0;
	  Imlib_Image im = NULL;

	  result = evas_object_image_add (evas_object_evas_get (o));
	  if (!e->info)
	    e->info = epsilon_info_get (e->e);
	  if (orient && epsilon_info_exif_get (e->info))
	    {
	      switch (epsilon_info_exif_props_as_int_get (e->info, 0x0112))
		{
		case 3:
		  if ((im = imlib_load_image (e->e->src)))
		    {
		      imlib_context_set_image (im);
		      imlib_image_orientate (2);
		    }
		  break;
		case 6:
		  if ((im = imlib_load_image (e->e->src)))
		    {
		      imlib_context_set_image (im);
		      imlib_image_orientate (1);
		    }
		  break;
		case 8:
		  if ((im = imlib_load_image (e->e->src)))
		    {
		      imlib_context_set_image (im);
		      imlib_image_orientate (3);
		    }
		  break;
		default:
		  break;
		}
	    }
	  if (!im)
	    {
	      evas_object_image_file_set (result, e->e->src, NULL);
	      switch (evas_object_image_load_error_get (result))
		{
		case EVAS_LOAD_ERROR_NONE:
		  evas_object_image_size_get (result, &iw, &ih);
		  break;
		default:
		  im = imlib_load_image (e->e->src);
		  imlib_context_set_image (im);
		  break;
		}
	    }
	  if (im)
	    {
	      iw = imlib_image_get_width ();
	      ih = imlib_image_get_height ();
	      evas_object_image_size_set (result, iw, ih);
	      evas_object_image_fill_set (result,
					  (Evas_Coord) 0, (Evas_Coord) 0,
					  (Evas_Coord) iw, (Evas_Coord) ih);
	      evas_object_image_alpha_set (result, imlib_image_has_alpha ());
	      evas_object_image_data_copy_set (result,
					       imlib_image_get_data ());
	      imlib_free_image_and_decache ();
	    }
	  e->info->w = iw;
	  e->info->h = ih;
	}
    }
  return (result);
}

const char *
esmart_thumb_format_get (Evas_Object * o)
{
  char *result = NULL;
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  if (!e->info)
	    e->info = epsilon_info_get (e->e);
	  if (!e->info)
	    return NULL;
	  return (e->info->mimetype);
	}
    }
  return (result);
}

int
esmart_thumb_freshen (Evas_Object * o)
{
  int result = EPSILON_FAIL;
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  if (epsilon_exists (e->e) == EPSILON_FAIL)
	    {
	      if (e->info)
		epsilon_info_free (e->info);
	      e->info = NULL;
	      if (e->image)
		evas_object_del (e->image);
	      e->image = NULL;

	      if (epsilon_generate (e->e) == EPSILON_OK)
		{
		  result = EPSILON_OK;
		  evas_image_cache_flush (evas_object_evas_get (o));
		  e->image = evas_object_image_add (evas_object_evas_get (o));
		  evas_object_image_file_set (e->image,
					      epsilon_thumb_file_get (e->
								      e),
					      NULL);
		  if (!evas_object_image_load_error_get (e->image))
		    {
		      evas_object_image_size_get (e->image, &e->tw, &e->th);
		      evas_object_repeat_events_set (e->image, 1);
		      evas_object_show (e->image);
		      evas_object_resize (o, e->w, e->h);
		    }
		  if (!e->info)
		    e->info = epsilon_info_get (e->e);
		}
	    }
	}
    }
  return (result);
}

/*==========================================================================
 * Smart Object Functions
 *========================================================================*/
static void
_e_thumb_add (Evas_Object * o)
{
  Esmart_Thumb *e = NULL;

  e = (Esmart_Thumb *) malloc (sizeof (Esmart_Thumb));
  memset (e, 0, sizeof (Esmart_Thumb));

  evas_object_smart_data_set (o, e);
  e->image = evas_object_image_add (evas_object_evas_get (o));
}

/**
 * _e_thumb_del - used when deleting the object from the evas
 * @o - Our Smart Evas Object
 */
static void
_e_thumb_del (Evas_Object * o)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)) == NULL)
	{
	  if (e->image)
	    evas_object_del (e->image);
	  if (e->e)
	    epsilon_free (e->e);
	  if (e->info)
	    epsilon_info_free (e->info);
	  free (e);
	}
    }
}

/**
 * _e_thumb_set - used when setting the thumbnail's layer
 * @o - Our Smart Evas Object
 */
static void
_e_thumb_layer_set (Evas_Object * o, int layer)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  if (e->image)
	    evas_object_layer_set (e->image, layer);
	}
    }
}

/**
 * _e_thumb_raise - send the thumbnail a "raise" request
 * @o - Our Smart Evas Object
 */
static void
_e_thumb_raise (Evas_Object * o)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  evas_object_raise (e->image);
	}
    }
}

/**
 * _e_thumb_raise - send the thumbnail a "lower" request
 * @o - Our Smart Evas Object
 */
static void
_e_thumb_lower (Evas_Object * o)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  evas_object_lower (e->image);
	}
    }
}

/**
 * _e_thumb_stack_above - used when placing the object above
 * another object in the same layer
 * @o - Our Smart Evas Object
 * @below - the object we want above
 */
static void
_e_thumb_stack_above (Evas_Object * o, Evas_Object * above)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  evas_object_stack_above (e->image, above);
	}
    }
}

/**
 * _e_thumb_stack_below - used when placing this object below
 * another object in the same layer
 * @o - Our Smart Evas Object
 * @below - the object we want below
 */
static void
_e_thumb_stack_below (Evas_Object * o, Evas_Object * below)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  evas_object_stack_below (e->image, below);
	}
    }
}

/**
 * _e_thumb_move - move the smart text to the specified coordinates
 * @o - Our Smart Object
 * @x - the x-coordinate
 * @y - the y-coordinate 
 */
static void
_e_thumb_move (Evas_Object * o, Evas_Coord x, Evas_Coord y)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  e->x = x;
	  e->y = y;
	  _e_thumb_resize (o, e->w, e->h);
	}
    }
}

/**
 * _e_thumb_resize - resize the smart text to the specified values
 * @o - Our Smart Evas Object, an Evas_Text_Smart
 * @w - the new width
 * @h - the new height
 */
static void
_e_thumb_resize (Evas_Object * o, Evas_Coord w, Evas_Coord h)
{
  if (o && (w > 1) && (h > 1))
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  Evas_Coord ww = w, hh = h;
	  e->w = w;
	  e->h = h;
	  if (e->tw > e->th)
	    hh *= (double) e->th / (double) e->tw;
	  else
	    ww *= (double) e->tw / (double) e->th;

	  evas_object_resize (e->image, ww, hh);
	  evas_object_image_fill_set (e->image, 0.0, 0.0, ww, hh);
	  evas_object_move (e->image, e->x + ((w - ww) / 2),
			    e->y + ((h - hh) / 2));
	}
    }
}

/**
 * _e_thumb_show - used sending the smart text a "show" request
 * @o - Our Smart Evas Object, an Evas_Text_Smart
 */
static void
_e_thumb_show (Evas_Object * o)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  evas_object_show (e->image);
	}
    }
}

/**
 * _e_thumb_hide - used sending the smart text a "hide" request
 * @o - Our Smart Evas Object, an Evas_Text_Smart
 */
static void
_e_thumb_hide (Evas_Object * o)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  evas_object_hide (e->image);
	}
    }
}

/**
 * _e_thumb_color_set - set the smart text color
 * @o - Our Smart Evas Object, an Evas_Text_Smart
 * @r - the new r value
 * @g - the new g value
 * @b - the new b value
 * @a - the new a value
 */
static void
_e_thumb_color_set (Evas_Object * o, int r, int g, int b, int a)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  evas_object_color_set (e->image, r, g, b, a);
	}
    }
}

/**
 * _e_thumb_clip_set - clip the smart text to the object clip
 * @o - Our Smart Evas Object, an Evas_Text_Smart
 * @clip - The Evas_Object we want to clip the smart text to
 */
static void
_e_thumb_clip_set (Evas_Object * o, Evas_Object * clip)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  evas_object_clip_set (e->image, clip);
	}
    }
}

/**
 * _e_thumb_clip_unset - remove the smart text from the clipped area
 * @o - Our Smart Evas Object, an Evas_Text_Smart
 */
static void
_e_thumb_clip_unset (Evas_Object * o)
{
  if (o)
    {
      Esmart_Thumb *e = NULL;
      if ((e = (Esmart_Thumb *) evas_object_smart_data_get (o)))
	{
	  evas_object_clip_unset (e->image);
	}
    }
}

/*****************************************************************************/
