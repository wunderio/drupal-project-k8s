--TEST--
thrown exceptions are recorded as span events (incl. exotic message access)
--INI--
otelsilta.cli_enabled=1
otelsilta.sample_rate=1
otelsilta.otel_service_name=testsvc
otelsilta.otel_exporter_otlp_endpoint=http://127.0.0.1:1
--FILE--
<?php
try { throw new RuntimeException('boom'); } catch (RuntimeException $e) {}

$events = 0;
foreach (otelsilta_test_spans() as $s) { $events += $s['event_count']; }
var_dump($events >= 1);
echo "alive\n";
?>
--EXPECT--
bool(true)
alive
