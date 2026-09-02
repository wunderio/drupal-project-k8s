--TEST--
fast function spans are dropped; a kept child forces its ancestors kept
--INI--
otelsilta.cli_enabled=1
otelsilta.sample_rate=1
otelsilta.otel_service_name=testsvc
otelsilta.otel_exporter_otlp_endpoint=http://127.0.0.1:1
otelsilta.features.functions=1
otelsilta.min_span_duration_ms=50
otelsilta.max_span_depth=10
--FILE--
<?php
function ots_fast() { return 1; }
function ots_with_manual_child() {
    $h = otelsilta_span_start('manual');
    otelsilta_span_finish($h);
    return 1;
}
ots_fast();
ots_with_manual_child();

$names = array_column(otelsilta_test_spans(), 'name');
var_dump(in_array('ots_fast', $names, true));              // dropped (fast)
var_dump(in_array('manual', $names, true));                // kept (explicit)
var_dump(in_array('ots_with_manual_child', $names, true)); // force-kept ancestor
?>
--EXPECT--
bool(false)
bool(true)
bool(true)
