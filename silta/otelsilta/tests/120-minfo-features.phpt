--TEST--
phpinfo lists every enabled feature flag
--INI--
otelsilta.features.errors=1
otelsilta.features.db=1
otelsilta.features.http=0
otelsilta.features.cache=1
otelsilta.features.templates=0
otelsilta.features.functions=0
otelsilta.features.profiling=0
--FILE--
<?php
ob_start();
phpinfo(INFO_MODULES);
$info = ob_get_clean();
preg_match('/^Features => (.*)$/m', $info, $m);
var_dump(trim($m[1] ?? ''));
?>
--EXPECT--
string(15) "errors db cache"
