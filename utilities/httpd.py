import http.server
import socketserver


class MyHttpRequestHandler(http.server.SimpleHTTPRequestHandler):
  def do_GET(self):
    print(f"Client: {self.client_address}")
    print(f"Requested path: {self.path}")
    print(f"Headers: {self.headers}")
    if self.path.startswith("/espotify"):
      print("Matched /espotify")
      # The HTTP response line is written to the internal buffer,
      # followed by Server and Date headers.
      self.send_response(200)
      self.send_header('Content-type', 'text/html')
      self.end_headers()
      self.wfile.write(b"<h1>Hejka</h1>")
    # return http.server.SimpleHTTPRequestHandler.do_GET(self)
    return

  def do_POST(self):
    print(f"Client: {self.client_address}")
    print(f"Requested path: {self.path}")
    print(f"Headers: {self.headers}")
    content_length = int(self.headers['Content-Length'])
    post_data = self.rfile.read(content_length)
    print(f"Data: {post_data}")
    return


# Create an object of the above class
handler_object = MyHttpRequestHandler

PORT = 8008
my_server = socketserver.TCPServer(("", PORT), handler_object)

# Star the server
try:
  my_server.serve_forever()
except:
  print("Shutting down...")
  my_server.shutdown()
