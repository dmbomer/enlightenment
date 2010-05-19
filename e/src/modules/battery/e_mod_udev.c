#include "e.h"
#include "e_mod_main.h"

static void _battery_udev_event_battery(const char *syspath, const char *event, void *data, Eeze_Udev_Watch *watch);
static void _battery_udev_event_ac(const char *syspath, const char *event, void *data, Eeze_Udev_Watch *watch);
static void _battery_udev_battery_add(const char *syspath);
static void _battery_udev_ac_add(const char *syspath);
static void _battery_udev_battery_del(const char *syspath);
static void _battery_udev_ac_del(const char *syspath);
static int _battery_udev_battery_update_poll(void *data);
static void _battery_udev_battery_update(const char *syspath, Battery *bat);
static void _battery_udev_ac_update(const char *syspath, Ac_Adapter *ac);

void 
_battery_udev_start(void)
{
   Eina_List *l, *devices;
   const char *dev;
   
   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_POWER_BAT, NULL);
   EINA_LIST_FOREACH(devices, l, dev)
     _battery_udev_battery_add(dev);
   eina_list_free(devices);

   devices = eeze_udev_find_by_type(EEZE_UDEV_TYPE_POWER_AC, NULL);
   EINA_LIST_FOREACH(devices, l, dev)
     _battery_udev_ac_add(dev);
   eina_list_free(devices);

   if (!battery_config->batwatch)
     battery_config->batwatch = eeze_udev_watch_add(EEZE_UDEV_TYPE_POWER_BAT, _battery_udev_event_battery, NULL);
   if (!battery_config->batwatch)
     battery_config->acwatch = eeze_udev_watch_add(EEZE_UDEV_TYPE_POWER_AC, _battery_udev_event_ac, NULL);

   init_time = ecore_time_get();
}

void 
_battery_udev_stop(void)
{
   Ac_Adapter *ac;
   Battery *bat;

   if (battery_config->batwatch)
     eeze_udev_watch_del(battery_config->batwatch);
   if (battery_config->acwatch)
     eeze_udev_watch_del(battery_config->acwatch);

   EINA_LIST_FREE(device_ac_adapters, ac)
     {
        free(ac);
     }
   EINA_LIST_FREE(device_batteries, bat)
     {
        eina_stringshare_del(bat->udi);
        eina_stringshare_del(bat->technology);
        eina_stringshare_del(bat->model);
        eina_stringshare_del(bat->vendor);
        ecore_poller_del(bat->poll);
        free(bat);
     }
}


static void 
_battery_udev_event_battery(const char *syspath, const char *event, void *data, Eeze_Udev_Watch *watch)
{
   if ((!strcmp(event, "add")) || (!strcmp(event, "online")))
     _battery_udev_battery_add(syspath);
   else if ((!strcmp(event, "remove")) || (!strcmp(event, "offline")))
     _battery_udev_battery_del(syspath);
   else /* must be change */
     _battery_udev_battery_update(syspath, data);
}

static void 
_battery_udev_event_ac(const char *syspath, const char *event, void *data, Eeze_Udev_Watch *watch)
{
   if ((!strcmp(event, "add")) || (!strcmp(event, "online")))
     _battery_udev_ac_add(syspath);
   else if ((!strcmp(event, "remove")) || (!strcmp(event, "offline")))
     _battery_udev_ac_del(syspath);
   else /* must be change */
     _battery_udev_ac_update(syspath, data);
}

static void 
_battery_udev_battery_add(const char *syspath)
{
   Battery *bat;

   if ((bat = _battery_battery_find(syspath)))
     {
        eina_stringshare_del(syspath);
        _battery_udev_battery_update(NULL, bat);
        return;
     }

   if (!(bat = E_NEW(Battery, 1)))
     {
        eina_stringshare_del(syspath);
        return;
     }
   bat->last_update = ecore_time_get();
   bat->udi = eina_stringshare_add(syspath);
   bat->poll = ecore_poller_add(ECORE_POLLER_CORE, battery_config->poll_interval, _battery_udev_battery_update_poll, bat);
   device_batteries = eina_list_append(device_batteries, bat);
   _battery_udev_battery_update(syspath, bat);
}

static void 
_battery_udev_ac_add(const char *syspath)
{
   Ac_Adapter *ac;

   if ((ac = _battery_ac_adapter_find(syspath)))
     {
        eina_stringshare_del(syspath);
        _battery_udev_ac_update(NULL, ac);
        return;
     }

   if (!(ac = E_NEW(Ac_Adapter, 1)))
     {
        eina_stringshare_del(syspath);
        return;
     }
   ac->udi = eina_stringshare_add(syspath);
   device_ac_adapters = eina_list_append(device_ac_adapters, ac);
   _battery_udev_ac_update(syspath, ac);
}

static void 
_battery_udev_battery_del(const char *syspath)
{
   Eina_List *l;
   Battery *bat;

   if (!(bat = _battery_battery_find(syspath)))
     {
        eina_stringshare_del(syspath);
        _battery_device_update();
        return;
     }

   l = eina_list_data_find(device_batteries, bat);
   eina_stringshare_del(bat->udi);
   eina_stringshare_del(bat->technology);
   eina_stringshare_del(bat->model);
   eina_stringshare_del(bat->vendor);
   ecore_poller_del(bat->poll);
   free(bat);
   device_batteries = eina_list_remove_list(device_batteries, l);
}

static void 
_battery_udev_ac_del(const char *syspath)
{
   Eina_List *l;
   Ac_Adapter *ac;

   if (!(ac = _battery_ac_adapter_find(syspath)))
     {
        eina_stringshare_del(syspath);
        _battery_device_update();
        return;
     }

   l = eina_list_data_find(device_ac_adapters, ac);
   eina_stringshare_del(ac->udi);
   free(ac);
   device_ac_adapters = eina_list_remove_list(device_ac_adapters, l);
}

static int
_battery_udev_battery_update_poll(void *data)
{
   _battery_udev_battery_update(NULL, data);

   return 1;
}

#define GET_NUM(TYPE, VALUE, PROP) test = eeze_udev_syspath_get_property(TYPE->udi, #PROP); \
  do \
    if (test) \
      { \
         TYPE->VALUE = strtod(test, NULL); \
         eina_stringshare_del(test); \
      } \
  while (0)

#define GET_STR(TYPE, VALUE, PROP) TYPE->VALUE = eeze_udev_syspath_get_property(TYPE->udi, #PROP)
  
static void 
_battery_udev_battery_update(const char *syspath, Battery *bat)
{
   const char *test;
   double time, charge;
   
   if (!bat)
     {
        if (!(bat = _battery_battery_find(syspath)))
          return _battery_udev_battery_add(syspath);
     }
   /* reset the poller */
   ecore_poller_del(bat->poll);
   bat->poll = ecore_poller_add(ECORE_POLLER_CORE, battery_config->poll_interval, _battery_udev_battery_update_poll, bat);
   
   GET_NUM(bat, present, POWER_SUPPLY_PRESENT);
   if (!bat->got_prop)
     {/* only need to get these once */
        GET_STR(bat, technology, POWER_SUPPLY_TECHNOLOGY);
        GET_STR(bat, model, POWER_SUPPLY_MODEL_NAME);
        GET_STR(bat, vendor, POWER_SUPPLY_MANUFACTURER);
        GET_NUM(bat, design_charge, POWER_SUPPLY_ENERGY_FULL_DESIGN);
        if (!bat->design_charge)
          GET_NUM(bat, design_charge, POWER_SUPPLY_CHARGE_FULL_DESIGN);
     }
   GET_NUM(bat, last_full_charge, POWER_SUPPLY_ENERGY_FULL);
     if (!bat->last_full_charge)
       GET_NUM(bat, last_full_charge, POWER_SUPPLY_CHARGE_FULL);
   test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_ENERGY_NOW");
   if (!test)
     {
	eina_stringshare_del(test);
        test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_CHARGE_NOW");
     }
   if (test)
     {

        charge = strtod(test, NULL);
        eina_stringshare_del(test);
        time = ecore_time_get();
        if ((bat->got_prop) && (charge != bat->current_charge))
          bat->charge_rate = ((charge - bat->current_charge) / (time - bat->last_update));
        bat->last_update = time;
        bat->current_charge = charge;
        bat->percent = 100 * (bat->current_charge / bat->last_full_charge);
        if (bat->got_prop)
          {
             if (bat->charge_rate > 0)
               {
                  if (battery_config->fuzzy && (++battery_config->fuzzcount <= 10) && (bat->time_full > 0))
                    bat->time_full = (((bat->last_full_charge - bat->current_charge) / bat->charge_rate) + bat->time_full) / 2;
                  else
                    bat->time_full = (bat->last_full_charge - bat->current_charge) / bat->charge_rate;
                  bat->time_left = -1;
               }
             else
               {
                  if (battery_config->fuzzy && (battery_config->fuzzcount <= 10) && (bat->time_left > 0))
                    bat->time_left = (((0 - bat->current_charge) / bat->charge_rate) + bat->time_left) / 2;
                  else
                    bat->time_left = (0 - bat->current_charge) / bat->charge_rate;
                  bat->time_full = -1;
               }
          }
        else
          {
             bat->time_full = -1;
             bat->time_left = -1;
          }
     }
   if (battery_config->fuzzcount > 10) battery_config->fuzzcount = 0;
   test = eeze_udev_syspath_get_property(bat->udi, "POWER_SUPPLY_STATUS");
   if (test)
     {
        if (!strcmp(test, "Charging"))
          bat->charging = 1;
        else if ((!strcmp(test, "Unknown")) && (bat->charge_rate >= 0))
            bat->charging = 1;
        else
          bat->charging = 0;
        eina_stringshare_del(test);
     }
   else
     bat->charging = 0;
   if (bat->got_prop)
     _battery_device_update();
   bat->got_prop = 1;
}

static void 
_battery_udev_ac_update(const char *syspath, Ac_Adapter *ac)
{
   const char *test;
   
   if (!ac)
     {
        if (!(ac = _battery_ac_adapter_find(syspath)))
          return _battery_udev_ac_add(syspath);
     }

   GET_NUM(ac, present, POWER_SUPPLY_ONLINE);
   /* yes, it's really that simple. */

   _battery_device_update();
}
