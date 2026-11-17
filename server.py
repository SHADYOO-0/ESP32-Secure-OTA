import http.server
import socketserver
from urllib.parse import urlparse, parse_qs

PORT = 8000

class SecureOTAServer(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        parsed_path = urlparse(self.path)
        
        # If the ESP32 is reporting update status
        if parsed_path.path == "/report":
            query = parse_qs(parsed_path.query)
            status = query.get("status", ["UNKNOWN"])[0]
            
            print("\n" + "="*50)
            if status == "SUCCESS":
                print("  >>> [SERVER NOTIFICATION]: ESP32 VERIFIED & APPLIED UPDATE! <<<")
            elif status == "TAMPER_DETECTED":
                print("  >>> [SECURITY ALERT]: ESP32 DETECTED SIGNATURE MISMATCH / TAMPERING! <<<")
                print("  >>> Malicious firmware was rejected by the device! <<<")
            print("="*50 + "\n")
            
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"OK")
            return

        # Otherwise serve files normally (firmware.bin, firmware.sig)
        return super().do_GET()

print(f"[*] Secure OTA Server running on port {PORT}...")
print("[*] Waiting for ESP32 requests...\n")
with socketserver.TCPServer(("", PORT), SecureOTAServer) as httpd:
    httpd.serve_forever()