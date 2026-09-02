--TEST--
curl header merge preserves app headers, dedupes traceparent, appends ours
--INI--
otelsilta.cli_enabled=1
otelsilta.sample_rate=1
otelsilta.otel_service_name=testsvc
otelsilta.otel_exporter_otlp_endpoint=http://127.0.0.1:1
--FILE--
<?php
var_dump(otelsilta_test_merge_headers(null, 'traceparent: 00-aa-bb-01'));
var_dump(otelsilta_test_merge_headers(
    ['Authorization: Bearer x', 'Content-Type: application/json'],
    'traceparent: 00-aa-bb-01'));
var_dump(otelsilta_test_merge_headers(
    ['TraceParent: 00-old-old-00', 'X-Foo: bar', 42],
    'traceparent: 00-aa-bb-01'));
?>
--EXPECT--
array(1) {
  [0]=>
  string(24) "traceparent: 00-aa-bb-01"
}
array(3) {
  [0]=>
  string(23) "Authorization: Bearer x"
  [1]=>
  string(30) "Content-Type: application/json"
  [2]=>
  string(24) "traceparent: 00-aa-bb-01"
}
array(2) {
  [0]=>
  string(10) "X-Foo: bar"
  [1]=>
  string(24) "traceparent: 00-aa-bb-01"
}
