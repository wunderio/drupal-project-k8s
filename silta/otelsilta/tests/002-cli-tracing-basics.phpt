--TEST--
CLI test mode activates tracing: trace id + root span exist
--INI--
otelsilta.cli_enabled=1
otelsilta.sample_rate=1
otelsilta.otel_service_name=testsvc
otelsilta.otel_exporter_otlp_endpoint=http://127.0.0.1:1
--FILE--
<?php
$tid = otelsilta_current_trace_id();
var_dump(is_string($tid) && strlen($tid) === 32 && ctype_xdigit($tid));
var_dump(otelsilta_test_span_count() >= 1);
?>
--EXPECT--
bool(true)
bool(true)
