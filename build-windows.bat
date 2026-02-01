@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ============================
REM Force MSVC environment
REM ============================
where cl.exe >nul 2>&1
if errorlevel 1 (
    echo cl.exe not found in PATH. Attempting to locate Visual Studio...
    
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!VSWHERE!" (
        set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
    )

    if exist "!VSWHERE!" (
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            set "VS_PATH=%%i"
        )
        
        if defined VS_PATH (
            echo Found Visual Studio at: !VS_PATH!
            if exist "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" (
                echo Activating x64 Developer Command Prompt...
                call "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x64
            ) else (
                echo ERROR: vcvarsall.bat not found in !VS_PATH!\VC\Auxiliary\Build\
                exit /b 1
            )
        ) else (
            echo ERROR: Visual Studio C++ tools not found via vswhere.
            exit /b 1
        )
    ) else (
        echo ERROR: vswhere.exe not found. Please install Visual Studio or run from a Developer Command Prompt.
        exit /b 1
    )
)

REM ============================
REM Defaults
REM ============================
if not defined BUILD_DIR set BUILD_DIR=build
set "GAME_DIR=%BUILD_DIR%\game"

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

REM ============================
REM Resolve vcpkg toolchain
REM ============================
if defined VCPKG_TOOLCHAIN_FILE (
    set "TOOLCHAIN=%VCPKG_TOOLCHAIN_FILE%"
) else if defined VCPKG_ROOT (
    set "TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
) else (
    REM Try to find vcpkg in PATH if not set explicitly
    for /f "delims=" %%I in ('where vcpkg 2^>nul') do (
        set "VCPKG_ROOT=%%~dpI"
        set "TOOLCHAIN=%%~dpIscripts\buildsystems\vcpkg.cmake"
        goto :vcpkg_found
    )

    echo ERROR: vcpkg not configured.
    echo.
    echo Set one of:
    echo   VCPKG_ROOT=path\to\vcpkg
    echo   VCPKG_TOOLCHAIN_FILE=path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
    echo.
    echo Or ensure 'vcpkg' is in your system PATH.
    echo.
    echo Example:
    echo   set VCPKG_ROOT=C:\vcpkg
    exit /b 1
)

:vcpkg_found
if not exist "%TOOLCHAIN%" (
    echo ERROR: vcpkg toolchain file not found:
    echo   %TOOLCHAIN%
    exit /b 1
)

REM ============================
REM Install vcpkg dependencies
REM ============================
if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\vcpkg.exe" (
        echo Installing vcpkg dependencies...
        if defined VCPKG_TARGET_TRIPLET (
            "%VCPKG_ROOT%\vcpkg.exe" install --triplet "%VCPKG_TARGET_TRIPLET%" --x-install-root "%BUILD_DIR%\vcpkg_installed"
        ) else (
            "%VCPKG_ROOT%\vcpkg.exe" install --x-install-root "%BUILD_DIR%\vcpkg_installed"
        )
        if errorlevel 1 (
            echo.
            echo ERROR: vcpkg installation failed.
            echo Press any key to continue anyway, or press Ctrl+C to abort...
            pause
        )
    ) else (
        echo WARN: vcpkg executable not found at %VCPKG_ROOT%\vcpkg.exe (skipping install)
    )
)

REM ============================
REM Configure + Build using presets
REM ============================
set "PRESET=%~1"
if "%PRESET%"=="" set "PRESET=debug"

cmake --preset "%PRESET%"
if errorlevel 1 exit /b 1

cmake --build --preset "%PRESET%" --parallel
if errorlevel 1 exit /b 1

REM ============================
REM Build GUI (Nuitka onefile)
REM ============================
set "VENV_DIR=launcher\.venv-build"
if not defined PYTHON_BIN set "PYTHON_BIN=py"
if "%PYTHON_BIN%"=="py" (
    %PYTHON_BIN% -3 --version >nul 2>&1
    if errorlevel 1 (
        set "PYTHON_BIN=python"
    )
)

if "%PYTHON_BIN%"=="py" (
    %PYTHON_BIN% -3 -m venv "%VENV_DIR%"
) else (
    %PYTHON_BIN% -m venv "%VENV_DIR%"
)
if errorlevel 1 (
    echo ERROR: Python 3 not found. Install Python and retry.
    exit /b 1
)
call "%VENV_DIR%\Scripts\activate.bat"
python -m pip install --upgrade pip >nul
python -m pip install -r launcher\requirements.txt >nul

echo Building launcher (standalone)...
REM NOTE: We use --standalone instead of --onefile because --onefile often triggers 
REM false positives in Windows Defender and other AV software, which can prevent 
REM the launcher from opening.
set "LAUNCHER_OUT_DIR=%BUILD_DIR%\launcher_build"
python -m nuitka --standalone --assume-yes-for-downloads --lto=no --windows-console-mode=disable --enable-plugin=tk-inter --include-package=customtkinter --output-dir="%LAUNCHER_OUT_DIR%" --output-filename=launcher launcher\main.py
if errorlevel 1 exit /b 1

REM ============================
REM Prepare Clean Distribution
REM ============================
set "DIST_DIR=dist"
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"
mkdir "%DIST_DIR%\game"

echo Polishing distribution folder...
set "LAUNCHER_DIST=%LAUNCHER_OUT_DIR%\main.dist"
if exist "%LAUNCHER_DIST%" (
    xcopy /E /I /Y "%LAUNCHER_DIST%\*" "%DIST_DIR%\" >nul
)

set "SNAKE_PATH="
if exist "%BUILD_DIR%\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\snake.exe"
if not defined SNAKE_PATH if exist "%BUILD_DIR%\Debug\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\Debug\snake.exe"
if not defined SNAKE_PATH if exist "%BUILD_DIR%\Release\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\Release\snake.exe"
if not defined SNAKE_PATH if exist "%BUILD_DIR%\bin\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\bin\snake.exe"
if not defined SNAKE_PATH if exist "%BUILD_DIR%\Debug\snake\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\Debug\snake\snake.exe"
if not defined SNAKE_PATH if exist "%BUILD_DIR%\Release\snake\snake.exe" set "SNAKE_PATH=%BUILD_DIR%\Release\snake\snake.exe"
if not defined SNAKE_PATH (
    for /r "%BUILD_DIR%" %%F in (snake.exe) do (
        if not defined SNAKE_PATH set "SNAKE_PATH=%%F"
    )
)

if defined SNAKE_PATH (
    copy /Y "%SNAKE_PATH%" "%DIST_DIR%\game\snake.exe" >nul
) else (
    echo ERROR: missing snake binary after build.
    exit /b 1
)

echo Copying game assets and dependencies...
if exist "assets" (
    mkdir "%DIST_DIR%\game\assets"
    xcopy /E /I /Y "assets\*" "%DIST_DIR%\game\assets\" >nul
)
copy /Y "%BUILD_DIR%\*.dll" "%DIST_DIR%\game\" >nul 2>nul
if exist "launcher\readme.txt" (
    copy /Y "launcher\readme.txt" "%DIST_DIR%\readme.txt" >nul
)

echo.
echo ============================
echo Build Complete: .\%DIST_DIR%
echo ============================
echo Launcher: .\%DIST_DIR%\launcher.exe
echo Game:     .\%DIST_DIR%\game\snake.exe
