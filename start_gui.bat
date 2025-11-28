@echo off
echo Starting IBFS File Manager...
echo Compiling C programs...

REM Compile C programs
gcc -o ibfs_tool ibfs_tool.c io.c bitmap.c inode.c bplustree.c

echo Starting web server...
echo Browser will open automatically...

REM Python server start karo aur browser open karo
start http://localhost:8000
python ibfs_server.py

pause