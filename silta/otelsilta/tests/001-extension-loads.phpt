--TEST--
otelsilta extension loads
--FILE--
<?php
var_dump(extension_loaded('otelsilta'));
var_dump(function_exists('otelsilta_span_start'));
?>
--EXPECT--
bool(true)
bool(true)
