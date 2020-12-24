
/* Used by main to communicate with parse_opt. */
struct cb_backup_ctx
{               
  int method;
  char *category;
  char *path;
  int all_drives;
  int num_drives;
  char *drives[4];
  int ref;
};