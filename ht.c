#include "ht.h"
#include <string.h>
#include <stdlib.h>

unsigned long ht_hash(struct ht_table *ctx, const char *key) {
  /* Note this hash function was taken from http://www.cse.yorku.ca/~oz/hash.html */
  unsigned long hash = 5381;
  int c;

  while (c = *key++)
      hash = c + (hash << 6) + (hash << 16) - hash;

  return hash % ctx->size;
}

struct ht_item *ht_item_init(const char *key, void *value) {
  struct ht_item *new_item = (struct ht_item *)malloc(sizeof(struct ht_item));
  memset(new_item, 0, sizeof(struct ht_item));
  new_item->key = key;
  new_item->value = value;

  return new_item;
}

struct ht_table *ht_init(int size) {
  struct ht_table *ctx = (struct ht_table *)malloc(sizeof(struct ht_table));  
  memset(ctx, 0, sizeof(struct ht_table));

  ctx->size = size;

  ctx->table = (struct ht_item **)malloc(sizeof(struct ht_item)*size);
  memset(ctx->table, 0, sizeof(struct ht_item)*size);

  return ctx;
}

void ht_set(struct ht_table *ctx, const char* key, void *value) {
  unsigned long index = ht_hash(ctx, key);
  struct ht_item *item = ctx->table[index];
  struct ht_item *previous_item = NULL;

  if(!item) {
    /* set hash if it doesn't exist */
    ctx->table[index] = ht_item_init(key, value);
  } else {
    /* search linked list to see if key already exists */
    do {
      /* if we already have the key, replace the value */
      if(!strcmp(item->key, key)) {
        item->value = value;
        return;
      }

      previous_item = item;
      item = item->next;

    } while(item);

    /* we didnt find the key so add to the end of the linked list */
    previous_item->next = ht_item_init(key, value);
  }

  return;
}

void *ht_get(struct ht_table *ctx, const char *key) {
  void *value = NULL;

  unsigned int index = ht_hash(ctx, key) % ctx->size;
  struct ht_item *item = ctx->table[index];

  while(item) {
    if(!strcmp(item->key, key)) {
      value = item->value;
      break;
    }
    item = item->next;
  }

  return value;
}

void ht_free(struct ht_table *ctx) {
  /* TODO */
}
