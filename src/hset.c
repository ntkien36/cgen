/*
  (C) Nguyen Ba Ngoc 2o21
*/

#include "hset.h"

#include <stdio.h>
#include <stdlib.h>

static void hset_setup_storage(hset_t hs);
static inline int hset_lookup_node(hset_t hs, gtype key, uint *hash_return);
static inline int hset_maybe_realloc(hset_t hs);
static void hset_remove_node(hset_t hs, int i);
static void hset_free_nodes(hset_t hs);

hset_t hset_create(gtype_hash_t hash_func, gtype_cmp_t cmp,
          gtype_free_t free_key) {
  hset_t hs = malloc(sizeof(struct hash_set_s));
  hs->size = 0;
  hs->noccupied = 0;
  hs->hash_func = hash_func;
  hs->cmp = cmp;
  hs->free_key = free_key;
  hset_setup_storage(hs);
  return hs;
}

int hset_index_of(hset_t hs, gtype key) {
  int node_index = hset_lookup_node(hs, key, NULL);
  return (HASH_IS_REAL(elem(hs->hashes, node_index)))?
          node_index: INDEX_NOT_FOUND;
}

int hset_insert(hset_t hs, gtype key) {
  uint *hashes = ARR(hs->hashes);
  gtype *keys = ARR(hs->keys);
  uint key_hash;
  int node_index = hset_lookup_node(hs, key, &key_hash);
  uint curr_hash = hashes[node_index];
  int already_exists = HASH_IS_REAL(curr_hash);
  if (already_exists) {
    return 0;
  }
  hashes[node_index] = key_hash;
  keys[node_index] = key;
  hs->size++;
  if (HASH_IS_UNUSED(curr_hash)) {
    hs->noccupied++;
    hset_maybe_realloc(hs);
  }
  return 1;
}

int hset_remove(hset_t hs, gtype key) {
  int node_index = hset_lookup_node(hs, key, NULL);
  if (HASH_IS_NOTREAL(elem(hs->hashes, node_index))) {
    return 0;
  }
  hset_remove_node(hs, node_index);
  hset_maybe_realloc(hs);
  return 1;
}

void hset_clear(hset_t hs) {
  hset_free_nodes(hs);
  hset_setup_storage(hs);
}

void hset_free(hset_t hs) {
  hset_free_nodes(hs);
  free(hs);
}

void hset_gtype_free(gtype value) {
  hset_t s = value.v;
  hset_free(s);
}

gtype *hset_next_pkey(hset_t hs, gtype* curr) {
  gtype * r;
  hashes_next_pkey_or_pvalue(hs, curr, keys, r);
  return r;
}

static void hset_set_shift(hset_t hs, int shift) {
  hs->capacity = 1 << shift;
  hs->mod = prime_mod[shift];
  hs->mask = hs->capacity - 1;
}

static int hset_find_closest_shift(int n) {
  int i = 0;
  for (; n; ++i) {
    n >>= 1;
  }
  return i;
}

static void hset_set_shift_from_capacity(hset_t hs, int capacity) {
  int shift = hset_find_closest_shift(capacity);
  shift = MAX(shift, HASH_MIN_SHIFT);
  hset_set_shift(hs, shift);
}

#define hset_hash_to_index(hs, hash)((hash * 11) % hs->mod)

static void hset_setup_storage(hset_t hs) {
  hset_set_shift(hs, HASH_MIN_SHIFT);
  hs->hashes = arr_create(hs->capacity, uint);
  memset(ARR(hs->hashes), 0, hs->capacity * sizeof(uint));
  hs->keys = arr_create(hs->capacity, gtype);
}

static inline int hset_lookup_node(hset_t hs, gtype key, uint *hash_return) {
  uint lookup_hash = hs->hash_func(key);
  if (HASH_IS_NOTREAL(lookup_hash)) {
    lookup_hash = MIN_HASH_VALUE;
  }
  if (hash_return) {
    *hash_return = lookup_hash;
  }
  uint *hashes = ARR(hs->hashes);
  gtype *keys = ARR(hs->keys);
  int node_index = hset_hash_to_index(hs, lookup_hash);
  uint node_hash = hashes[node_index];
  int first_deleted = -1;
  int step = 0;
  while (HASH_IS_INUSED(node_hash)) {
    if (node_hash == lookup_hash) {
      gtype node_key = keys[node_index];
      if (hs->cmp(node_key, key) == 0) {
        return node_index;
      }
    } else if (HASH_IS_DELETED(node_hash) && first_deleted < 0) {
      first_deleted = node_index;
    }
    ++step;
    node_index += step;
    node_index &= hs->mask;
    node_hash = hashes[node_index];
  }
  if (first_deleted >= 0) {
    return first_deleted;
  }
  return node_index;
}

static void hset_remove_node(hset_t hs, int i) {
  gtype key = elem(hs->keys, i);
  elem(hs->hashes, i) = DELETED_HASH_VALUE;
  hs->size--;
  if (hs->free_key) {
    hs->free_key(key);
  }
}

static void hset_free_nodes(hset_t hs) {
  int capacity = hs->capacity;
  gtype *keys = ARR(hs->keys);
  uint *hashes = ARR(hs->hashes);
  for (int i = 0; i < capacity; ++i) {
    if (HASH_IS_REAL(hashes[i])) {
      if (hs->free_key) {
        hs->free_key(keys[i]);
      }
    }
  }
  arr_free(hs->keys);
  arr_free(hs->hashes);

  hs->size = 0;
  hs->noccupied = 0;
}

static void hset_realloc_arrays(hset_t hs) {
  arr_set_capacity(hs->hashes, hs->capacity);
  arr_set_capacity(hs->keys, hs->capacity);
}

static void relocate_set(hset_t hs, uint old_capacity, uint32 *reallocated_buckets_bitmap) {
  uint *hashes = ARR(hs->hashes);
  gtype *keys = ARR(hs->keys);
  for (int i = 0; i < old_capacity; ++i) {
    uint node_hash = hashes[i];
    gtype key;
    if (HASH_IS_NOTREAL(node_hash)) {
      hashes[i] = UNUSED_HASH_VALUE;
      continue;
    }
    if (get_status_bit(reallocated_buckets_bitmap, i)) {
      continue;
    }
    hashes[i] = UNUSED_HASH_VALUE;
    key = keys[i];
    for (;;) {
      uint idx, replaced_hash, step = 0;
      idx = hset_hash_to_index(hs, node_hash);
      while (get_status_bit(reallocated_buckets_bitmap, idx)) {
        ++step;
        idx += step;
        idx &= hs->mask;
      }
      set_status_bit(reallocated_buckets_bitmap, idx);
      replaced_hash = hashes[idx];
      hashes[idx] = node_hash;
      if (HASH_IS_NOTREAL(replaced_hash)) {
        keys[idx] = key;
        break;
      }
      node_hash = replaced_hash;
      evict_key_or_value(keys, idx, key, key);
    }
  }
}

static void hset_realloc(hset_t hs) {
  uint32 *reallocated_buckets_bitmap;
  int old_capacity;
  old_capacity = hs->capacity;
  hset_set_shift_from_capacity(hs, hs->size * 1.333);
  if (hs->capacity > old_capacity) {
    hset_realloc_arrays(hs);
    memset(ARR(hs->hashes) + old_capacity, 0, (hs->capacity - old_capacity) * sizeof(uint));
    reallocated_buckets_bitmap = calloc((hs->capacity + 31) / 32, sizeof(uint32));
  } else {
    reallocated_buckets_bitmap = calloc((old_capacity + 31) / 32, sizeof(uint32));
  }

  relocate_set(hs, old_capacity, reallocated_buckets_bitmap);
  free(reallocated_buckets_bitmap);
  if (hs->capacity < old_capacity) {
    hset_realloc_arrays(hs);
  }
  hs->noccupied = hs->size;
}

static inline int hset_maybe_realloc(hset_t hs) {
  uint noccupied = hs->noccupied, capacity = hs->capacity;
  if ((capacity > hs->size * 4 && capacity > 1 << HASH_MIN_SHIFT) ||
      (capacity <= noccupied + (noccupied/16))) {
    hset_realloc(hs);
    return 1;
  }
  return 0;
}
