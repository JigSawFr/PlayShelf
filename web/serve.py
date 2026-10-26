#!/usr/bin/env python3
"""Serves a folder with the headers threads need in a browser (COOP / COEP).

Usage: web/serve.py [folder] [port]"""
import http.server, sys, functools

class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cache-Control', 'no-cache')
        super().end_headers()

folder = sys.argv[1] if len(sys.argv) > 1 else '.'
port = int(sys.argv[2]) if len(sys.argv) > 2 else 8000
http.server.ThreadingHTTPServer(('', port), functools.partial(Handler, directory=folder)).serve_forever()
