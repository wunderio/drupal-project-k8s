<?php
http_response_code(500);
try { throw new \LogicException('rendered-500'); }
catch (\Throwable $e) { echo "handled as 500"; }
