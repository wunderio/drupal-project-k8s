--TEST--
sanitize_url strips query and fragment (used for root span http.url)
--INI--
otelsilta.cli_enabled=1
--FILE--
<?php
var_dump(otelsilta_test_sanitize_url('/user/reset?token=SECRET&x=1'));
var_dump(otelsilta_test_sanitize_url('/plain/path'));
var_dump(otelsilta_test_sanitize_url('/a#frag'));
?>
--EXPECT--
string(11) "/user/reset"
string(11) "/plain/path"
string(2) "/a"
