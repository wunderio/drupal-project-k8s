/*
 * aggregator.c — Span aggregation buffer for repeated DB / cache operations.
 *
 * Drupal fires 50–200 identical DB queries per request.  Instead of one
 * span per query, operations are grouped by key (db.operation + table)
 * and flushed as a single aggregated span at RSHUTDOWN.
 *
 * The aggregation HashTable is stored in per-request module globals
 * (OTELSILTA_G(agg_buckets)).  Each entry is a heap-allocated
 * otelsilta_agg_bucket_t keyed by a string like "SELECT cache_default".
 */

#include "aggregator.h"
#include "span.h"
#include "tracer.h"
#include "php_otelsilta.h"

#include <string.h>
#include <stdio.h>

/* ---- HashTable destructor for aggregation buckets ---- */

static void agg_bucket_dtor(zval *zv) {
    otelsilta_agg_bucket_t *bucket = (otelsilta_agg_bucket_t *)Z_PTR_P(zv);
    if (bucket) {
        efree(bucket);
    }
}

/* ---- Public API ---- */

void otelsilta_aggregator_init(void) {
    OTELSILTA_G(agg_buckets) = (HashTable *)emalloc(sizeof(HashTable));
    zend_hash_init(OTELSILTA_G(agg_buckets), 32, NULL, agg_bucket_dtor, 0);
}

void otelsilta_aggregator_add(const char *key,
                               const char *db_system,
                               const char *db_operation,
                               const char *table_or_sys,
                               const char *statement,
                               uint64_t duration_ns,
                               const char *parent_id) {
    if (!OTELSILTA_G(agg_buckets)) return;

    uint64_t now = otelsilta_time_ns();

    otelsilta_agg_bucket_t *bucket = (otelsilta_agg_bucket_t *)
        zend_hash_str_find_ptr(OTELSILTA_G(agg_buckets), key, strlen(key));

    if (bucket) {
        /* Existing bucket: accumulate */
        bucket->count++;
        bucket->total_duration_ns += duration_ns;
        if (now > bucket->last_end_ns) {
            bucket->last_end_ns = now;
        }
    } else {
        /* New bucket */
        bucket = (otelsilta_agg_bucket_t *)emalloc(sizeof(*bucket));
        memset(bucket, 0, sizeof(*bucket));

        strncpy(bucket->key, key, sizeof(bucket->key) - 1);
        strncpy(bucket->db_system, db_system ? db_system : "other",
                sizeof(bucket->db_system) - 1);
        strncpy(bucket->db_operation, db_operation ? db_operation : "QUERY",
                sizeof(bucket->db_operation) - 1);
        strncpy(bucket->table_or_system,
                table_or_sys ? table_or_sys : "",
                sizeof(bucket->table_or_system) - 1);
        if (statement && statement[0]) {
            strncpy(bucket->sample_statement, statement,
                    sizeof(bucket->sample_statement) - 1);
        }
        if (parent_id) {
            strncpy(bucket->parent_span_id, parent_id,
                    sizeof(bucket->parent_span_id) - 1);
        }

        bucket->count              = 1;
        bucket->total_duration_ns  = duration_ns;
        bucket->first_start_ns     = now - duration_ns;
        bucket->last_end_ns        = now;
        bucket->kind               = SPAN_KIND_CLIENT;

        zval zv;
        ZVAL_PTR(&zv, bucket);
        zend_hash_str_update(OTELSILTA_G(agg_buckets), key, strlen(key), &zv);
    }
}

void otelsilta_aggregator_flush(void) {
    if (!OTELSILTA_G(agg_buckets)) return;
    if (!OTELSILTA_G(request_active)) return;

    zend_string *zkey;
    zval *zv;

    ZEND_HASH_FOREACH_STR_KEY_VAL(OTELSILTA_G(agg_buckets), zkey, zv) {
        (void)zkey;
        otelsilta_agg_bucket_t *bucket = (otelsilta_agg_bucket_t *)Z_PTR_P(zv);
        if (!bucket || bucket->count == 0) continue;

        /* Build span name: e.g. "mysql SELECT (x80)" */
        char span_name[320];
        if (bucket->table_or_system[0]) {
            snprintf(span_name, sizeof(span_name), "%s %s %s (x%llu)",
                     bucket->db_system,
                     bucket->db_operation,
                     bucket->table_or_system,
                     (unsigned long long)bucket->count);
        } else {
            snprintf(span_name, sizeof(span_name), "%s %s (x%llu)",
                     bucket->db_system,
                     bucket->db_operation,
                     (unsigned long long)bucket->count);
        }

        /* Create a real span — bypass depth/threshold checks since this
         * is already an aggregate.  Use otelsilta_span_create directly
         * to avoid the tracer's span stack management. */
        otelsilta_span_t *span = otelsilta_span_create(
            span_name,
            bucket->kind,
            OTELSILTA_G(trace_id),
            bucket->parent_span_id);

        if (!span) continue;

        /* Timing: use the cumulative DB/cache time as the span duration.
         * NOT the wall-clock window (last_end - first_start), which would
         * span the entire request and be hugely misleading in Grafana.
         *
         * Place the span right before the export point (last_end) so it
         * doesn't overlap other spans visually. */
        span->start_time_ns = bucket->last_end_ns - bucket->total_duration_ns;
        span->end_time_ns   = bucket->last_end_ns;
        span->is_finished   = 1;

        /* Attributes */
        otelsilta_span_set_str(span, "db.system", bucket->db_system);
        otelsilta_span_set_str(span, "db.operation", bucket->db_operation);

        if (bucket->table_or_system[0]) {
            /* For DB: this is the table name; for cache: the system */
            otelsilta_span_set_str(span, "db.sql.table",
                                    bucket->table_or_system);
        }

        if (bucket->sample_statement[0]) {
            otelsilta_span_set_str(span, "db.statement",
                                    bucket->sample_statement);
        }

        /* Aggregation-specific attributes */
        otelsilta_span_set_int(span, "db.statement.count",
                                (zend_long)bucket->count);

        double total_ms = (double)bucket->total_duration_ns / 1e6;
        otelsilta_span_set_dbl(span, "db.total_duration_ms", total_ms);

        otelsilta_span_set_status(span, SPAN_STATUS_OK, NULL);

        /* Append to the all_spans linked list (same as tracer does) */
        if (OTELSILTA_G(last_span)) {
            OTELSILTA_G(last_span)->next = span;
        } else {
            OTELSILTA_G(all_spans) = span;
        }
        OTELSILTA_G(last_span) = span;
        OTELSILTA_G(span_count)++;

    } ZEND_HASH_FOREACH_END();
}

void otelsilta_aggregator_destroy(void) {
    if (OTELSILTA_G(agg_buckets)) {
        zend_hash_destroy(OTELSILTA_G(agg_buckets));
        efree(OTELSILTA_G(agg_buckets));
        OTELSILTA_G(agg_buckets) = NULL;
    }
}
