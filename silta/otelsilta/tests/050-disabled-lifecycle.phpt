--TEST--
enabled=0: API is inert, runtime ini_set of system INIs is refused, no shutdown crash
--INI--
otelsilta.enabled=0
otelsilta.cli_enabled=1
otelsilta.sample_rate=1
otelsilta.otel_service_name=testsvc
otelsilta.otel_exporter_otlp_endpoint=http://127.0.0.1:1
--FILE--
<?php
var_dump(otelsilta_span_start('x'));
var_dump(otelsilta_current_trace_id());
var_dump(@ini_set('otelsilta.enabled', '1'));          // SYSTEM: not settable
var_dump(@ini_set('otelsilta.features.db', '1'));      // SYSTEM: not settable
echo "alive\n";
?>
--EXPECT--
bool(false)
bool(false)
bool(false)
bool(false)
alive
