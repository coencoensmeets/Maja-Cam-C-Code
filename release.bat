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
echo   [1] Create official release (vX.Y.Z)
echo   [2] Create testing build
echo   [0] Exit
echo.

set /p MAIN_CHOICE="Select option (0-2): "

if "%MAIN_CHOICE%"=="0" goto EXIT_SCRIPT
if "%MAIN_CHOICE%"=="1" goto RELEASE_OFFICIAL
if "%MAIN_CHOICE%"=="2" goto RELEASE_TESTING

echo Invalid choice. Please try again.
timeout /t 2 >nul
goto MAIN_MENU

:: ============================================================================
:: BUILD ONLY
:: ============================================================================
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
echo Enter release notes (press Enter on empty line when done):
echo.

set "NOTES_FILE=%TEMP%\release_notes.txt"
if exist "%NOTES_FILE%" del "%NOTES_FILE%"

:READ_NOTES_OFFICIAL
set "line="
set /p "line="
if not defined line goto NOTES_DONE_OFFICIAL
echo !line! >> "%NOTES_FILE%"
goto READ_NOTES_OFFICIAL

:NOTES_DONE_OFFICIAL
echo.

:: Ensure notes file exists (even if empty)
if not exist "%NOTES_FILE%" (
    echo Official release > "%NOTES_FILE%"
)

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

:: Get current version from ota_manager.h
for /f "tokens=3" %%v in ('findstr /C:"FIRMWARE_VERSION" main\ota_manager.h') do set "BASE_VERSION=%%v"
:: Remove quotes from version
set "BASE_VERSION=%BASE_VERSION:"=%"

:: Check if it's already a test version (e.g., v1.2.0-test.3)
echo %BASE_VERSION% | findstr /R "\-test\.[0-9]*$" >nul
if not errorlevel 1 (
    :: Extract base version (v1.2.0) and test number (3)
    for /f "tokens=1 delims=-" %%b in ("%BASE_VERSION%") do set "CLEAN_VERSION=%%b"
    :: Extract test number after "-test."
    for /f "tokens=2 delims=." %%n in ('echo %BASE_VERSION% ^| findstr /R "\-test\."') do set "TEST_PART=%%n"
    :: TEST_PART now contains everything after the last dot before -test, extract just the number
    for /f "tokens=2 delims=-" %%t in ('echo %BASE_VERSION%') do (
        for /f "tokens=2 delims=." %%n in ("%%t") do set "TEST_NUM=%%n"
    )
    set /a TEST_NUM+=1
) else (
    :: First test version for this release
    set "CLEAN_VERSION=%BASE_VERSION%"
    set "TEST_NUM=1"
)

set "VERSION=%CLEAN_VERSION%-test.%TEST_NUM%"
set "TAG_NAME=%VERSION%"

echo Testing version: %VERSION%
echo Base version: %CLEAN_VERSION%
echo Test number: %TEST_NUM%
echo.
echo Enter testing notes (press Enter on empty line when done):
echo.

set "NOTES_FILE=%TEMP%\release_notes.txt"
if exist "%NOTES_FILE%" del "%NOTES_FILE%"

:READ_NOTES_TESTING
set "line="
set /p "line="
if not defined line goto NOTES_DONE_TESTING
echo !line! >> "%NOTES_FILE%"
goto READ_NOTES_TESTING

:NOTES_DONE_TESTING
echo.

:: Ensure notes file exists (even if empty)
if not exist "%NOTES_FILE%" (
    echo Automated testing build > "%NOTES_FILE%"
)

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

:: Check if firmware already exists - use it if available
set "FIRMWARE_BIN="
if exist "build\Poem_cam.bin" (
    set "FIRMWARE_BIN=build\Poem_cam.bin"
) else if exist "build\project-name.bin" (
    set "FIRMWARE_BIN=build\project-name.bin"
)

if defined FIRMWARE_BIN (
    echo.
    echo Using existing firmware: !FIRMWARE_BIN!
    for %%A in ("!FIRMWARE_BIN!") do set "BIN_SIZE=%%~zA"
    echo Size: !BIN_SIZE! bytes
    echo.
    goto SKIP_BUILD
)

:: Firmware must already exist in build folder
echo.
echo Note: Using existing firmware from build folder
echo If you need to rebuild, run: idf.py build
echo.

:SKIP_BUILD
echo.
echo ========================================
echo   Build Successful!
echo ========================================
echo.

:: Find the firmware binary
set "FIRMWARE_BIN="
if exist "build\Poem_cam.bin" (
    set "FIRMWARE_BIN=build\Poem_cam.bin"
) else if exist "build\project-name.bin" (
    set "FIRMWARE_BIN=build\project-name.bin"
)

if defined FIRMWARE_BIN (
    for %%A in ("%FIRMWARE_BIN%") do set "BIN_SIZE=%%~zA"
    echo Binary size: !BIN_SIZE! bytes
) else (
    echo ERROR: Firmware binary not found!
    echo Reverting version change...
    git checkout -- "%OTA_HEADER%"
    pause
    goto MAIN_MENU
)
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

:: Create tag
git tag -a "%TAG_NAME%" -m "Release %TAG_NAME%"
if errorlevel 1 (
    echo.
    echo ERROR: Tag already exists!
    echo Delete it with: git tag -d %TAG_NAME%
    pause
    goto MAIN_MENU
)
echo Created tag: %TAG_NAME%

echo.
echo Pushing to GitHub...
git push origin HEAD
if errorlevel 1 (
    echo Push failed!
    pause
    goto MAIN_MENU
)

git push origin "%TAG_NAME%"
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

:: Find the firmware binary (could be Poem_cam.bin or project-name.bin)
set "FIRMWARE_BIN="
if exist "build\Poem_cam.bin" (
    set "FIRMWARE_BIN=build\Poem_cam.bin"
) else if exist "build\project-name.bin" (
    set "FIRMWARE_BIN=build\project-name.bin"
)

if not defined FIRMWARE_BIN (
    echo.
    echo ERROR: Firmware binary not found!
    echo Please build the firmware first.
    pause
    goto MAIN_MENU
)

copy "%FIRMWARE_BIN%" "%RELEASE_DIR%\firmware.bin"
if errorlevel 1 (
    echo.
    echo ERROR: Failed to copy firmware file!
    pause
    goto MAIN_MENU
)

echo Firmware copied to %RELEASE_DIR%\firmware.bin
echo.

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
echo   Creating GitHub Release...
echo ========================================
echo.

:: Check if GitHub CLI is installed
set "GH_PATH="
where gh >nul 2>&1
if not errorlevel 1 (
    set "GH_PATH=gh"
) else if exist "C:\Program Files\GitHub CLI\gh.exe" (
    set "GH_PATH=C:\Program Files\GitHub CLI\gh.exe"
)

if defined GH_PATH (
    echo Using GitHub CLI: !GH_PATH!
    goto CREATE_RELEASE
) else (
    goto MANUAL_UPLOAD
)

:MANUAL_UPLOAD
echo GitHub CLI not found. Manual upload required.
echo.
echo Binary: %RELEASE_DIR%\firmware.bin
echo Size: !BIN_SIZE! bytes
echo.
echo Upload to: https://github.com/coencoensmeets/Maja-Cam/releases/new?tag=%TAG_NAME%
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

:: Check if this is a testing build
echo %TAG_NAME% | findstr /R "\-test\.[0-9]*$" >nul
if not errorlevel 1 (
    echo 5. Check "This is a pre-release"
)

echo 6. Publish release
echo.
echo Press any key to exit...
pause >nul
exit /b 0

:CREATE_RELEASE
:: Create GitHub release using gh CLI
echo %TAG_NAME% | findstr /R "\-test\.[0-9]*$" >nul
if not errorlevel 1 (
    :: This is a testing build - create as prerelease
    "%GH_PATH%" release create "%TAG_NAME%" "%RELEASE_DIR%\firmware.bin" --title "Testing Build %VERSION%" --notes-file "%NOTES_FILE%" --prerelease --repo coencoensmeets/Maja-Cam
) else (
    :: This is an official release
    "%GH_PATH%" release create "%TAG_NAME%" "%RELEASE_DIR%\firmware.bin" --title "Release %VERSION%" --notes-file "%NOTES_FILE%" --repo coencoensmeets/Maja-Cam
)

if errorlevel 1 (
    echo.
    echo GitHub release creation failed!
    echo You can create it manually at:
    echo https://github.com/coencoensmeets/Maja-Cam/releases/new?tag=%TAG_NAME%
    echo.
    explorer "%RELEASE_DIR%"
    echo Press any key to exit...
    pause >nul
    exit /b 0
)

echo.
echo ========================================
echo   Release Published Successfully!
echo ========================================
echo.
echo Binary: %RELEASE_DIR%\firmware.bin
echo Size: !BIN_SIZE! bytes
echo Release: https://github.com/coencoensmeets/Maja-Cam/releases/tag/%TAG_NAME%
echo.

echo Press any key to exit...
pause >nul
exit /b 0

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
