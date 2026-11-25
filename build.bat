@echo off
set "GCC_CMD=gcc"

where gcc >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [NOTE] 'gcc' not found directly in current PATH. Attempting standard invocation...
)

echo =========================================================
echo Compiling Campus Event Registration Register C Backend...
echo =========================================================

%GCC_CMD% -std=c99 -Iinclude -Wall -Wextra src/main.c src/http_server.c src/service.c src/storage.c src/validator.c src/json_utils.c -o server.exe -lws2_32

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [BUILD SUCCESS] server.exe created successfully!
    echo Run server using: .\server.exe
) else (
    echo.
    echo [BUILD FAILED] Compilation errors occurred.
)
