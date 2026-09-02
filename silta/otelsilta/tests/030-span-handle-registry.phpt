--TEST--
span handles are registry IDs; bogus/double-finished handles are safe no-ops
--INI--
otelsilta.cli_enabled=1
otelsilta.sample_rate=1
otelsilta.otel_service_name=testsvc
otelsilta.otel_exporter_otlp_endpoint=http://127.0.0.1:1
--FILE--
<?php
$h1 = otelsilta_span_start('one');
$h2 = otelsilta_span_start('two');
var_dump($h1 === 1, $h2 === 2);

otelsilta_span_set_attribute($h1, 'k', 'v');
var_dump(otelsilta_test_span_attribute_count($h1) === 1);

// Arbitrary integers must not crash or alias a live span.
otelsilta_span_finish(0);
otelsilta_span_finish(-1);
otelsilta_span_finish(999999);
otelsilta_span_finish(PHP_INT_MAX);
otelsilta_span_set_attribute(424242, 'k', 'v');
var_dump(otelsilta_test_span_attribute_count(999999) === -1);

otelsilta_span_finish($h2);
otelsilta_span_finish($h2); // double finish: idempotent
otelsilta_span_finish($h1);
echo "alive\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
alive
