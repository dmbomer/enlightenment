/** @file etk_argument.h */
#ifndef _ETK_ARGUMENT_H_
#define _ETK_ARGUMENT_H_

#include "etk_types.h"

/**
 * @defgroup Etk_Argument Etk_Argument
 * @{
 */

enum _Etk_Argument_Returns
{
   ETK_ARGUMENT_RETURN_OK = 1,                   /* no problems */
   ETK_ARGUMENT_RETURN_REQUIRED_NOT_FOUND = 2,   /* required arg not found */
   ETK_ARGUMENT_RETURN_REQUIRED_VALUE_NOT_FOUND = 3, /* required value for arg no found */
   ETK_ARGUMENT_RETURN_MALFORMED = 4             /* malformed argument */  
};

enum _Etk_Argument_Flags
{
   ETK_ARGUMENT_FLAG_REQUIRED = 1 << 0,     /* argument itself is required */
   ETK_ARGUMENT_FLAG_OPTIONAL = 1 << 1,     /* argument itself is optional */
   ETK_ARGUMENT_FLAG_VALUE_REQUIRED = 1 << 2, /* value of the argument is required */
   ETK_ARGUMENT_FLAG_MULTIVALUE = 1 << 3,   /* argument uses multi-valued args */
   ETK_ARGUMENT_FLAG_NONE = 1 << 4          /* used when terminating options */
};

struct _Etk_Argument
{
   char *long_name;           /* long name of argument: --foo */
   char  short_name;          /* short name of argument: -f */
   Evas_List *data;                /* filled with value of argument: -f blah */
   void (*func)(Etk_Argument *args, int index); /* callback */
   void *func_data;           /* data to the callback */
   Etk_Argument_Flags flags;  /* flags */
   char *description;         /* description of the argument for help */
};

int etk_arguments_parse(Etk_Argument *args, int argc, char **argv);
Evas_List *etk_argument_extra_find(const char *key);
  
/** @} */

#endif
