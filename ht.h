#ifndef MULTIPROXY_HT
#define MULTIPROXY_HT

struct ht_item {
  const char *key;
  char *value;
  struct ht_item *next;
};

struct ht_table {
  unsigned int size;
  struct ht_item **table;
};

struct ht_table *ht_init(int size);
struct ht_item *ht_item_init(const char *key, void *value);
void *ht_get(struct ht_table *ctx, const char *key);
void ht_set(struct ht_table *ctx, const char* key, void *value);

#endif /* MULTIPROXY_HT */
