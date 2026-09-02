--TEST--
span ids are well-formed and unique
--INI--
otelsilta.cli_enabled=1
otelsilta.sample_rate=1
otelsilta.otel_service_name=testsvc
otelsilta.otel_exporter_otlp_endpoint=http://127.0.0.1:1
--FILE--
<?php
for ($i = 0; $i < 20; $i++) {
    otelsilta_span_finish(otelsilta_span_start("s$i"));
}
$ids = array_column(otelsilta_test_spans(), 'span_id');
var_dump(count($ids) === count(array_unique($ids)));
foreach ($ids as $id) {
    if (strlen($id) !== 16 || !ctype_xdigit($id)) { echo "bad id: $id\n"; }
}
echo "done\n";
?>
--EXPECT--
bool(true)
done
