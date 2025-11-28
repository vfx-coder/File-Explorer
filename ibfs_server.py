from http.server import HTTPServer, SimpleHTTPRequestHandler
import json
import subprocess
import os
import cgi
import urllib.parse
import webbrowser
import threading
import time

class IBFSHandler(SimpleHTTPRequestHandler):
    
    def do_GET(self):
        print(f"GET request: {self.path}")
        
        if self.path == '/':
            self.path = '/index.html'
            return SimpleHTTPRequestHandler.do_GET(self)
        
        elif self.path.startswith('/api/list'):
            query = urllib.parse.urlparse(self.path).query
            params = urllib.parse.parse_qs(query)
            path = params.get('path', ['/'])[0]
            
            print(f"Listing directory: {path}")
            
            result = subprocess.run(['./ibfs_tool', 'mydisk.ibfs', 'ls', path], 
                                  capture_output=True, text=True)
            
            files = self.parse_ls_output(result.stdout)
            
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps({'files': files}).encode())
        
        elif self.path.startswith('/api/mkdir'):
            query = urllib.parse.urlparse(self.path).query
            params = urllib.parse.parse_qs(query)
            path = params.get('path', [''])[0]
            
            print(f"Creating directory: {path}")
            
            result = subprocess.run(['./ibfs_tool', 'mydisk.ibfs', 'mkdir', path], 
                                  capture_output=True, text=True)
            
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps({
                'success': result.returncode == 0,
                'message': result.stdout if result.returncode == 0 else result.stderr
            }).encode())
        
        elif self.path.startswith('/api/cp_in'):
            query = urllib.parse.urlparse(self.path).query
            params = urllib.parse.parse_qs(query)
            host_path = params.get('host_path', [''])[0]
            ibfs_path = params.get('ibfs_path', [''])[0]
            
            print(f"Copying from {host_path} to {ibfs_path}")
            
            result = subprocess.run(['./ibfs_tool', 'mydisk.ibfs', 'cp_in', host_path, ibfs_path], 
                                  capture_output=True, text=True)
            
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps({
                'success': result.returncode == 0,
                'message': result.stdout if result.returncode == 0 else result.stderr
            }).encode())
        
        elif self.path.startswith('/api/cp_out'):
            query = urllib.parse.urlparse(self.path).query
            params = urllib.parse.parse_qs(query)
            ibfs_path = params.get('ibfs_path', [''])[0]
            host_path = params.get('host_path', [''])[0]
            
            print(f"Copying from {ibfs_path} to {host_path}")
            
            result = subprocess.run(['./ibfs_tool', 'mydisk.ibfs', 'cat', ibfs_path], 
                                  capture_output=True, text=True)
            
            if result.returncode == 0:
                try:
                    with open(host_path, 'w', encoding='utf-8') as f:
                        f.write(result.stdout)
                    success = True
                    message = "File copied successfully"
                except Exception as e:
                    success = False
                    message = f"Error writing to host file: {str(e)}"
            else:
                success = False
                message = result.stderr
            
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps({
                'success': success,
                'message': message
            }).encode())
        
        else:
            return SimpleHTTPRequestHandler.do_GET(self)
    
    def parse_ls_output(self, output):
        """Parse ibfs_tool ls output into structured data"""
        files = []
        lines = output.split('\n')
        
        for line in lines:
            line = line.strip()
            if not line or '---' in line or 'Type Lnk' in line or 'Listing directory' in line or 'ls Complete' in line:
                continue
            
            parts = line.split()
            if len(parts) >= 4:
                file_type = parts[0]
                name = parts[-1]
                display_name = name.rstrip('/')
                
                files.append({
                    'name': display_name,
                    'type': 'directory' if file_type == 'd' else 'file',
                    'size': parts[2] if len(parts) > 2 else '',
                    'full_name': name
                })
        
        return files
    
    def do_POST(self):
        if self.path == '/api/upload':
            content_type = self.headers.get('Content-Type', '')
            
            if 'multipart/form-data' in content_type:
                form = cgi.FieldStorage(
                    fp=self.rfile,
                    headers=self.headers,
                    environ={'REQUEST_METHOD': 'POST', 'CONTENT_TYPE': content_type}
                )
                
                file_item = form['file']
                path_value = form.getvalue('path', '/')
                
                if file_item.filename:
                    temp_path = f"temp_{file_item.filename}"
                    with open(temp_path, 'wb') as f:
                        f.write(file_item.file.read())
                    
                    result = subprocess.run(['./ibfs_tool', 'mydisk.ibfs', 'cp_in', temp_path, path_value], 
                                          capture_output=True, text=True)
                    
                    os.remove(temp_path)
                    
                    self.send_response(200)
                    self.send_header('Content-type', 'application/json')
                    self.end_headers()
                    self.wfile.write(json.dumps({
                        'success': result.returncode == 0,
                        'message': result.stdout if result.returncode == 0 else result.stderr
                    }).encode())
                    return
            
            self.send_response(400)
            self.end_headers()
        
        elif self.path == '/api/delete':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            data = json.loads(post_data)
            
            path = data['path']
            print(f"Deleting: {path}")
            
            if path.endswith('/'):
                cmd = 'rmdir'
                path = path.rstrip('/')
            else:
                cmd = 'rm'
            
            result = subprocess.run(['./ibfs_tool', 'mydisk.ibfs', cmd, path], 
                                  capture_output=True, text=True)
            
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({
                'success': result.returncode == 0,
                'message': result.stdout if result.returncode == 0 else result.stderr
            }).encode())

def run_server():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    # Check if mydisk.ibfs exists
    if not os.path.exists('mydisk.ibfs'):
        print("❌ Error: mydisk.ibfs not found!")
        print("Please run: ./mkfs mydisk.ibfs first")
        return
    
    server = HTTPServer(('localhost', 8000), IBFSHandler)
    
    print("🚀 Starting IBFS File Manager...")
    print("📍 Local:   http://localhost:8000")
    print("📂 Disk:    mydisk.ibfs")
    print("⏹️  Press Ctrl+C to stop")
    print("=" * 50)
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n✅ Server stopped successfully")

def open_browser():
    """Wait for server to start then open browser"""
    time.sleep(3)  # Give server time to start
    print("🌐 Opening browser...")
    try:
        webbrowser.open('http://localhost:8000')
        print("✅ Browser opened successfully!")
    except Exception as e:
        print(f"❌ Could not open browser: {e}")
        print("💡 Please manually open: http://localhost:8000")

def compile_c_programs():
    """Compile all C programs"""
    print("🔨 Compiling C programs...")
    
    # Check if source files exist
    required_files = ['ibfs_tool.c', 'io.c', 'bitmap.c', 'inode.c', 'bplustree.c']
    missing_files = [f for f in required_files if not os.path.exists(f)]
    
    if missing_files:
        print(f"❌ Missing files: {missing_files}")
        return False
    
    # Compile ibfs_tool
    compile_cmd = [
        'gcc', '-o', 'ibfs_tool', 
        'ibfs_tool.c', 'io.c', 'bitmap.c', 'inode.c', 'bplustree.c'
    ]
    
    result = subprocess.run(compile_cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        print("❌ Compilation failed!")
        print("Error details:")
        print(result.stderr)
        return False
    
    print("✅ Compilation successful!")
    return True

if __name__ == '__main__':
    print("=" * 50)
    print("       IBFS FILE MANAGER - WEB GUI")
    print("=" * 50)
    
    # Step 1: Compile C programs
    if not compile_c_programs():
        print("❌ Cannot start server due to compilation errors")
        exit(1)
    
    # Step 2: Start browser in background thread
    browser_thread = threading.Thread(target=open_browser)
    browser_thread.daemon = True
    browser_thread.start()
    
    # Step 3: Start server
    run_server()