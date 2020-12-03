//
//  hashtable.c
//  C-ray
//
//  Created by Valtteri on 17.11.2019.
//  Copyright © 2015-2020 Valtteri Koskivuori. All rights reserved.
//

#include "../includes.h"
#include "hashtable.h"

#include <inttypes.h>
#include <assert.h>
#include "logging.h"
#include "../datatypes/vector.h"
#include "assert.h"
#include "fileio.h"
#include "string.h"

#define FNV_OFFSET UINT32_C(0x811C9DC5) // Initial value for an empty hash
#define FNV_PRIME  UINT32_C(0x01000193)

// Default hash map capacity. Must be a power of two.
#define DEFAULT_CAPACITY 8
// Maximum load factor (average number of elements per bucket) in percent.
#define MAX_LOAD_FACTOR_PERCENT 80

uint32_t hashInit(void) {
	return FNV_OFFSET;
}

uint32_t hashCombine(uint32_t h, uint8_t u) {
	return (h ^ u) * FNV_PRIME;
}

uint32_t hashBytes(uint32_t h, const void* bytes, size_t size) {
	for (size_t i = 0; i < size; ++i)
		h = hashCombine(h, ((uint8_t*)bytes)[i]);
	return h;
}

uint32_t hashString(uint32_t h, const char* str) {
	for (size_t i = 0; str[i]; ++i)
		h = hashCombine(h, str[i]);
	return h;
}

static inline bool isPowerOfTwo(size_t i) {
	return (i & (i - 1)) == 0;
}

struct hashtable* newHashtable(bool (*compare)(const void*, const void*)) {
	struct hashtable *hashtable = malloc(sizeof(struct hashtable));
	hashtable->bucketCount = DEFAULT_CAPACITY;
	hashtable->buckets = calloc(DEFAULT_CAPACITY, sizeof(struct bucket*));
	hashtable->elemCount = 0;
	hashtable->compare = compare;
	return hashtable;
}

static inline size_t hashToIndex(struct hashtable *hashtable, uint32_t hash) {
	assert(isPowerOfTwo(hashtable->bucketCount));
	return hash & (hashtable->bucketCount - 1);
}

void* findInHashtable(struct hashtable *hashtable, const void* element, uint32_t hash) {
	struct bucket *bucket = hashtable->buckets[hashToIndex(hashtable, hash)];
	while (bucket) {
		if (bucket->hash == hash && hashtable->compare(&element, &bucket->data))
			return &bucket->data;
		bucket = bucket->next;
	}
	return NULL;
}

static inline bool needsRehash(struct hashtable *hashtable) {
	return hashtable->elemCount * MAX_LOAD_FACTOR_PERCENT / hashtable->bucketCount > 100;
}

static inline void rehash(struct hashtable *hashtable) {
	size_t newBucketCount = 2 * hashtable->bucketCount;
	struct bucket **newBuckets = calloc(newBucketCount, sizeof(struct bucket*));
	struct hashtable newHashtable = {
		.bucketCount = newBucketCount,
		.buckets = newBuckets
	};
	for (size_t i = 0, n = hashtable->bucketCount; i < n; ++i) {
		struct bucket *bucket = hashtable->buckets[i];
		while (bucket) {
			struct bucket *next = bucket->next;
			struct bucket **newBucket = &newBuckets[hashToIndex(&newHashtable, bucket->hash)];
			bucket->next = *newBucket;
			*newBucket = bucket;
			bucket = next;
		}
	}
	hashtable->buckets = newBuckets;
	hashtable->bucketCount = newBucketCount;
}

static inline void insertElement(struct hashtable *hashtable, struct bucket **prev, const void *element, size_t elementSize, uint32_t hash) {
	if (needsRehash(hashtable))
		rehash(hashtable);
	struct bucket *next = malloc(sizeof(struct bucket) + elementSize);
	memcpy(&next->data, element, elementSize);
	next->hash = hash;
	next->next = *prev;
	*prev = next;
	hashtable->elemCount++;
}

static inline bool insertOrReplaceInHashtable(struct hashtable *hashtable, bool isInsert, const void *element, size_t elementSize, uint32_t hash) {
	struct bucket **prev = &hashtable->buckets[hashToIndex(hashtable, hash)];
	struct bucket *bucket = *prev;
	while (bucket) {
		if (bucket->hash == hash && hashtable->compare(&element, &bucket->data)) {
			if (isInsert)
				return false;
			memcpy(&bucket->data, element, elementSize);
			return true;
		}
		prev = &bucket->next;
		bucket = bucket->next;
	}
	insertElement(hashtable, prev, element, elementSize, hash);
	return true;
}

bool insertInHashtable(struct hashtable *hashtable, const void *element, size_t elementSize, uint32_t hash) {
	return insertOrReplaceInHashtable(hashtable, true, element, elementSize, hash);
}

void replaceInHashtable(struct hashtable *hashtable, const void *element, size_t elementSize, uint32_t hash) {
	insertOrReplaceInHashtable(hashtable, false, element, elementSize, hash);
}

void forceInsertInHashtable(struct hashtable *hashtable, const void *element, size_t elementSize, uint32_t hash) {
	insertElement(hashtable, &hashtable->buckets[hashToIndex(hashtable, hash)], element, elementSize, hash);
}

bool removeFromHashtable(struct hashtable *hashtable, const void *element, uint32_t hash) {
	struct bucket **prev = &hashtable->buckets[hashToIndex(hashtable, hash)];
	struct bucket *bucket = *prev;
	while (bucket) {
		if (bucket->hash == hash && hashtable->compare(element, &bucket->data)) {
			*prev = bucket->next;
			free(bucket);
			return true;
		}
		prev = &bucket->next;
		bucket = bucket->next;
	}
	return false;
}

void freeHashtable(struct hashtable *hashtable) {
	for (size_t i = 0, n = hashtable->bucketCount; i < n; ++i) {
		struct bucket *bucket = hashtable->buckets[i];
		while (bucket) {
			struct bucket *next = bucket->next;
			free(bucket);
			bucket = next;
		}
	}
}

bool compareDatabaseEntry(const void* entry1, const void* entry2) {
	return !strcmp(*(const char**)entry1, *(const char**)entry2);
}

struct constantsDatabase* newConstantsDatabase(void) {
	return (struct constantsDatabase*)newHashtable(compareDatabaseEntry);
}

bool existsInDatabase(struct constantsDatabase *database, const char *key) {
	return findInHashtable(&database->hashtable, key, hashString(hashInit(), key)) != NULL;
}

#define DATABASE_ACCESSORS(T, entryName, setterName, getterName, defaultValue) \
	struct entryName { \
		const char *key; \
		void* toFree; \
		T value; \
	}; \
	void setterName(struct constantsDatabase *database, const char *key, T value) { \
		replaceInHashtable( \
			&database->hashtable, \
			&(struct entryName) { .key = stringCopy(key), .value = value }, \
			sizeof(struct entryName), \
			hashString(hashInit(), key)); \
	} \
	T getterName(struct constantsDatabase *database, const char *key) { \
		struct entryName *entry = findInHashtable(&database->hashtable, key, hashString(hashInit(), key)); \
		return entry ? entry->value : (defaultValue); \
	}

DATABASE_ACCESSORS(struct vector, vectorEntry, setDatabaseVector, getDatabaseVector, ((struct vector) { 0.0f, 0.0f, 0.0f }))
DATABASE_ACCESSORS(float,         floatEntry,  setDatabaseFloat,  getDatabaseFloat,  0.0f)
DATABASE_ACCESSORS(char*,         stringEntry, ignoreAccessor,    getDatabaseString, NULL)
DATABASE_ACCESSORS(int,           intEntry,    setDatabaseInt,    getDatabaseInt,    0)

void setDatabaseString(struct constantsDatabase *database, const char *key, const char *value) {
	struct stringEntry *entry = findInHashtable(&database->hashtable, key, hashString(hashInit(), key));
	char *valueCopy = stringCopy(value);
	if (entry) {
		// We need to free the existing string stored in the entry
		free(entry->value);
		entry->toFree = entry->value = valueCopy;
	} else {
		forceInsertInHashtable(
			&database->hashtable,
			&(struct stringEntry) { .key = stringCopy(key), .toFree = valueCopy, .value = valueCopy },
			sizeof(struct stringEntry),
			hashString(hashInit(), key));
	}
}

void setDatabaseTag(struct constantsDatabase *database, const char *key) {
	replaceInHashtable(&database->hashtable, &key, sizeof(const char*), hashString(hashInit(), key));
}

struct databaseEntry {
	char *key;
	void* toFree;
};

void freeConstantsDatabase(struct constantsDatabase *database) {
	for (size_t i = 0, n = database->hashtable.bucketCount; i < n; ++i) {
		struct bucket *bucket = database->hashtable.buckets[i];
		while (bucket) {
			struct databaseEntry *entry = (struct databaseEntry*)&bucket->data;
			free(entry->key);
			free(entry->toFree);
			bucket = bucket->next;
		}
	}
	freeHashtable(&database->hashtable);
}

static void printTableUsage(struct hashtable *hashtable) {
	for (size_t i = 0, n = hashtable->bucketCount; i < n; ++i)
		printf("[%s]", hashtable->buckets[i] ? "x" : " ");
	printf("\n");
}

void testTable() {
	struct constantsDatabase *database = newConstantsDatabase();

	printTableUsage(&database->hashtable);

	setDatabaseFloat(database, "Foo", 0.1f);
	setDatabaseFloat(database, "Bar", 0.2f);
	setDatabaseFloat(database, "Baz", 0.3f);

	printTableUsage(&database->hashtable);

	logr(debug, "Value at %s is %f\n", "Foo", getDatabaseFloat(database, "Foo"));
	logr(debug, "Value at %s is %f\n", "Bar", getDatabaseFloat(database, "Bar"));
	logr(debug, "Value at %s is %f\n", "Baz", getDatabaseFloat(database, "Baz"));

	setDatabaseFloat(database, "Baz", 0.4f);
	logr(debug, "Value at %s is %f\n", "Baz", getDatabaseFloat(database, "Baz"));
	printTableUsage(&database->hashtable);
	for (int i = 0; i < 10000000; ++i) {
		setDatabaseFloat(database, "Yeet", 3.3333);
	}
	printTableUsage(&database->hashtable);
	logr(debug, "Testing overfill\n");
	char buf[5];
	for (size_t i = 0, n = database->hashtable.bucketCount; i < n; ++i) {
		sprintf(buf, "%zu", i);
		setDatabaseFloat(database, buf, (float)i);
		printTableUsage(&database->hashtable);
	}

	freeConstantsDatabase(database);
}
