--TEST--
max_attributes_per_span above the compiled cap must clamp to 32, not overflow
--INI--
otelsilta.cli_enabled=1
otelsilta.sample_rate=1
otelsilta.otel_service_name=testsvc
otelsilta.otel_exporter_otlp_endpoint=http://127.0.0.1:1
otelsilta.max_attributes_per_span=1000
--FILE--
<?php
$h = otelsilta_span_start('attr-test');
var_dump($h !== false);
for ($i = 0; $i < 100; $i++) {
    otelsilta_span_set_attribute($h, "k$i", "v$i");
}
var_dump(otelsilta_test_span_attribute_count($h));
otelsilta_span_finish($h);
echo "alive\n";
?>
--EXPECT--
bool(true)
int(32)
alive
