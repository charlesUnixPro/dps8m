// 
//
//{ "foo", 0, 7, NULL, & foo_val},
//{ "memsize", 1, 0, memsizes, & mem_size_val}
//{ NULL, 0, 0, NULL, NULL}
//
// {"16K", 0 },
// {"32K", 1 } 

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define sim_printf printf

typedef struct value_list
  {
    char * value_name;
    int64_t value;
  } value_list_t;

typedef struct config_list
  {
    char * name; // opt name
    int64_t min, max; // value limits
    value_list_t * value_list;
  } config_list_t;

typedef struct config_state
  {
    char * copy;
    char * statement_save;
  } config_state_t;

// return
//   0-n: selected option
//   -1: done
//   -2: error

int cfgparse (char * tag, char * cptr, config_list_t * clist, config_state_t * state, int64_t * result)
  {
    char * start = NULL;
    if (! state -> copy)
      {
        state -> copy = strdup (cptr);
        start = state -> copy;
        state ->  statement_save = NULL;
      }

    int ret = -2; // error

    // grab every thing up to the next semicolon
    char * statement;
    statement = strtok_r (start, ";", & state -> statement_save);
    start = NULL;
    if (! statement)
      {
        ret = -1; // done
        goto done;
      }

    // extract name
    char * name_start = statement;
    char * name_save = NULL;
    char * name;
    name = strtok_r (name_start, "=", & name_save);
    if (! name)
      {
        sim_printf ("error: %s: can't parse name\n", tag);
        goto done;
      }

    // lookup name
    config_list_t * p = clist;
    while (p -> name)
      {
        if (strcasecmp (name, p -> name) == 0)
          break;
        p ++;
      }
    if (! p -> name)
      {
        sim_printf ("error: %s: don't know name\n", tag);
        goto done;
      }

    // extract value
    char * value;
    value = strtok_r (NULL, "", & name_save);
    if (! value)
      {
        // Special case; min>max and no value list
        // means that a missing value is ok
        if (p -> min > p -> max && ! p -> value_list)
          {
            return p - clist;
          }
        sim_printf ("error: %s: can't parse value\n", tag);
        goto done;
      }

    // first look to value in the value list
    value_list_t * v = p -> value_list;
    if (v)
      {
        while (v -> value_name)
          {
            if (strcasecmp (value, v -> value_name) == 0)
              break;
            v ++;
          }

        // Hit?
        if (v -> value_name)
          {
            * result = v -> value;
            return p - clist;
          }
      }

    // Must be a number

    if (p -> min > p -> max)
      {
        sim_printf ("error: %s: can't parse value\n", tag);
        goto done;
      }

    if (strlen (value) == 0)
      {
         sim_printf ("error: %s: missing value\n", tag);
         goto done;
      }
    char * endptr;
    int64_t n = strtoll (value, & endptr, 0);
    if (* endptr)
      {
        sim_printf ("error: %s: can't parse value\n", tag);
        goto done;
      } 

    if (n < p -> min || n > p -> max)
      {
        sim_printf ("error: %s: value out of range\n", tag);
        goto done;
      } 
    
    * result = n;
    return p - clist;

done:
    free (state -> copy);
    state -> copy= NULL;
    return ret;
  }

//// Test code

static value_list_t onoff [] =
  {
    { "off", 0 },
    { "on",  1 },
    { NULL }
  };

static config_list_t cfg [] =
  {
    { "novalue", 1, 0, NULL },
    { "onoff", 1, 0, onoff },
    { "1..10", 1, 10, NULL },
    { "-1..1", -1, 1, NULL },
    { NULL }
  };

static config_state_t cfg_state = { NULL };

int main (int argc, char * argv [])
  {
    int rc;
    for (;;)
      {
        int64_t v;
        rc = cfgparse ("test", argv [1], cfg, & cfg_state, & v);
        printf ("%d: %s: %ld\n", rc, rc < 0 ? "" : cfg [rc] . name, v);
        if (rc < 0)
          break;
      }
    return rc;
  }
