#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "eclipse.h"

/* get filename from uri */
char *get_shortname_for(const char *filename);

Evas_List *eclips_file_add_dir(Evas_List *eo, char *file);

int eclips_file_is_dir(char *file);
    
