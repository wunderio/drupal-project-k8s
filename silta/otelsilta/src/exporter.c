#include "exporter.h"
#include "span.h"
#include "php_otelsilta.h"

#include "zend_smart_str.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>

/* ---- JSON helpers ---- */

/* Escape a string for JSON output into buf (max buf_size). */
static void json_escape(const char *src, char *buf, size_t buf_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 6 < buf_size; i++) {
        unsigned char c = (unsigned char)src[i];
        if      (c == '"')  { buf[j++] = '\\'; buf[j++] = '"'; }
        else if (c == '\\') { buf[j++] = '\\'; buf[j++] = '\\'; }
        else if (c == '\n') { buf[j++] = '\\'; buf[j++] = 'n'; }
        else if (c == '\r') { buf[j++] = '\\'; buf[j++] = 'r'; }
        else if (c == '\t') { buf[j++] = '\\'; buf[j++] = 't'; }
        else if (c < 0x20) {
            j += snprintf(buf + j, buf_size - j, "\\u%04x", c);
        } else {
            buf[j++] = (char)c;
        }
    }
    buf[j] = '\0';
}

static void append_attribute(smart_str *out, const otelsilta_attribute_t *a,
                              int is_first) {
    char esc_key[256];
    json_escape(a->key, esc_key, sizeof(esc_key));

    if (!is_first) smart_str_appends(out, ",");
    smart_str_appends(out, "{\"key\":\"");
    smart_str_appends(out, esc_key);
    smart_str_appends(out, "\",\"value\":{");

    switch (a->type) {
    case ATTR_TYPE_STRING: {
        char esc_val[OTELSILTA_MAX_STR_LEN * 2];
        json_escape(a->value.str_val, esc_val, sizeof(esc_val));
        smart_str_appends(out, "\"stringValue\":\"");
        smart_str_appends(out, esc_val);
        smart_str_appends(out, "\"");
        break;
    }
    case ATTR_TYPE_INT: {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%lld", (long long)a->value.int_val);
        smart_str_appends(out, "\"intValue\":\"");
        smart_str_appends(out, tmp);
        smart_str_appends(out, "\"");
        break;
    }
    case ATTR_TYPE_DOUBLE: {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%g", a->value.dbl_val);
        smart_str_appends(out, "\"doubleValue\":");
        smart_str_appends(out, tmp);
        break;
    }
    case ATTR_TYPE_BOOL:
        smart_str_appends(out, "\"boolValue\":");
        smart_str_appends(out, a->value.bool_val ? "true" : "false");
        break;
    }
    smart_str_appends(out, "}}");
}

static void append_span_json(smart_str *out, const otelsilta_span_t *s,
                              int is_first) {
    if (!is_first) smart_str_appends(out, ",");

    char tmp[128];

    smart_str_appends(out, "{");
    smart_str_appends(out, "\"traceId\":\"");
    smart_str_appends(out, s->trace_id);
    smart_str_appends(out, "\",\"spanId\":\"");
    smart_str_appends(out, s->span_id);
    smart_str_appends(out, "\"");

    if (s->parent_span_id[0] != '\0') {
        smart_str_appends(out, ",\"parentSpanId\":\"");
        smart_str_appends(out, s->parent_span_id);
        smart_str_appends(out, "\"");
    }

    char esc_name[512];
    json_escape(s->name, esc_name, sizeof(esc_name));
    smart_str_appends(out, ",\"name\":\"");
    smart_str_appends(out, esc_name);
    smart_str_appends(out, "\"");

    snprintf(tmp, sizeof(tmp), "%d", (int)s->kind);
    smart_str_appends(out, ",\"kind\":");
    smart_str_appends(out, tmp);

    snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)s->start_time_ns);
    smart_str_appends(out, ",\"startTimeUnixNano\":\"");
    smart_str_appends(out, tmp);
    smart_str_appends(out, "\"");

    uint64_t end_ns = s->end_time_ns ? s->end_time_ns : otelsilta_time_ns();
    snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)end_ns);
    smart_str_appends(out, ",\"endTimeUnixNano\":\"");
    smart_str_appends(out, tmp);
    smart_str_appends(out, "\"");

    /* Attributes */
    smart_str_appends(out, ",\"attributes\":[");
    for (int i = 0; i < s->attribute_count; i++) {
        append_attribute(out, &s->attributes[i], i == 0);
    }
    smart_str_appends(out, "]");

    /* Status */
    smart_str_appends(out, ",\"status\":{\"code\":");
    snprintf(tmp, sizeof(tmp), "%d", (int)s->status);
    smart_str_appends(out, tmp);
    if (s->status_message[0] != '\0') {
        char esc_msg[1024];
        json_escape(s->status_message, esc_msg, sizeof(esc_msg));
        smart_str_appends(out, ",\"message\":\"");
        smart_str_appends(out, esc_msg);
        smart_str_appends(out, "\"");
    }
    smart_str_appends(out, "}");

    smart_str_appends(out, "}");
}

/* Build full OTLP JSON payload */
static smart_str build_otlp_json(void) {
    smart_str out = {0};

    char esc_svc[256];
    const char *svc = otelsilta_effective_service_name();
    if (!svc || svc[0] == '\0') svc = "unknown";
    json_escape(svc, esc_svc, sizeof(esc_svc));

    smart_str_appends(&out,
        "{\"resourceSpans\":[{\"resource\":{\"attributes\":["
        "{\"key\":\"service.name\",\"value\":{\"stringValue\":\"");
    smart_str_appends(&out, esc_svc);
    smart_str_appends(&out, "\"}}");

    /* Conditionally emit service.namespace (only if non-empty) */
    {
        const char *ns = otelsilta_effective_service_namespace();
        if (ns && ns[0] != '\0') {
            char esc_ns[256];
            json_escape(ns, esc_ns, sizeof(esc_ns));
            smart_str_appends(&out, ",{\"key\":\"service.namespace\",\"value\":{\"stringValue\":\"");
            smart_str_appends(&out, esc_ns);
            smart_str_appends(&out, "\"}}");
        }
    }

    /* Conditionally emit deployment.environment (only if non-empty) */
    {
        const char *dep = otelsilta_effective_deployment_environment();
        if (dep && dep[0] != '\0') {
            char esc_dep[256];
            json_escape(dep, esc_dep, sizeof(esc_dep));
            smart_str_appends(&out, ",{\"key\":\"deployment.environment\",\"value\":{\"stringValue\":\"");
            smart_str_appends(&out, esc_dep);
            smart_str_appends(&out, "\"}}");
        }
    }

    /* Emit extra resource attributes from otel_resource_attributes
     * (or OTEL_RESOURCE_ATTRIBUTES env var).  Format: key=value,key=value
     * All values are emitted as stringValue. */
    const char *rattr = otelsilta_effective_resource_attributes();
    if (rattr && rattr[0] != '\0') {
        const char *p = rattr;
        while (*p) {
            /* skip commas and whitespace between entries */
            while (*p == ',' || *p == ' ') p++;
            if (*p == '\0') break;

            /* find '=' */
            const char *eq = strchr(p, '=');
            if (!eq) break;  /* malformed, stop */

            size_t key_len = (size_t)(eq - p);
            const char *val_start = eq + 1;

            /* find end of value (next comma or end of string) */
            const char *val_end = val_start;
            while (*val_end && *val_end != ',') val_end++;
            size_t val_len = (size_t)(val_end - val_start);

            if (key_len > 0) {
                char esc_key[256], esc_val[512];
                /* copy key/val to temporary NUL-terminated buffers */
                size_t kl = key_len < sizeof(esc_key) - 1 ? key_len : sizeof(esc_key) - 2;
                size_t vl = val_len < sizeof(esc_val) - 1 ? val_len : sizeof(esc_val) - 2;
                char tmp_key[256], tmp_val[512];
                memcpy(tmp_key, p, kl); tmp_key[kl] = '\0';
                memcpy(tmp_val, val_start, vl); tmp_val[vl] = '\0';

                json_escape(tmp_key, esc_key, sizeof(esc_key));
                json_escape(tmp_val, esc_val, sizeof(esc_val));

                smart_str_appends(&out, ",{\"key\":\"");
                smart_str_appends(&out, esc_key);
                smart_str_appends(&out, "\",\"value\":{\"stringValue\":\"");
                smart_str_appends(&out, esc_val);
                smart_str_appends(&out, "\"}}");
            }

            p = val_end;
        }
    }

    smart_str_appends(&out,
        ","
        "{\"key\":\"telemetry.sdk.name\",\"value\":{\"stringValue\":\"otelsilta\"}},"
        "{\"key\":\"telemetry.sdk.version\",\"value\":{\"stringValue\":\"" PHP_OTELSILTA_VERSION "\"}}");
    smart_str_appends(&out, "]}");   /* /resource.attributes, /resource */
    smart_str_appends(&out, ",");    /* comma before scopeSpans */

    /* scopeSpans */
    smart_str_appends(&out,
        "\"scopeSpans\":[{\"scope\":"
        "{\"name\":\"otelsilta\",\"version\":\"" PHP_OTELSILTA_VERSION "\"},"
        "\"spans\":[");

    otelsilta_span_t *s   = OTELSILTA_G(all_spans);
    int               idx = 0;
    while (s) {
        append_span_json(&out, s, idx == 0);
        idx++;
        s = s->next;
    }

    smart_str_appends(&out, "]}]}]}");  /* /spans, /scopeSpans, /resourceSpans */
    smart_str_0(&out);
    return out;
}

/* ---- HTTP POST via raw POSIX sockets ----
 *
 * PHP streams are unreliable during RSHUTDOWN — the stream layer may be
 * partially torn down, and even when the open "succeeds" the request body
 * may never reach the wire.  Using raw sockets avoids the PHP layer
 * entirely and gives us full control over the blocking send.
 *
 * We only support http:// (not https://) since the OTLP collector is
 * typically on an internal network.  This is a fire-and-forget POST:
 * we send the request and read just enough of the response to confirm
 * delivery, then close.  Timeout is 2 seconds.
 */

/* Parse "http://host:port/path" into components.  Returns 0 on success. */
static int parse_http_url(const char *url,
                          char *host, size_t host_sz,
                          char *port, size_t port_sz,
                          char *path, size_t path_sz) {
    if (strncmp(url, "http://", 7) != 0) return -1;
    const char *hp = url + 7;
    const char *slash = strchr(hp, '/');
    const char *colon = NULL;

    /* Find colon, but only before the first slash */
    for (const char *p = hp; p < (slash ? slash : hp + strlen(hp)); p++) {
        if (*p == ':') { colon = p; break; }
    }

    if (colon) {
        size_t hlen = (size_t)(colon - hp);
        if (hlen >= host_sz) return -1;
        memcpy(host, hp, hlen);
        host[hlen] = '\0';

        const char *port_start = colon + 1;
        const char *port_end = slash ? slash : port_start + strlen(port_start);
        size_t plen = (size_t)(port_end - port_start);
        if (plen >= port_sz) return -1;
        memcpy(port, port_start, plen);
        port[plen] = '\0';
    } else {
        const char *host_end = slash ? slash : hp + strlen(hp);
        size_t hlen = (size_t)(host_end - hp);
        if (hlen >= host_sz) return -1;
        memcpy(host, hp, hlen);
        host[hlen] = '\0';
        strncpy(port, "80", port_sz - 1);
        port[port_sz - 1] = '\0';
    }

    if (slash) {
        strncpy(path, slash, path_sz - 1);
        path[path_sz - 1] = '\0';
    } else {
        strncpy(path, "/", path_sz - 1);
        path[path_sz - 1] = '\0';
    }
    return 0;
}

/* Write all bytes with poll-based timeout.  Returns 0 on success. */
static int send_all(int fd, const char *buf, size_t len, int timeout_ms) {
    size_t sent = 0;
    while (sent < len) {
        struct pollfd pfd = { .fd = fd, .events = POLLOUT };
        int pr = poll(&pfd, 1, timeout_ms);
        if (pr <= 0) return -1;  /* timeout or error */

        ssize_t n = write(fd, buf + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static void http_post_otlp(const char *url, const char *body, size_t body_len) {
    char host[256], port[8], path[1024];

    if (parse_http_url(url, host, sizeof(host), port, sizeof(port),
                       path, sizeof(path)) != 0) {
        otelsilta_debug_log("otelsilta: invalid endpoint URL: %s", url);
        return;
    }

    /* DNS resolve */
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int gai = getaddrinfo(host, port, &hints, &res);
    if (gai != 0 || !res) {
        otelsilta_debug_log("otelsilta: DNS resolve failed for %s: %s",
                             host, gai_strerror(gai));
        return;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return;
    }

    /* Non-blocking connect with 2s timeout */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int cr = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (cr < 0 && errno != EINPROGRESS) {
        close(fd);
        otelsilta_debug_log("otelsilta: connect failed to %s:%s", host, port);
        return;
    }

    if (cr < 0) {
        /* Wait for connect to complete */
        struct pollfd pfd = { .fd = fd, .events = POLLOUT };
        int pr = poll(&pfd, 1, 2000);
        if (pr <= 0) {
            close(fd);
            otelsilta_debug_log("otelsilta: connect timeout to %s:%s",
                                 host, port);
            return;
        }
        int so_err = 0;
        socklen_t so_len = sizeof(so_err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_err, &so_len);
        if (so_err) {
            close(fd);
            otelsilta_debug_log("otelsilta: connect error %d to %s:%s",
                                 so_err, host, port);
            return;
        }
    }

    /* Switch back to blocking for the send/recv (with poll timeouts) */
    fcntl(fd, F_SETFL, flags);

    /* Build HTTP request header */
    char hdr[2048];
    int hdr_len = snprintf(hdr, sizeof(hdr),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, port, body_len);

    if (hdr_len < 0 || (size_t)hdr_len >= sizeof(hdr)) {
        close(fd);
        return;
    }

    /* Send header + body */
    if (send_all(fd, hdr, (size_t)hdr_len, 2000) != 0 ||
        send_all(fd, body, body_len, 2000) != 0) {
        close(fd);
        otelsilta_debug_log("otelsilta: send failed to %s", url);
        return;
    }

    /* Read response status line (fire-and-forget: we just check the
     * status code for debug logging, don't need the body). */
    char resp[256];
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int pr = poll(&pfd, 1, 2000);
    if (pr > 0) {
        ssize_t n = read(fd, resp, sizeof(resp) - 1);
        if (n > 0) {
            resp[n] = '\0';
            /* Parse "HTTP/1.1 200 ..." */
            int status = 0;
            if (sscanf(resp, "HTTP/%*d.%*d %d", &status) == 1) {
                if (status >= 200 && status < 300) {
                    otelsilta_debug_log("otelsilta: exported %d spans to %s (HTTP %d)",
                                         OTELSILTA_G(span_count), url, status);
                } else {
                    otelsilta_debug_log("otelsilta: export to %s returned HTTP %d",
                                         url, status);
                }
            }
        }
    }

    close(fd);
}

/* ---- public ---- */

void otelsilta_export_trace(void) {
    if (!OTELSILTA_G(request_active)) return;
    if (OTELSILTA_G(span_count) == 0) return;

    const char *endpoint = otelsilta_effective_endpoint();
    if (!endpoint || endpoint[0] == '\0') {
        otelsilta_debug_log("otelsilta: no otel_exporter_otlp_endpoint set, skipping export");
        return;
    }

    /* Build the full OTLP traces URL.  Per the OTLP/HTTP spec,
     * OTEL_EXPORTER_OTLP_ENDPOINT is the *base* URL and the signal
     * path (/v1/traces) must be appended.  If the endpoint already
     * ends with /v1/traces we use it as-is. */
    char url[2048];
    size_t ep_len = strlen(endpoint);
    const char *suffix = "/v1/traces";
    size_t suffix_len = strlen(suffix);

    if (ep_len >= suffix_len &&
        strcmp(endpoint + ep_len - suffix_len, suffix) == 0) {
        /* Already has /v1/traces */
        snprintf(url, sizeof(url), "%s", endpoint);
    } else {
        /* Strip trailing slash if present, then append */
        if (ep_len > 0 && endpoint[ep_len - 1] == '/') {
            snprintf(url, sizeof(url), "%.*s%s", (int)(ep_len - 1), endpoint, suffix);
        } else {
            snprintf(url, sizeof(url), "%s%s", endpoint, suffix);
        }
    }

    smart_str json = build_otlp_json();

    if (OTELSILTA_G(debug_mode)) {
        otelsilta_debug_log("OTLP export: %zu bytes to %s", ZSTR_LEN(json.s), url);
    }

    http_post_otlp(url, ZSTR_VAL(json.s), ZSTR_LEN(json.s));
    smart_str_free(&json);
}
