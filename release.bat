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
echo   [3] Delete tags
echo   [4] Sync tags with GitHub
echo   [0] Exit
echo.

set /p MAIN_CHOICE="Select option (0-4): "

if "%MAIN_CHOICE%"=="0" goto EXIT_SCRIPT
if "%MAIN_CHOICE%"=="1" goto RELEASE_OFFICIAL
if "%MAIN_CHOICE%"=="2" goto RELEASE_TESTING
if "%MAIN_CHOICE%"=="3" goto DELETE_TAGS
if "%MAIN_CHOICE%"=="4" goto SYNC_TAGS

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
set "IS_TESTING_BUILD=0"

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
echo ========================================
echo   Create Testing Build
echo ========================================
echo.

:: Get current version from ota_manager.h
for /f "tokens=3" %%v in ('findstr /C:"FIRMWARE_VERSION" main\ota_manager.h') do set "BASE_VERSION=%%v"
:: Remove quotes from version
set "BASE_VERSION=%BASE_VERSION:"=%"

:: Always strip all -test.N suffixes to get clean semantic version
:: e.g., v1.0.0-test.1 -> v1.0.0, v1.0.0-test.1-test.1 -> v1.0.0
for /f "tokens=1 delims=-" %%b in ("%BASE_VERSION%") do set "CLEAN_VERSION=%%b"

:: Find the highest test number on GitHub (not just locally)
echo Checking GitHub for existing test versions...
set "MAX_TEST_NUM=0"
for /f "tokens=*" %%t in ('git ls-remote --tags origin ^| findstr /R "%CLEAN_VERSION%-test\.[0-9]" ^| findstr /R "test\.[0-9]*$"') do (
    for /f "tokens=2 delims=/" %%v in ("%%t") do (
        for /f "tokens=2 delims=." %%n in ("%%v") do (
            if %%n GTR !MAX_TEST_NUM! set "MAX_TEST_NUM=%%n"
        )
    )
)

:: Next test number is one more than the highest found
set /a TEST_NUM=MAX_TEST_NUM+1

if !MAX_TEST_NUM! GTR 0 (
    echo Found existing test versions up to test.!MAX_TEST_NUM! on GitHub
)

set "VERSION=%CLEAN_VERSION%-test.%TEST_NUM%"
set "TAG_NAME=%VERSION%"
set "IS_TESTING_BUILD=1"

echo Testing version: %VERSION% [PRE-RELEASE]
echo Base version: %CLEAN_VERSION%
echo Test number: %TEST_NUM%
echo.
echo NOTE: This will be marked as a PRE-RELEASE on GitHub
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
:: DELETE TAGS
:: ============================================================================
:DELETE_TAGS
echo ========================================
echo   Delete Tags
echo ========================================
echo.
echo Fetching tags from local repository and GitHub...
echo.

:: Get all local tags and check if they exist on remote
set "TAG_COUNT=0"
for /f "tokens=*" %%t in ('git tag -l') do (
    set /a TAG_COUNT+=1
    set "TAG_!TAG_COUNT!=%%t"
    set "SELECTED_!TAG_COUNT!=0"
    
    :: Check if tag exists on remote
    git ls-remote --tags origin "%%t" 2>nul | findstr "%%t" >nul
    if !ERRORLEVEL! EQU 0 (
        set "REMOTE_!TAG_COUNT!=YES"
    ) else (
        set "REMOTE_!TAG_COUNT!=NO"
    )
)

if !TAG_COUNT! EQU 0 (
    echo No tags found in local repository.
    echo.
    pause
    goto MAIN_MENU
)

:DELETE_TAGS_MENU
cls
echo ========================================
echo   Delete Tags ^(!TAG_COUNT! tags found^)
echo ========================================
echo.
echo Select tags to delete (toggle with number):
echo   A - Select all tags
echo   C - Clear ALL tags (force delete everything)
echo   D - Delete selected tags
echo   0 - Cancel
echo.

:: Display tags with selection status and remote status
for /l %%i in (1,1,!TAG_COUNT!) do (
    if !SELECTED_%%i! EQU 1 (
        if "!REMOTE_%%i!"=="YES" (
            echo [X] %%i. !TAG_%%i! ^(local + remote^)
        ) else (
            echo [X] %%i. !TAG_%%i! ^(local only^)
        )
    ) else (
        if "!REMOTE_%%i!"=="YES" (
            echo [ ] %%i. !TAG_%%i! ^(local + remote^)
        ) else (
            echo [ ] %%i. !TAG_%%i! ^(local only^)
        )
    )
)

echo.
set /p TAG_CHOICE="Enter choice: "

:: Handle cancel
if /i "%TAG_CHOICE%"=="0" goto MAIN_MENU

:: Handle force clear all
if /i "%TAG_CHOICE%"=="C" goto FORCE_CLEAR_ALL_TAGS

:: Handle delete selected
if /i "%TAG_CHOICE%"=="D" goto DELETE_SELECTED_TAGS

:: Handle select all
if /i "%TAG_CHOICE%"=="A" (
    for /l %%i in (1,1,!TAG_COUNT!) do (
        set "SELECTED_%%i=1"
    )
    goto DELETE_TAGS_MENU
)

:: Handle toggle single tag
if "%TAG_CHOICE%" GEQ "1" if "%TAG_CHOICE%" LEQ "!TAG_COUNT!" (
    if !SELECTED_%TAG_CHOICE%! EQU 0 (
        set "SELECTED_%TAG_CHOICE%=1"
    ) else (
        set "SELECTED_%TAG_CHOICE%=0"
    )
    goto DELETE_TAGS_MENU
)

echo Invalid choice. Please try again.
timeout /t 2 >nul
goto DELETE_TAGS_MENU

:FORCE_CLEAR_ALL_TAGS
echo.
echo ========================================
echo   FORCE CLEAR ALL TAGS
echo ========================================
echo.
echo WARNING: This will DELETE ALL !TAG_COUNT! tags from:
echo   - Local repository
echo   - GitHub remote
echo.
echo This action CANNOT be undone!
echo.
set /p CONFIRM="Type 'DELETE ALL' to confirm: "

if /i not "%CONFIRM%"=="DELETE ALL" (
    echo Deletion cancelled.
    pause
    goto MAIN_MENU
)

echo.
echo Force deleting all tags...
echo.

set "SUCCESS_COUNT=0"
set "FAIL_COUNT=0"

:: First, delete all from GitHub
echo Deleting all tags from GitHub...
for /l %%i in (1,1,!TAG_COUNT!) do (
    if "!REMOTE_%%i!"=="YES" (
        git push origin --delete "!TAG_%%i!" >nul 2>&1
        if !ERRORLEVEL! EQU 0 (
            echo   Remote: !TAG_%%i! deleted from GitHub
            set /a SUCCESS_COUNT+=1
        ) else (
            echo   Remote: !TAG_%%i! failed to delete from GitHub
            set /a FAIL_COUNT+=1
        )
    )
)

:: Then delete all local tags
echo.
echo Deleting all local tags...
for /f "tokens=*" %%t in ('git tag -l') do (
    git tag -d "%%t" >nul 2>&1
)

echo.
echo ========================================
echo   Force Clear Complete
echo ========================================
echo.
echo Remote tags deleted: !SUCCESS_COUNT!
echo Remote tags failed: !FAIL_COUNT!
echo All local tags deleted.
echo.
pause
goto MAIN_MENU

:DELETE_SELECTED_TAGS
:: Count selected tags
set "DELETE_COUNT=0"
for /l %%i in (1,1,!TAG_COUNT!) do (
    if !SELECTED_%%i! EQU 1 (
        set /a DELETE_COUNT+=1
    )
)

if !DELETE_COUNT! EQU 0 (
    echo No tags selected.
    timeout /t 2 >nul
    goto DELETE_TAGS_MENU
)

echo.
echo ========================================
echo   Confirm Deletion
echo ========================================
echo.
echo You are about to delete !DELETE_COUNT! tag(s):
echo.
for /l %%i in (1,1,!TAG_COUNT!) do (
    if !SELECTED_%%i! EQU 1 (
        if "!REMOTE_%%i!"=="YES" (
            echo   - !TAG_%%i! ^(local + remote^)
        ) else (
            echo   - !TAG_%%i! ^(local only^)
        )
    )
)
echo.
set /p CONFIRM="Are you sure? (yes/no): "

if /i not "%CONFIRM%"=="yes" (
    echo Deletion cancelled.
    pause
    goto MAIN_MENU
)

echo.
echo Deleting tags...
echo.

set "SUCCESS_COUNT=0"
set "FAIL_COUNT=0"

for /l %%i in (1,1,!TAG_COUNT!) do (
    if !SELECTED_%%i! EQU 1 (
        echo Deleting !TAG_%%i!...
        
        :: Delete from GitHub if it exists there
        if "!REMOTE_%%i!"=="YES" (
            git push origin --delete "!TAG_%%i!" >nul 2>&1
            if !ERRORLEVEL! EQU 0 (
                echo   Remote: deleted from GitHub
            ) else (
                echo   Remote: failed to delete from GitHub
                set /a FAIL_COUNT+=1
            )
        )
        
        :: Delete locally
        git tag -d "!TAG_%%i!" >nul 2>&1
        if !ERRORLEVEL! EQU 0 (
            echo   Local: deleted
            set /a SUCCESS_COUNT+=1
        ) else (
            echo   Local: failed to delete
            set /a FAIL_COUNT+=1
        )
    )
)

echo.
echo ========================================
echo   Deletion Complete
echo ========================================
echo.
echo Successfully deleted: !SUCCESS_COUNT! tag(s)
echo Failed: !FAIL_COUNT! tag(s)
echo.
pause
goto MAIN_MENU

:: ============================================================================
:: SYNC TAGS
:: ============================================================================
:SYNC_TAGS
echo ========================================
echo   Sync Tags with GitHub
echo ========================================
echo.
echo This will:
echo   1. Delete ALL local tags
echo   2. Fetch tags from GitHub
echo   3. Your local repository will match GitHub exactly
echo.
echo WARNING: Any local-only tags will be LOST!
echo.
set /p CONFIRM="Continue? (yes/no): "

if /i not "%CONFIRM%"=="yes" (
    echo Sync cancelled.
    pause
    goto MAIN_MENU
)

echo.
echo Syncing tags...
echo.

:: Count local tags before deletion
set "LOCAL_COUNT=0"
for /f "tokens=*" %%t in ('git tag -l') do (
    set /a LOCAL_COUNT+=1
)

if !LOCAL_COUNT! GTR 0 (
    echo Step 1: Deleting !LOCAL_COUNT! local tag^(s^)...
    for /f "tokens=*" %%t in ('git tag -l') do (
        git tag -d "%%t" >nul 2>&1
    )
    echo   Deleted all local tags
) else (
    echo Step 1: No local tags to delete
)

echo.
echo Step 2: Fetching tags from GitHub...
git fetch --tags --force
if errorlevel 1 (
    echo   ERROR: Failed to fetch tags from GitHub
    echo.
    pause
    goto MAIN_MENU
)

:: Count remote tags after fetch
set "REMOTE_COUNT=0"
for /f "tokens=*" %%t in ('git tag -l') do (
    set /a REMOTE_COUNT+=1
)

echo   Fetched !REMOTE_COUNT! tag^(s^) from GitHub

echo.
echo ========================================
echo   Sync Complete
echo ========================================
echo.
echo Local tags before: !LOCAL_COUNT!
echo Remote tags synced: !REMOTE_COUNT!
echo.
echo Your local repository is now in sync with GitHub.
echo.
pause
goto MAIN_MENU

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

:: Check for uncommitted changes
git diff --quiet
if errorlevel 1 (
    echo Warning: Working directory has uncommitted changes
    if "%IS_TESTING_BUILD%"=="1" (
        echo This is OK for testing builds - they can include uncommitted changes
    )
    echo.
)

git add "%OTA_HEADER%"
git diff --cached --quiet
if errorlevel 1 (
    git commit -m "Release %TAG_NAME%"
    echo Committed version update
) else (
    echo No changes to commit
)

:: Create tag (with auto-increment if it already exists)
set "TAG_ATTEMPT=1"
set "FINAL_TAG=%TAG_NAME%"

:TAG_CREATION_LOOP
set "TAG_ERROR_FILE=%TEMP%\git_tag_error.txt"

:: For testing builds, use lightweight tags to avoid issues with dirty working directory
if "%IS_TESTING_BUILD%"=="1" (
    git tag "%FINAL_TAG%" 2>"%TAG_ERROR_FILE%"
    set "TAG_RESULT=!ERRORLEVEL!"
) else (
    git tag -a "%FINAL_TAG%" -m "Release %FINAL_TAG%" 2>"%TAG_ERROR_FILE%"
    set "TAG_RESULT=!ERRORLEVEL!"
)

:: If tag creation succeeded (errorlevel 0), we're done
if !TAG_RESULT! EQU 0 (
    echo Created tag: %FINAL_TAG%
    goto TAG_CREATED
)

:: Tag creation failed - check if it's because it already exists
findstr /C:"already exists" "%TAG_ERROR_FILE%" >nul
if !ERRORLEVEL! EQU 0 (
    :: Tag already exists
    if "%IS_TESTING_BUILD%"=="1" (
        :: Testing build - auto-increment test number
        set /a TEST_NUM+=1
        set "FINAL_TAG=%CLEAN_VERSION%-test.!TEST_NUM!"
        set /a TAG_ATTEMPT+=1
        
        echo.
        echo Tag already exists, trying next test version: !FINAL_TAG!
        echo.
        
        if !TAG_ATTEMPT! LEQ 10 (
            :: Also update version in code to match new test number
            set "TEMP_FILE=%TEMP%\ota_header_temp.h"
            powershell -Command "(Get-Content '%OTA_HEADER%') -replace '#define FIRMWARE_VERSION \".*\"', '#define FIRMWARE_VERSION \"!FINAL_TAG!\"' | Set-Content '!TEMP_FILE!'"
            move /y "!TEMP_FILE!" "%OTA_HEADER%" >nul
            
            :: Stage the new version change and amend commit
            git add "%OTA_HEADER%"
            git commit --amend --no-edit >nul 2>&1
            
            goto TAG_CREATION_LOOP
        ) else (
            echo.
            echo ERROR: Too many tag attempts (!TAG_ATTEMPT!)
            echo Please manually check git tags: git tag -l
            pause
            goto MAIN_MENU
        )
    ) else (
        :: Official release - check if tag was pushed to remote
        git ls-remote --tags origin "%FINAL_TAG%" 2>nul | findstr "%FINAL_TAG%" >nul
        if !ERRORLEVEL! EQU 0 (
            :: Tag exists on remote - abort
            echo.
            echo ERROR: Tag already exists locally and on GitHub: %FINAL_TAG%
            echo This release has already been published.
            echo.
            pause
            goto MAIN_MENU
        ) else (
            :: Tag exists locally but not on remote - use it
            echo.
            echo Warning: Tag exists locally but not on remote. Will push existing tag.
            echo Created tag: %FINAL_TAG%
            goto TAG_CREATED
        )
    )
) else (
    :: Different git error - show detailed error message
    echo.
    echo ========================================
    echo   ERROR: Failed to Create Git Tag
    echo ========================================
    echo.
    echo Tag: %FINAL_TAG%
    echo.
    
    :: Display the actual error from git
    if exist "%TAG_ERROR_FILE%" (
        for %%A in ("%TAG_ERROR_FILE%") do set "FILESIZE=%%~zA"
        if defined FILESIZE (
            if !FILESIZE! GTR 0 (
                echo Git Error Message:
                echo ------------------
                type "%TAG_ERROR_FILE%"
                echo.
                echo ------------------
            )
        )
    )
    
    if exist "%TAG_ERROR_FILE%" del "%TAG_ERROR_FILE%"
    
    echo.
    echo Attempting to diagnose the issue...
    echo.
    echo Current git status:
    git status --short
    echo.
    echo Recent tags:
    git tag -l --sort=-version:refname | findstr /R "test\.[0-9]" | more +0
    echo.
    pause
    goto MAIN_MENU
)

:TAG_CREATED
:: Clean up error file
if exist "%TAG_ERROR_FILE%" del "%TAG_ERROR_FILE%"

:: Update TAG_NAME variable for rest of script
set "TAG_NAME=%FINAL_TAG%"

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
echo ========================================
echo   GitHub CLI Not Installed
echo ========================================
echo.
echo The GitHub CLI (gh) is not installed. To enable automatic
echo release creation, install it using one of these methods:
echo.
echo Method 1 - WinGet (Recommended):
echo   winget install --id GitHub.cli
echo.
echo Method 2 - Chocolatey:
echo   choco install gh
echo.
echo Method 3 - Manual Download:
echo   https://cli.github.com/
echo.
echo After installation, authenticate with:
echo   gh auth login
echo.
echo ========================================
echo   Manual Upload Required
echo ========================================
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
    echo 5. Check "This is a pre-release" ^(REQUIRED for testing builds^)
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
    echo Creating PRE-RELEASE (testing build)
    echo.
    echo Running: %GH_PATH% release create "%TAG_NAME%" --title "[TEST] %VERSION%" --notes-file "%NOTES_FILE%" --prerelease --repo coencoensmeets/Maja-Cam
    "%GH_PATH%" release create "%TAG_NAME%" "%RELEASE_DIR%\firmware.bin" --title "[TEST] %VERSION%" --notes-file "%NOTES_FILE%" --prerelease --repo coencoensmeets/Maja-Cam
    set "RELEASE_ERROR=!ERRORLEVEL!"
    goto CHECK_RELEASE_RESULT
) else (
    :: This is an official release
    echo Creating OFFICIAL RELEASE
    echo.
    echo Running: %GH_PATH% release create "%TAG_NAME%" --title "Release %VERSION%" --notes-file "%NOTES_FILE%" --repo coencoensmeets/Maja-Cam
    "%GH_PATH%" release create "%TAG_NAME%" "%RELEASE_DIR%\firmware.bin" --title "Release %VERSION%" --notes-file "%NOTES_FILE%" --repo coencoensmeets/Maja-Cam
    set "RELEASE_ERROR=!ERRORLEVEL!"
    goto CHECK_RELEASE_RESULT
)

:CHECK_RELEASE_RESULT

if !RELEASE_ERROR! equ 0 (
    echo.
    echo ========================================
    echo   Release Published Successfully!
    echo ========================================
    echo.
    
    :: Check if this was a testing build
    echo %TAG_NAME% | findstr /R "\-test\.[0-9]*$" >nul
    if not errorlevel 1 (
        echo Type: PRE-RELEASE ^(Testing Build^)
    ) else (
        echo Type: OFFICIAL RELEASE
    )
    echo.
    echo Binary: %RELEASE_DIR%\firmware.bin
    echo Size: !BIN_SIZE! bytes
    echo Release: https://github.com/coencoensmeets/Maja-Cam/releases/tag/%TAG_NAME%
    echo.
    echo Press any key to exit...
    pause >nul
    exit /b 0
) else (
    echo.
    echo GitHub release creation failed with error code: !RELEASE_ERROR!
    echo.
    echo Troubleshooting:
    echo 1. Check if gh CLI is authenticated: gh auth status
    echo 2. Verify tag exists on GitHub: git ls-remote --tags origin
    echo 3. Try manual upload at:
    echo    https://github.com/coencoensmeets/Maja-Cam/releases/new?tag=%TAG_NAME%
    echo.
    echo Binary location: %RELEASE_DIR%\firmware.bin
    echo Notes file: %NOTES_FILE%
    echo.
    explorer "%RELEASE_DIR%"
    echo Press any key to exit...
    pause >nul
    exit /b 0
)

:: ============================================================================
:: EXIT
:: ============================================================================
:EXIT_SCRIPT
echo.
echo Goodbye!
echo.
timeout /t 1 >nul
exit /b 0

endlocal
