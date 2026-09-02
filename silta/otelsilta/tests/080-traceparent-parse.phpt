--TEST--
traceparent parsing: versions, zero-ids, malformed inputs
--INI--
otelsilta.cli_enabled=1
--FILE--
<?php
$ok = '00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01';
var_dump(otelsilta_test_parse_traceparent($ok));
// future version accepted (parsed per 00 layout)
var_dump(otelsilta_test_parse_traceparent('01-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-00') !== false);
// version ff invalid
var_dump(otelsilta_test_parse_traceparent('ff-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01'));
// all-zero trace id invalid
var_dump(otelsilta_test_parse_traceparent('00-00000000000000000000000000000000-00f067aa0ba902b7-01'));
// all-zero parent id invalid
var_dump(otelsilta_test_parse_traceparent('00-4bf92f3577b34da6a3ce929d0e0e4736-0000000000000000-01'));
// malformed
var_dump(otelsilta_test_parse_traceparent('00-short-x-01'));
var_dump(otelsilta_test_parse_traceparent('zz-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01'));
?>
--EXPECT--
array(3) {
  ["trace_id"]=>
  string(32) "4bf92f3577b34da6a3ce929d0e0e4736"
  ["span_id"]=>
  string(16) "00f067aa0ba902b7"
  ["flags"]=>
  int(1)
}
bool(true)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
