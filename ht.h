#include <stdlib.h>

struct ht_item {
  char *key;
  char *value;
  struct ht_item *next;
};

struct ht_table {
  unsigned int size;
  struct ht_item **table;
};

struct ht_table *ht_init(int size);
void *ht_get(struct ht_table *ctx, const char *key);
void ht_set(struct ht_table *ctx, const char* key, void *value);
