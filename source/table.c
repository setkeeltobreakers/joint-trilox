#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "table.h"

#define TOMBSTONE_VAL ((Value) {VAL_NIL, {.number = 1}})
#define IS_TOMBSTONE(value) (value.type == VAL_NIL && value.as.number == 1)

void initTable(Table *table) {
  table->capacity = 0;
  table->count = 0;
  table->entries = NULL;
}

void freeTable(Table *table, VM *vm) {
  FREE_ARRAY(Entry, table->entries, table->capacity, vm);
  initTable(table);
}

static Entry *findEntry(Entry *entries, int capacity, ObjString *key) {
  uint32_t index = key->hash & (capacity - 1);
  Entry *tombstone = NULL;
  
  while (1) {
    Entry *entry = &entries[index];
    if (entry->key == NULL) {
      if (IS_TOMBSTONE(entry->value)) {
	if (tombstone == NULL) tombstone = entry; /* If it's a tombstone, keep going */
      } else {
	return tombstone != NULL ? tombstone : entry; /* if not, return a tombstone if you found it. Otherwise, return empty entry */
      }
    } else if (entry->key == key) {
      return entry; /* Found it */
    }
    
    index = (index + 1) & (capacity - 1);
  }
}

static void adjustCapacity(Table *table, int capacity, VM *vm) {
  Entry *entries = ALLOCATE(Entry, capacity, vm);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  int count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    if (entry->key == NULL) continue;

    Entry *dest = findEntry(entries, capacity, entry->key);
    count++;
    dest->key = entry->key;
    dest->value = entry->value;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity, vm);
  table->entries = entries;
  table->count = count;
  table->capacity = capacity;
}

int tableSet(Table *table, ObjString *key, Value value, VM *vm) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD_FACTOR) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity, vm);
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);
  int isNewKey = entry->key == NULL;
  if (isNewKey && IS_NIL(entry->value)) table->count++;

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

int tableGet(Table *table, ObjString *key, Value *value) {
  if (table->count == 0) return 0;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return 0;

  *value = entry->value;
  return 1;
}

int tableGetN(Table *table, int number, Value *value, Value *key) {
  if (table->count == 0) return 0;
  Entry *entry;
  int j = 0;
  for (int i = 0; i < number; i++) {
    while (table->entries[j].key == NULL && j < table->capacity) {
      j++;
    }
    entry = &table->entries[j];
    j++;
  }

  *value = entry->value;
  *key = OBJECT_VAL(entry->key);
  return 1;
}

void tableAddAll(Table *from, Table *to, VM *vm) {
  for (int i = 0; i < from->capacity; i++) {
    Entry *entry = &from->entries[i];
    if (entry->key != NULL) {
      tableSet(to, entry->key, entry->value, vm);
    }
  }
}

int tableDelete(Table *table, ObjString *key) {
  if (table->count == 0) return 0;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return 0;

  entry->key = NULL;
  entry->value = TOMBSTONE_VAL;
  return 1;
}

ObjString *tableFindString(Table *table, char *chars, int length, uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash & (table->capacity - 1);
  while (1) {
    Entry *entry = &table->entries[index];
    if (entry->key == NULL) { /* If key is null, check for tombstone, if none found, return NULL */
      if (!IS_TOMBSTONE(entry->value)) return NULL;
    } else if (entry->key->length == length	\
	       && entry->key->hash == hash &&	\
	       memcmp(entry->key->chars, chars, length) == 0) {
      /* Found it :) */
      return entry->key;
    }

    index = (index + 1) & (table->capacity - 1);
  }
}

void tableRemoveWhite(Table *table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    if (entry->key != NULL && !entry->key->obj.isMarked) {
      tableDelete(table, entry->key);
    }
  }
}

void printTable(Table *table) {
  printf(":[ ");
  int entryCount = 0;
  for (int i = 0; i < table->capacity; i++) {
    
    Entry *entry = &table->entries[i];
    if (entry->key != NULL) {
      if (entryCount > 0) {
	printf(", ");
      }
      entryCount++;
      printf("%s", entry->key->chars);
      printf(" : ");
      printValue(entry->value);
    }
  }
  printf(" ]");
}
