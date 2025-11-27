@echo off
setlocal enabledelayedexpansion

:: ============================================================================
:: ESP32 Firmware Manager
:: ============================================================================
:: One-stop script for all firmware build and release operations
:: ============================================================================

title ESP32 Firmware Manager

:MAIN_MENU

:: Check if we're in the correct directory
if not exist "main\main.c" (
    echo ERROR: This script must be run from the Poem_cam project root directory
    echo Current directory: %CD%
    pause
    exit /b 1
)

:: Check if ESP-IDF is available, if not try to initialize it
call idf.py --version >nul 2>&1
if errorlevel 1 (
    echo ESP-IDF not found in PATH, attempting to initialize...
    
    :: Try common ESP-IDF installation paths
    set "IDF_FOUND=0"
    set "EXPORT_BAT="
    
    :: Check %USERPROFILE%\esp\vX.X.X\esp-idf pattern
    for /d %%D in ("%USERPROFILE%\esp\v*") do (
        if exist "%%D\esp-idf\export.bat" (
            set "EXPORT_BAT=%%D\esp-idf\export.bat"
            goto :found_idf
        )
    )
    
    :: Check %USERPROFILE%\.espressif (legacy)
    if exist "%USERPROFILE%\.espressif\export.bat" (
        set "EXPORT_BAT=%USERPROFILE%\.espressif\export.bat"
        goto :found_idf
    )
    
    :: Check C:\Espressif\frameworks pattern
    for /d %%D in ("C:\Espressif\frameworks\esp-idf-v*") do (
        if exist "%%D\export.bat" (
            set "EXPORT_BAT=%%D\export.bat"
            goto :found_idf
        )
    )
    
    :: Check C:\esp\esp-idf
    if exist "C:\esp\esp-idf\export.bat" (
        set "EXPORT_BAT=C:\esp\esp-idf\export.bat"
        goto :found_idf
    )
    
    :found_idf
    if defined EXPORT_BAT (
        echo Found ESP-IDF at: !EXPORT_BAT!
        
        :: Get IDF directory
        set "IDF_DIR="
        for %%F in ("!EXPORT_BAT!") do set "IDF_DIR=%%~dpF"
        set "IDF_DIR=!IDF_DIR:~0,-1!"
        
        :: Find existing Python environment
        set "PYTHON_ENV_DIR="
        for /d %%D in ("%USERPROFILE%\.espressif\python_env\idf*_py*") do (
            if exist "%%D\Scripts\python.exe" (
                set "PYTHON_ENV_DIR=%%D"
                goto :found_python
            )
        )
        
        :found_python
        if defined PYTHON_ENV_DIR (
            echo Found Python environment: !PYTHON_ENV_DIR!
            
            :: Manually initialize ESP-IDF environment
            set "IDF_PATH=!IDF_DIR!"
            set "IDF_TOOLS_PATH=%USERPROFILE%\.espressif"
            
            :: Activate Python virtual environment
            call "!PYTHON_ENV_DIR!\Scripts\activate.bat"
            
            :: Add ESP-IDF tools to PATH
            set "PATH=!IDF_PATH!\tools;!IDF_TOOLS_PATH!\tools\bin;%PATH%"
            
            echo ESP-IDF environment initialized manually
        ) else (
            echo Python environment not found, trying standard export...
            call "!EXPORT_BAT!"
        )
        
        set "IDF_FOUND=1"
    )
    
    :: Check again if it worked
    call idf.py --version >nul 2>&1
    if errorlevel 1 (
        echo.
        echo ERROR: ESP-IDF initialization failed
        echo.
        echo Please run the ESP-IDF Command Prompt from Start Menu instead.
        echo.
        pause
        exit /b 1
    ) else (
        echo ESP-IDF initialized successfully!
        timeout /t 2 >nul
    )
)

cls
echo ========================================
echo   ESP32 Firmware Manager
echo ========================================
echo.

:: Get current version
for /f "tokens=3" %%a in ('findstr "FIRMWARE_VERSION" main\ota_manager.h') do set CURRENT_VER=%%a
set CURRENT_VER=%CURRENT_VER:"=%
echo Current Version: %CURRENT_VER%
echo.

echo.
echo   [1] Build firmware only
echo   [2] Create official release (vX.Y.Z)
echo   [3] Create testing build
echo   [4] Flash firmware to device
echo   [5] Clean build directory
echo   [6] Monitor device logs
echo   [0] Exit
echo.

set /p MAIN_CHOICE="Select option (0-6): "

if "%MAIN_CHOICE%"=="0" goto EXIT_SCRIPT
if "%MAIN_CHOICE%"=="1" goto BUILD_ONLY
if "%MAIN_CHOICE%"=="2" goto RELEASE_OFFICIAL
if "%MAIN_CHOICE%"=="3" goto RELEASE_TESTING
if "%MAIN_CHOICE%"=="4" goto FLASH_DEVICE
if "%MAIN_CHOICE%"=="5" goto CLEAN_BUILD
if "%MAIN_CHOICE%"=="6" goto MONITOR_DEVICE

echo Invalid choice. Please try again.
timeout /t 2 >nul
goto MAIN_MENU
goto MAIN_MENU

:: ============================================================================
:: BUILD ONLY
:: ============================================================================
:BUILD_ONLY
cls
echo ========================================
echo   Build Firmware
echo ========================================
echo.

call idf.py build
if errorlevel 1 (
    echo.
    echo ========================================
    echo   BUILD FAILED!
    echo ========================================
    pause
    goto MAIN_MENU
)

echo.
echo ========================================
echo   Build Successful!
echo ========================================
echo.

for %%A in (build\Poem_cam.bin) do set "BIN_SIZE=%%~zA"
echo Firmware: build\Poem_cam.bin
echo Size: !BIN_SIZE! bytes
echo.

:BUILD_SUBMENU
echo What next?
echo   [1] Flash to device
echo   [2] Copy to releases folder
echo   [3] Return to main menu
echo.
set /p BUILD_NEXT="Choice: "

if "%BUILD_NEXT%"=="1" goto FLASH_DEVICE
if "%BUILD_NEXT%"=="2" (
    if not exist "releases\%CURRENT_VER%" mkdir "releases\%CURRENT_VER%"
    copy "build\Poem_cam.bin" "releases\%CURRENT_VER%\firmware.bin" >nul
    echo.
    echo Copied to: releases\%CURRENT_VER%\firmware.bin
    explorer "releases\%CURRENT_VER%"
    pause
    goto MAIN_MENU
)
if "%BUILD_NEXT%"=="3" goto MAIN_MENU

echo Invalid choice.
timeout /t 1 >nul
goto BUILD_SUBMENU

:: ============================================================================
:: FLASH DEVICE
:: ============================================================================
:FLASH_DEVICE
cls
echo ========================================
echo   Flash Firmware to Device
echo ========================================
echo.

:: Try to detect COM port
echo Detecting COM ports...
set PORT_COUNT=0
for /f "tokens=1" %%p in ('wmic path Win32_SerialPort get DeviceID ^| findstr "COM"') do (
    set /a PORT_COUNT+=1
    set "PORT_!PORT_COUNT!=%%p"
    echo   [!PORT_COUNT!] %%p
)

if %PORT_COUNT%==0 (
    echo No COM ports detected.
    echo.
    set /p MANUAL_PORT="Enter COM port manually (e.g., COM3): "
    set "FLASH_PORT=!MANUAL_PORT!"
) else if %PORT_COUNT%==1 (
    set "FLASH_PORT=!PORT_1!"
    echo.
    echo Auto-selected: !FLASH_PORT!
) else (
    echo.
    set /p PORT_CHOICE="Select port (1-%PORT_COUNT%) or enter manually: "
    
    :: Check if numeric choice
    echo !PORT_CHOICE! | findstr /R "^[0-9][0-9]*$" >nul
    if not errorlevel 1 (
        if !PORT_CHOICE! LEQ %PORT_COUNT% (
            set "FLASH_PORT=!PORT_%PORT_CHOICE%!"
        ) else (
            echo Invalid selection.
            pause
            goto MAIN_MENU
        )
    ) else (
        set "FLASH_PORT=!PORT_CHOICE!"
    )
)

echo.
echo Flashing to !FLASH_PORT!...
echo.

call idf.py -p !FLASH_PORT! flash
if errorlevel 1 (
    echo.
    echo Flash failed!
    pause
    goto MAIN_MENU
)

echo.
echo ========================================
echo   Flash Successful!
echo ========================================
echo.
echo Monitor device? [Y/N]
set /p MONITOR_CHOICE=": "
if /i "%MONITOR_CHOICE%"=="Y" (
    call idf.py -p !FLASH_PORT! monitor
)

pause
goto MAIN_MENU

:: ============================================================================
:: CLEAN BUILD
:: ============================================================================
:CLEAN_BUILD
cls
echo ========================================
echo   Clean Build Directory
echo ========================================
echo.

call idf.py fullclean
if errorlevel 1 (
    echo Clean failed!
    pause
    goto MAIN_MENU
)

echo.
echo Build directory cleaned successfully!
pause
goto MAIN_MENU

:: ============================================================================
:: MONITOR DEVICE
:: ============================================================================
:MONITOR_DEVICE
cls
echo ========================================
echo   Monitor Device Logs
echo ========================================
echo.

:: Detect COM port
echo Detecting COM ports...
set PORT_COUNT=0
for /f "tokens=1" %%p in ('wmic path Win32_SerialPort get DeviceID ^| findstr "COM"') do (
    set /a PORT_COUNT+=1
    set "PORT_!PORT_COUNT!=%%p"
    echo   [!PORT_COUNT!] %%p
)

if %PORT_COUNT%==0 (
    echo No COM ports detected.
    pause
    goto MAIN_MENU
) else if %PORT_COUNT%==1 (
    set "MON_PORT=!PORT_1!"
    echo.
    echo Monitoring: !MON_PORT!
    echo.
    call idf.py -p !MON_PORT! monitor
) else (
    echo.
    set /p PORT_CHOICE="Select port (1-%PORT_COUNT%): "
    set "MON_PORT=!PORT_%PORT_CHOICE%!"
    echo.
    call idf.py -p !MON_PORT! monitor
)

pause
goto MAIN_MENU

:: ============================================================================
:: RELEASE OFFICIAL
:: ============================================================================
:RELEASE_OFFICIAL
cls
echo ========================================
echo   Create Official Release
echo ========================================
echo.
echo Current version: %CURRENT_VER%
echo.

set /p NEW_VERSION="Enter new version (X.Y.Z, e.g., 1.0.1): "

:: Validate version format
echo %NEW_VERSION% | findstr /R "^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$" >nul
if errorlevel 1 (
    echo.
    echo ERROR: Invalid format. Use X.Y.Z (e.g., 1.0.0)
    pause
    goto MAIN_MENU
)

set "TAG_NAME=v%NEW_VERSION%"
set "VERSION=%NEW_VERSION%"

echo.
echo New version: v%NEW_VERSION%
echo.
echo Enter release notes (Ctrl+Z then Enter when done):
echo.

set "NOTES_FILE=%TEMP%\release_notes.txt"
copy con "%NOTES_FILE%" >nul
echo.

goto DO_RELEASE

:: ============================================================================
:: RELEASE TESTING
:: ============================================================================
:RELEASE_TESTING
cls
echo ========================================
echo   Create Testing Build
echo ========================================
echo.

:: Generate timestamp version
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set datetime=%%I
set "VERSION=testing-%datetime:~0,8%-%datetime:~8,6%"
set "TAG_NAME=testing"

echo Testing version: %VERSION%
echo Tag: %TAG_NAME%
echo.
echo Enter testing notes (Ctrl+Z then Enter when done):
echo.

set "NOTES_FILE=%TEMP%\release_notes.txt"
copy con "%NOTES_FILE%" >nul
echo.

goto DO_RELEASE

:: ============================================================================
:: PERFORM RELEASE
:: ============================================================================
:DO_RELEASE

:: Update version in code
echo ========================================
echo Updating version in code...
echo ========================================
echo.

set "OTA_HEADER=main\ota_manager.h"
set "TEMP_FILE=%TEMP%\ota_header_temp.h"

powershell -Command "(Get-Content '%OTA_HEADER%') -replace '#define FIRMWARE_VERSION \".*\"', '#define FIRMWARE_VERSION \"%TAG_NAME%\"' | Set-Content '%TEMP_FILE%'"
move /y "%TEMP_FILE%" "%OTA_HEADER%" >nul

echo Updated to: %TAG_NAME%
echo.

:: Build firmware
echo ========================================
echo Building firmware...
echo ========================================
echo.

call idf.py fullclean
call idf.py build
if errorlevel 1 (
    echo.
    echo ========================================
    echo   BUILD FAILED!
    echo ========================================
    echo.
    echo Reverting version change...
    git checkout -- "%OTA_HEADER%"
    pause
    goto MAIN_MENU
)

echo.
echo ========================================
echo   Build Successful!
echo ========================================
echo.

for %%A in (build\Poem_cam.bin) do set "BIN_SIZE=%%~zA"
echo Binary size: !BIN_SIZE! bytes
echo.

:: Git operations
echo ========================================
echo Git operations...
echo ========================================
echo.

git add "%OTA_HEADER%"
git diff --cached --quiet
if errorlevel 1 (
    git commit -m "Release %TAG_NAME%"
    echo Committed version update
) else (
    echo No changes to commit
)

:: Create/update tag
if "%TAG_NAME%"=="testing" (
    git tag -d testing >nul 2>&1
    git tag -a "testing" -m "Testing build %VERSION%"
    echo Created testing tag
) else (
    git tag -a "%TAG_NAME%" -m "Release %TAG_NAME%"
    if errorlevel 1 (
        echo.
        echo ERROR: Tag already exists!
        echo Delete it with: git tag -d %TAG_NAME%
        pause
        goto MAIN_MENU
    )
    echo Created tag: %TAG_NAME%
)

echo.
echo Pushing to GitHub...
git push origin HEAD
if errorlevel 1 (
    echo Push failed!
    pause
    goto MAIN_MENU
)

if "%TAG_NAME%"=="testing" (
    git push origin testing --force
) else (
    git push origin "%TAG_NAME%"
)

if errorlevel 1 (
    echo Tag push failed!
    pause
    goto MAIN_MENU
)

echo Pushed to GitHub successfully!
echo.

:: Prepare release folder
echo ========================================
echo Preparing release...
echo ========================================
echo.

set "RELEASE_DIR=releases\%TAG_NAME%"
if not exist "releases" mkdir "releases"
if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%"

copy "build\Poem_cam.bin" "%RELEASE_DIR%\firmware.bin" >nul

:: Create release info
set "INFO_FILE=%RELEASE_DIR%\release-info.txt"
echo Firmware Release > "%INFO_FILE%"
echo ================ >> "%INFO_FILE%"
echo. >> "%INFO_FILE%"
echo Version: %TAG_NAME% >> "%INFO_FILE%"
echo Date: %DATE% %TIME% >> "%INFO_FILE%"
echo Size: !BIN_SIZE! bytes >> "%INFO_FILE%"
echo. >> "%INFO_FILE%"
echo Release Notes: >> "%INFO_FILE%"
echo -------------- >> "%INFO_FILE%"
type "%NOTES_FILE%" >> "%INFO_FILE%" 2>nul

echo.
echo ========================================
echo   Release Ready!
echo ========================================
echo.
echo Binary: %RELEASE_DIR%\firmware.bin
echo Size: !BIN_SIZE! bytes
echo.
echo Upload to: https://github.com/coencoensmeets/Poem_cam/releases/new?tag=%TAG_NAME%
echo.
explorer "%RELEASE_DIR%"

echo.
echo ========================================
echo   GitHub Release Steps:
echo ========================================
echo.
echo 1. Tag: %TAG_NAME% (auto-selected)
echo 2. Title: Release %VERSION%
echo 3. Copy notes from: %NOTES_FILE%
echo 4. Attach: %RELEASE_DIR%\firmware.bin
if "%TAG_NAME%"=="testing" echo 5. Check "This is a pre-release"
echo 6. Publish release
echo.

pause
goto MAIN_MENU

:: ============================================================================
:: EXIT
:: ============================================================================
:EXIT_SCRIPT
cls
echo.
echo Goodbye!
echo.
timeout /t 1 >nul
exit /b 0

endlocal
