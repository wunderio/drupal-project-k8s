--TEST--
observer-based function spans nest correctly under their callers
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
function ots_inner() { usleep(2000); return 1; }
function ots_outer() { return ots_inner(); }
ots_outer();

$spans = otelsilta_test_spans();
$by_name = [];
foreach ($spans as $s) { $by_name[$s['name']] = $s; }

var_dump(isset($by_name['ots_outer'], $by_name['ots_inner']));
$root = $spans[0]; // root span is appended first
var_dump($by_name['ots_outer']['parent_span_id'] === $root['span_id']);
var_dump($by_name['ots_inner']['parent_span_id'] === $by_name['ots_outer']['span_id']);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
