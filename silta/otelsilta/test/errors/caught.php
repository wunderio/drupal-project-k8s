<?php
try { throw new \RuntimeException('caught-boom'); }
catch (\Throwable $e) { /* handled */ }
echo "ok";
