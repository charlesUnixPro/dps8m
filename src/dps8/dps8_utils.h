// Interface for cfgparse

typedef struct config_value_list
  {
    const char * value_name;
    int64_t value;
  } config_value_list_t;

typedef struct config_list
  {
    const char * name; // opt name
    int64_t min, max; // value limits
    config_value_list_t * value_list;
  } config_list_t;

typedef struct config_state
  {
    char * copy;
    char * statement_save;
  } config_state_t;

int cfgparse (const char * tag, char * cptr, config_list_t * clist, config_state_t * state, int64_t * result);
void cfgparse_done (config_state_t * state);

