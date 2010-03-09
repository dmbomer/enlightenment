#include <Elementary.h>
#include "elm_priv.h"
#include <ctype.h>

/**
 * @defgroup Spinner
 *
 * A spinner is a widget which allows the user to increase or decrease
 * numeric values. By default the spinner will not wrap and has a label
 * of "%.0f" (just showing the integer value of the double).
 *
 * A spinner has a label that is formatted with floating
 * point values and thus accepts a printf-style format string, like
 * “%1.2f units”.
 *
 * Signals that you can add callbacks for are:
 *
 * changed - Whenever the spinner value is changed by the user.
 *
 * delay,changed - A short time after the value is changed by the user.
 * This will be called only when the user stops dragging for a very short
 * period or when they release their finger/mouse, so it avoids possibly
 * expensive reactions to the value change.
 */
typedef struct _Widget_Data Widget_Data;

struct _Widget_Data
{
   Evas_Object *spinner, *ent;
   const char *label;
   double val, val_min, val_max, orig_val, step;
   double drag_start_pos, spin_speed, interval;
   Ecore_Timer *delay, *spin;
   Eina_Bool wrap : 1;
   Eina_Bool entry_visible : 1;
   Eina_Bool dragging : 1;
};

static const char *widtype = NULL;
static void _del_hook(Evas_Object *obj);
static void _disable_hook(Evas_Object *obj);
static void _write_label(Evas_Object *obj);
static void _sizing_eval(Evas_Object *obj);
static void _changed_size_hints(void *data, Evas *e, Evas_Object *obj, void *event_info);
static void _parent_del(void *data, Evas *e, Evas_Object *obj, void *event_info);
static Eina_Bool _value_set(Evas_Object *obj, double delta);

static void
_del_hook(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);
   if (!wd) return;
   if (wd->label) eina_stringshare_del(wd->label);
   if (wd->delay) ecore_timer_del(wd->delay);
   free(wd);
}

static void
_disable_hook(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);
   if (elm_widget_disabled_get(obj))
     edje_object_signal_emit(wd->spinner, "elm,state,disabled", "elm");
   else
     edje_object_signal_emit(wd->spinner, "elm,state,enabled", "elm");
}

static void
_theme_hook(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);
   _elm_theme_set(wd->spinner, "spinner", "base", elm_widget_style_get(obj));
   edje_object_part_swallow(wd->spinner, "elm.swallow.entry", wd->ent);
   _write_label(obj);
   edje_object_message_signal_process(wd->spinner);
   edje_object_scale_set(wd->spinner, elm_widget_scale_get(obj) * _elm_config->scale);
   _sizing_eval(obj);
}

static int
_delay_change(void *data)
{
   
   Widget_Data *wd = elm_widget_data_get(data);
   wd->delay = NULL;
   evas_object_smart_callback_call(data, "delay,changed", NULL);
   return 0;
}

static void
_entry_show(Widget_Data *wd)
{
   char buf[32], fmt[32] = "%0.f";

   /* try to construct just the format from given label
    * completely ignoring pre/post words
    */
   if (wd->label)
     {
        const char *start = strchr(wd->label, '%');
        while (start)
          {
	     /* handle %% */
	     if (start[1] != '%')
	       break;
	     else
	       start = strchr(start + 2, '%');
          }

        if (start)
          {
	     const char *itr, *end = NULL;
	     for (itr = start + 1; *itr != '\0'; itr++)
	       {
		  /* allowing '%d' is quite dangerous, remove it? */
		  if ((*itr == 'd') || (*itr == 'f'))
		    {
		       end = itr + 1;
		       break;
		    }
	       }

	     if ((end) && ((size_t)(end - start + 1) < sizeof(fmt)))
	       {
		  memcpy(fmt, start, end - start);
		  fmt[end - start] = '\0';
	       }
          }
     }
   snprintf(buf, sizeof(buf), fmt, wd->val);
   elm_entry_entry_set(wd->ent, buf);
}

static void
_write_label(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);
   char buf[1024];

   if (wd->label)
     snprintf(buf, sizeof(buf), wd->label, wd->val);
   else
     snprintf(buf, sizeof(buf), "%.0f", wd->val);
   edje_object_part_text_set(wd->spinner, "elm.text", buf);

   if (wd->entry_visible)
     _entry_show(wd);
}

static Eina_Bool 
_value_set(Evas_Object *obj, double delta)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);
   double new_val;

   new_val = wd->val + delta;
   if (wd->wrap)
     {
        while (new_val < wd->val_min)
          new_val = wd->val_max + new_val + 1 - wd->val_min;
        while (new_val > wd->val_max)
          new_val = wd->val_min + new_val - wd->val_max;
     }
   else
     {
        if (new_val < wd->val_min)
          new_val = wd->val_min;
        else if (new_val > wd->val_max)
          new_val = wd->val_max;
     }

   if (new_val == wd->val) return 0;
   wd->val = new_val;

   evas_object_smart_callback_call(obj, "changed", NULL);
   if (wd->delay) ecore_timer_del(wd->delay);
   wd->delay = ecore_timer_add(0.2, _delay_change, obj);

   return 1;
}

static void
_sizing_eval(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);
   Evas_Coord minw = -1, minh = -1;

   if (!wd) return;
   elm_coords_finger_size_adjust(1, &minw, 1, &minh);
   edje_object_size_min_restricted_calc(wd->spinner, &minw, &minh, minw, minh);
   elm_coords_finger_size_adjust(1, &minw, 1, &minh);
   evas_object_size_hint_min_set(obj, minw, minh);
   evas_object_size_hint_max_set(obj, -1, -1);
}

static void
_changed_size_hints(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   _sizing_eval(data);
}

static void
_val_set(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);
   double pos = 0.0;

   if (wd->val_max > wd->val_min)
     pos = ((wd->val - wd->val_min) / (wd->val_max - wd->val_min));
   if (pos < 0.0) pos = 0.0;
   else if (pos > 1.0) pos = 1.0;
   edje_object_part_drag_value_set(wd->spinner, "elm.dragable.slider", 
                                   pos, pos);
}

static void
_drag(void *data, Evas_Object *obj, const char *emission, const char *source)
{
   
   Widget_Data *wd = elm_widget_data_get(data);
   double pos = 0.0, offset, delta;

   if (wd->entry_visible) return;
   edje_object_part_drag_value_get(wd->spinner, "elm.dragable.slider",
				   &pos, NULL);
   offset = wd->step;
   delta = (pos - wd->drag_start_pos) * offset;
   if (_value_set(data, delta))
     _write_label(data);

   wd->drag_start_pos = pos;
   wd->dragging = 1;
}

static void
_drag_start(void *data, Evas_Object *obj, const char *emission, const char *source)
{
   
   Widget_Data *wd = elm_widget_data_get(data);
   double pos;

   edje_object_part_drag_value_get(wd->spinner, "elm.dragable.slider",
				   &pos, NULL);
   wd->drag_start_pos = pos;
}

static void
_drag_stop(void *data, Evas_Object *obj, const char *emission, const char *source)
{
   
   Widget_Data *wd = elm_widget_data_get(data);

   wd->drag_start_pos = 0;
   edje_object_part_drag_value_set(wd->spinner, "elm.dragable.slider", 0.0, 0.0);
}

static void
_hide_entry(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);

   if (!wd) return;
   edje_object_signal_emit(wd->spinner, "elm,state,inactive", "elm");
   wd->entry_visible = 0;
}

static void
_reset_value(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);

   if (!wd) return;
   _hide_entry(obj);
   elm_spinner_value_set(obj, wd->orig_val);
}

static void
_apply_entry_value(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);
   const char *str;
   char *end;
   double val;

   if (!wd) return;
   _hide_entry(obj);

   str = elm_entry_entry_get(wd->ent);
   if (!str) return;
   val = strtod(str, &end);
   if ((*end != '\0') && (!isspace(*end))) return;
   elm_spinner_value_set(obj, val);
}

static void
_toggle_entry(void *data, Evas_Object *obj, const char *emission, const char *source)
{
   
   Widget_Data *wd = elm_widget_data_get(data);

   if (!wd) return;
   if (wd->dragging)
     {
        wd->dragging = 0;
        return;
     }
   if (elm_widget_disabled_get(data)) return;
   if (wd->entry_visible)
     _apply_entry_value(data);
   else
     {
        wd->orig_val = wd->val;
	edje_object_signal_emit(wd->spinner, "elm,state,active", "elm");
        _entry_show(wd);
        elm_entry_select_all(wd->ent);
	elm_widget_focus_set(wd->ent, 1);
        wd->entry_visible = 1;
     }
}

static int
_spin_value(void *data)
{
   
   Widget_Data *wd = elm_widget_data_get(data);

   if (!wd) return ECORE_CALLBACK_CANCEL;
   if (_value_set(data, wd->spin_speed))
     _write_label(data);

   wd->interval = wd->interval / 1.05;
   ecore_timer_interval_set(wd->spin, wd->interval);
   return ECORE_CALLBACK_RENEW;
}

static void
_val_inc_start(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);

   if (!wd) return;
   wd->interval = 0.85;
   wd->spin_speed = wd->step;
   if (wd->spin) ecore_timer_del(wd->spin);
   wd->spin = ecore_timer_add(wd->interval, _spin_value, obj);
   _spin_value(obj);
}

static void
_val_inc_stop(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);

   if (!wd) return;
   wd->interval = 0.85;
   wd->spin_speed = 0;
   if (wd->spin) ecore_timer_del(wd->spin);
   wd->spin = NULL;
}

static void
_val_dec_start(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);

   if (!wd) return;
   wd->interval = 0.85;
   wd->spin_speed = -wd->step;
   if (wd->spin) ecore_timer_del(wd->spin);
   wd->spin = ecore_timer_add(wd->interval, _spin_value, obj);
   _spin_value(obj);
}

static void
_val_dec_stop(Evas_Object *obj)
{
   
   Widget_Data *wd = elm_widget_data_get(obj);

   if (!wd) return;
   wd->interval = 0.85;
   wd->spin_speed = 0;
   if (wd->spin) ecore_timer_del(wd->spin);
   wd->spin = NULL;
}

static void
_button_inc_start(void *data, Evas_Object *obj, const char *emission, const char *source)
{
   
   Widget_Data *wd = elm_widget_data_get(data);

   if (!wd) return;
   if (wd->entry_visible)
     {
        _reset_value(data);
        return;
     }
   _val_inc_start(data);
}

static void
_button_inc_stop(void *data, Evas_Object *obj, const char *emission, const char *source)
{
   
   Widget_Data *wd = elm_widget_data_get(data);

   if (!wd) return;
   _val_inc_stop(data);
}

static void
_button_dec_start(void *data, Evas_Object *obj, const char *emission, const char *source)
{
   
   Widget_Data *wd = elm_widget_data_get(data);

   if (!wd) return;
   if (wd->entry_visible)
     {
        _reset_value(data);
        return;
     }
   _val_dec_start(data);
}

static void
_button_dec_stop(void *data, Evas_Object *obj, const char *emission, const char *source)
{
   
   Widget_Data *wd = elm_widget_data_get(data);

   if (!wd) return;
   _val_dec_stop(data);
}

static void
_entry_activated(void *data, Evas_Object *obj, void *event_info)
{
   
   Widget_Data *wd = elm_widget_data_get(data);

   if (!wd) return;

   _apply_entry_value(data);

   evas_object_smart_callback_call(data, "changed", NULL);
   if (wd->delay) ecore_timer_del(wd->delay);
   wd->delay = ecore_timer_add(0.2, _delay_change, data);
}

static void
_entry_event_key_down(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev = event_info;
   
   Widget_Data *wd = elm_widget_data_get(data);

   if (!wd) return;
   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (!strcmp(ev->keyname, "Up"))
     _val_inc_start(data);
   else if (!strcmp(ev->keyname, "Down"))
     _val_dec_start(data);
}

static void
_entry_event_key_up(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev = event_info;
   
   Widget_Data *wd = elm_widget_data_get(data);

   if (!wd) return;
   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (!strcmp(ev->keyname, "Up"))
     _val_inc_stop(data);
   else if (!strcmp(ev->keyname, "Down"))
     _val_dec_stop(data);
   else if (!strcmp(ev->keyname, "Escape"))
     _reset_value(data);
}

/**
 * Add a new spinner to the parent
 *
 * @param parent The parent object
 * @return The new object or NULL if it cannot be created
 *
 * @ingroup Spinner
 */
EAPI Evas_Object *
elm_spinner_add(Evas_Object *parent)
{
   Evas_Object *obj;
   Evas *e;
   Widget_Data *wd;

   wd = ELM_NEW(Widget_Data);
   e = evas_object_evas_get(parent);
   obj = elm_widget_add(e);
   ELM_SET_WIDTYPE(widtype, "spinner");
   elm_widget_type_set(obj, "spinner");
   elm_widget_sub_object_add(parent, obj);
   elm_widget_data_set(obj, wd);
   elm_widget_del_hook_set(obj, _del_hook);
   elm_widget_theme_hook_set(obj, _theme_hook);
   elm_widget_disable_hook_set(obj, _disable_hook);

   wd->val = 0.0;
   wd->val_min = 0.0;
   wd->val_max = 100.0;
   wd->wrap = 0;
   wd->step = 1.0;
   wd->entry_visible = 0;

   wd->spinner = edje_object_add(e);
   _elm_theme_set(wd->spinner, "spinner", "base", "default");
   elm_widget_resize_object_set(obj, wd->spinner);
   edje_object_signal_callback_add(wd->spinner, "drag", "*", _drag, obj);
   edje_object_signal_callback_add(wd->spinner, "drag,start", "*", 
                                   _drag_start, obj);
   edje_object_signal_callback_add(wd->spinner, "drag,stop", "*", 
                                   _drag_stop, obj);
   edje_object_signal_callback_add(wd->spinner, "drag,step", "*", 
                                   _drag_stop, obj);
   edje_object_signal_callback_add(wd->spinner, "drag,page", "*", 
                                   _drag_stop, obj);

   edje_object_signal_callback_add(wd->spinner, "elm,action,increment,start", 
                                   "*", _button_inc_start, obj);
   edje_object_signal_callback_add(wd->spinner, "elm,action,increment,stop", 
                                   "*", _button_inc_stop, obj);
   edje_object_signal_callback_add(wd->spinner, "elm,action,decrement,start", 
                                   "*", _button_dec_start, obj);
   edje_object_signal_callback_add(wd->spinner, "elm,action,decrement,stop", 
                                   "*", _button_dec_stop, obj);
   edje_object_part_drag_value_set(wd->spinner, "elm.dragable.slider", 
                                   0.0, 0.0);

   evas_object_event_callback_add(wd->spinner, EVAS_CALLBACK_KEY_DOWN, 
                                  _entry_event_key_down, obj);
   evas_object_event_callback_add(wd->spinner, EVAS_CALLBACK_KEY_UP, 
                                  _entry_event_key_up, obj);

   wd->ent = elm_entry_add(obj);
   elm_entry_single_line_set(wd->ent, 1);
   evas_object_smart_callback_add(wd->ent, "activated", _entry_activated, obj);
   edje_object_part_swallow(wd->spinner, "elm.swallow.entry", wd->ent);
   edje_object_signal_callback_add(wd->spinner, "elm,action,entry,toggle", 
                                   "*", _toggle_entry, obj);

   _write_label(obj);
   _sizing_eval(obj);
   return obj;
}

/**
 * Set the format string of the label area
 *
 * If NULL, this sets the format to "%.0f". If not it sets the format
 * string for the label text. The label text is provided a floating point
 * value, so the label text can display up to 1 floating point value. Note that
 * this is optional. Use a format string such as "%1.2f meters" for example.
 *
 * @param obj The spinner object
 * @param fmt The format string for the label display
 *
 * @ingroup Spinner
 */
EAPI void
elm_spinner_label_format_set(Evas_Object *obj, const char *fmt)
{
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);

   if (wd->label) eina_stringshare_del(wd->label);
   wd->label = eina_stringshare_add(fmt);
   _write_label(obj);
   _sizing_eval(obj);
}

/**
 * Get the label format of the spinner
 *
 * @param obj The spinner object
 * @return The text label format string in UTF-8
 *
 * @ingroup Spinner
 */
EAPI const char *
elm_spinner_label_format_get(Evas_Object *obj)
{
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);

   if (!wd) return NULL;
   return wd->label;
}

/**
 * Set the minimum and maximum values for the spinner
 *
 * Maximum mut be greater than minimum.
 *
 * @param obj The spinner object
 * @param min The minimum value
 * @param max The maximum value
 *
 * @ingroup Spinner
 */
EAPI void
elm_spinner_min_max_set(Evas_Object *obj, double min, double max)
{
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);

   if ((wd->val_min == min) && (wd->val_max == max)) return;
   wd->val_min = min;
   wd->val_max = max;
   if (wd->val < wd->val_min) wd->val = wd->val_min;
   if (wd->val > wd->val_max) wd->val = wd->val_max;
   _val_set(obj);
   _write_label(obj);
}

/**
 * Set the step for the spinner
 *
 * @param obj The spinner object
 * @param step The step value
 *
 * @ingroup Spinner
 */
EAPI void
elm_spinner_step_set(Evas_Object *obj, double step)
{
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   wd->step = step;
}

/**
 * Set the value the spinner indicates
 *
 * @param obj The spinner object
 * @param val The value (must be beween min and max for the spinner)
 *
 * @ingroup Spinner
 */
EAPI void
elm_spinner_value_set(Evas_Object *obj, double val)
{
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);

   if (wd->val == val) return;
   wd->val = val;
   if (wd->val < wd->val_min) wd->val = wd->val_min;
   if (wd->val > wd->val_max) wd->val = wd->val_max;
   _val_set(obj);
   _write_label(obj);
}

/**
 * Get the value the spinner has
 *
 * @param obj The spinner object
 * @return The value of the spinner
 *
 * @ingroup Spinner
 */
EAPI double
elm_spinner_value_get(const Evas_Object *obj)
{
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   return wd->val;
}

/**
 * Sets whether the spinner should wrap when it reaches its
 * minimum/maximum value
 * 
 * @param obj The spinner object
 * @param wrap True if it should wrap, false otherwise
 *
 * @ingroup Spinner
 */
EAPI void
elm_spinner_wrap_set(Evas_Object *obj, Eina_Bool wrap)
{
   ELM_CHECK_WIDTYPE(obj, widtype);
   Widget_Data *wd = elm_widget_data_get(obj);
   wd->wrap = wrap;
}
