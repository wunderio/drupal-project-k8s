/*
 * aggregator.h — Span aggregation for repeated DB / cache operations.
 *
 * Instead of emitting one span per query (Drupal fires 50-200 identical
 * DB queries per request), repeated operations are bucketed by a grouping
 * key and flushed as a single aggregated span at request shutdown.
 *
 * Grouping key:
 *   DB:    db.operation + normalized table name
 *   Cache: cache.operation + system (redis / memcached)
 *
 * Each bucket tracks count, total duration, and a sample statement.
 * At flush time, one CLIENT span per bucket is appended to the trace.
 */

#ifndef OTELSILTA_AGGREGATOR_H
#define OTELSILTA_AGGREGATOR_H

#include "php_otelsilta.h"

/* Maximum length for a grouping key (e.g. "SELECT cache_default") */
#define OTELSILTA_AGG_KEY_LEN  256

/* Maximum length for a sample statement stored in a bucket */
#define OTELSILTA_AGG_SAMPLE_LEN  512

/* Aggregation bucket — one per unique grouping key */
typedef struct {
    char        key[OTELSILTA_AGG_KEY_LEN];      /* grouping key */
    char        db_system[32];                    /* e.g. "mysql", "redis" */
    char        db_operation[32];                 /* e.g. "SELECT", "get" */
    char        table_or_system[128];             /* e.g. "cache_default" */
    char        sample_statement[OTELSILTA_AGG_SAMPLE_LEN]; /* first captured stmt */
    uint64_t    count;                            /* number of operations */
    uint64_t    total_duration_ns;                /* sum of durations */
    uint64_t    first_start_ns;                   /* earliest start time */
    uint64_t    last_end_ns;                      /* latest end time */
    char        parent_span_id[17];               /* parent span at time of first op */
    otelsilta_span_kind_t kind;                   /* CLIENT for DB/cache */
} otelsilta_agg_bucket_t;

/* Initialise the aggregation buffer.  Called from RINIT. */
void otelsilta_aggregator_init(void);

/* Add an operation to the aggregation buffer.
 *
 * key:          grouping key (e.g. "SELECT cache_default")
 * db_system:    e.g. "mysql", "redis", "memcached"
 * db_operation: e.g. "SELECT", "get"
 * table_or_sys: table name or cache system name
 * statement:    sample SQL/command (sanitised)
 * duration_ns:  how long this individual operation took
 * parent_id:    parent span ID for the resulting aggregated span
 */
void otelsilta_aggregator_add(const char *key,
                               const char *db_system,
                               const char *db_operation,
                               const char *table_or_sys,
                               const char *statement,
                               uint64_t duration_ns,
                               const char *parent_id);

/* Flush all aggregation buckets into real spans and append them to
 * the all_spans linked list.  Called from RSHUTDOWN before export. */
void otelsilta_aggregator_flush(void);

/* Destroy the aggregation buffer (free the HashTable).
 * Called from RSHUTDOWN after flush. */
void otelsilta_aggregator_destroy(void);

#endif /* OTELSILTA_AGGREGATOR_H */
