<?php
for ($i = 0; $i < 8; $i++) {
    try { throw new \RuntimeException("boom-$i"); } catch (\Throwable $e) {}
}
echo "done";
