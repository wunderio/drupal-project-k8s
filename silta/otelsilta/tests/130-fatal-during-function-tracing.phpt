--TEST--
fatal error while function spans are in flight does not crash shutdown
--INI--
otelsilta.cli_enabled=1
otelsilta.sample_rate=1
otelsilta.otel_service_name=testsvc
otelsilta.otel_exporter_otlp_endpoint=http://127.0.0.1:1
otelsilta.features.functions=1
otelsilta.min_span_duration_ms=0
otelsilta.max_span_depth=10
--FILE--
<?php
function ots_boom() { this_function_does_not_exist(); }
function ots_wrapper() { return ots_boom(); }
ots_wrapper();
echo "not reached\n";
?>
--EXPECTF--
Fatal error: Uncaught Error: Call to undefined function this_function_does_not_exist()%A
