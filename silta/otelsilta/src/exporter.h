#ifndef OTELSILTA_EXPORTER_H
#define OTELSILTA_EXPORTER_H

/* Export all collected spans for the current request via OTLP HTTP/JSON. */
void otelsilta_export_trace(void);

#endif /* OTELSILTA_EXPORTER_H */
