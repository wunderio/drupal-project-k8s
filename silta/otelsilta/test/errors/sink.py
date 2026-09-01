#!/usr/bin/env python3
# Minimal OTLP/HTTP sink: writes each POST body to /tmp/otlp/req-N.json
import http.server, os, itertools
os.makedirs("/tmp/otlp", exist_ok=True)
counter = itertools.count(1)
class H(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        n = next(counter)
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length)
        with open(f"/tmp/otlp/req-{n}.json", "wb") as f:
            f.write(body)
        self.send_response(200); self.end_headers()
    def log_message(self, *a): pass
http.server.HTTPServer(("127.0.0.1", 4318), H).serve_forever()
