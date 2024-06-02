# -*- coding: utf-8 -*-
#test on python 3.4 ,python of lower version  has different module organization.
import http.server
from http.server import HTTPServer, BaseHTTPRequestHandler
import socketserver
import posixpath

PORT = 8000

Handler = http.server.SimpleHTTPRequestHandler

def guess_type(self, path):
	base, ext = posixpath.splitext(path)
	if ext in self.extensions_map:
		return self.extensions_map[ext]
	ext = ext.lower()
	if ext in self.extensions_map:
		return self.extensions_map[ext]
	return self.extensions_map['']

Handler.extensions_map[''] = "text/html";
Handler.extensions_map['.html'] = "text/html";
Handler.extensions_map['.htm'] = "text/html";
Handler.extensions_map['.css'] = "text/css";
Handler.extensions_map['.js'] = "text/javascript";
Handler.extensions_map['.json'] = "application/json";
Handler.guess_type = guess_type

httpd = socketserver.TCPServer(("", PORT), Handler)

print("serving at port", PORT)
httpd.serve_forever()