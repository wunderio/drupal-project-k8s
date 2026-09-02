--TEST--
sample_rate=0 never samples; rate=1 always samples (CSPRNG draw)
--INI--
otelsilta.cli_enabled=1
otelsilta.sample_rate=0
otelsilta.otel_service_name=testsvc
otelsilta.otel_exporter_otlp_endpoint=http://127.0.0.1:1
--FILE--
<?php
var_dump(otelsilta_current_trace_id());
?>
--EXPECT--
bool(false)
