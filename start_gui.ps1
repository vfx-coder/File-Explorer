Write-Host "Starting IBFS File Manager..." -ForegroundColor Green
Write-Host "Compiling C programs..." -ForegroundColor Yellow

# Compile C programs
gcc -o ibfs_tool ibfs_tool.c io.c bitmap.c inode.c bplustree.c

Write-Host "Starting web server..." -ForegroundColor Yellow
Write-Host "Browser will open automatically..." -ForegroundColor Green

# Browser open karo
Start-Process "http://localhost:8000"

# Server start karo
python ibfs_server.py