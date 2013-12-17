// 

struct value_list
  {
    char * value_name;
    int64_t value;
  }

typedef struct config_list
  {
    char * name; // opt name
    int64_t min, max; // value limits
    value_list_t * value_list;
    void * result;
    enum { cf_uint64 } result_type;
  } config_list_t;

int cfgparse (char * tag, char * cptr, config_list_t * clist)
  {
    char * copy = strdup (cptr);
    char * start = copy;
    char * statement_save = NULL;

    for (;;) // process statements
      {
        char * statement;
        statement = strtok_r (start, ";", & statement_save);
        start = NULL;
        if (! statement)
          break;


        // process statement

        // extract name
        char * name_start = statement;
        char * name_save = NULL;
        char * name;
        name = strtok_r (name_start, "=", & name_save);
        if (! name)
          {
            sim_debug (DBG_ERR, & scu_dev, "%s: can't parse name\n", tag);
            sim_printf ("error: %s: can't parse name\n", tag);
            break;
          }

        // lookup name
      } // process statements
    free (copy);

    return 0;
  }


