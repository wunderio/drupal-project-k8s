--TEST--
SQL sanitizer: doubled quotes, backslash escapes, signed numbers, hex literals
--INI--
otelsilta.cli_enabled=1
--FILE--
<?php
$cases = [
    "SELECT * FROM t WHERE a = 'x' AND b = 5",
    "SELECT * FROM t WHERE name = 'it''s ok' AND x = 5",
    "SELECT * FROM t WHERE p = 'a\\\\' AND q = 2",
    "SELECT * FROM t WHERE x = -5",
    "SELECT * FROM t WHERE flags = 0x1F",
    "INSERT INTO t VALUES (NULL, TRUE, FALSE)",
    "SELECT * FROM t WHERE name = 'ab''''cd' AND x = 5",
];
foreach ($cases as $c) { echo otelsilta_test_sanitize_sql($c), "\n"; }
?>
--EXPECT--
SELECT * FROM t WHERE a = '?' AND b = ?
SELECT * FROM t WHERE name = '?' AND x = ?
SELECT * FROM t WHERE p = '?' AND q = ?
SELECT * FROM t WHERE x = -?
SELECT * FROM t WHERE flags = ?
INSERT INTO t VALUES (?, ?, ?)
SELECT * FROM t WHERE name = '?' AND x = ?
