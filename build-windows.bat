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

REM ============================
REM Optional Clean Build
REM ============================
set "DO_CLEAN=0"
if "%~1"=="clean" set "DO_CLEAN=1"
if "!DO_CLEAN!"=="1" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    if exist "dist" rmdir /s /q "dist"
)

REM ============================
REM Resolve vcpkg toolchain
REM ============================
if defined VCPKG_TOOLCHAIN_FILE (
    set "TOOLCHAIN=%VCPKG_TOOLCHAIN_FILE%"
    goto :vcpkg_resolved
)
if defined VCPKG_ROOT (
    set "TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
    goto :vcpkg_resolved
)

REM Try to find vcpkg in PATH if not set explicitly
for /f "delims=" %%I in ('where vcpkg 2^>nul') do (
    set "VCPKG_ROOT=%%~dpI"
    set "TOOLCHAIN=%%~dpIscripts\buildsystems\vcpkg.cmake"
    goto :vcpkg_resolved
)

echo ERROR: vcpkg not configured.
echo Set VCPKG_ROOT or ensure 'vcpkg' is in your PATH.
exit /b 1

:vcpkg_resolved
if not exist "%TOOLCHAIN%" (
    echo ERROR: vcpkg toolchain file not found: %TOOLCHAIN%
    exit /b 1
)

:vcpkg_found
REM ============================
REM Install vcpkg dependencies
REM ============================
if not defined VCPKG_ROOT goto :skip_vcpkg_install
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo WARN: vcpkg executable not found, skipping install.
    goto :skip_vcpkg_install
)

echo Installing vcpkg dependencies...
if defined VCPKG_TARGET_TRIPLET (
    "%VCPKG_ROOT%\vcpkg.exe" install --triplet "%VCPKG_TARGET_TRIPLET%" --x-install-root "%BUILD_DIR%\vcpkg_installed"
) else (
    "%VCPKG_ROOT%\vcpkg.exe" install --x-install-root "%BUILD_DIR%\vcpkg_installed"
)
if errorlevel 1 (
    echo ERROR: vcpkg installation failed.
    pause
)

:skip_vcpkg_install
REM ============================
REM Configure + Build using presets
REM ============================
set "PRESET=%~1"
if "!DO_CLEAN!"=="1" set "PRESET=%~2"
if "%PRESET%"=="" set "PRESET=debug"

cmake --preset "%PRESET%"
if errorlevel 1 exit /b 1

cmake --build --preset "%PRESET%" --parallel
if errorlevel 1 exit /b 1

REM ============================
REM Build GUI (Nuitka standalone)
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
    if not exist "%VENV_DIR%" %PYTHON_BIN% -3 -m venv "%VENV_DIR%"
) else (
    if not exist "%VENV_DIR%" %PYTHON_BIN% -m venv "%VENV_DIR%"
)
if errorlevel 1 (
    echo ERROR: Python 3 not found. Install Python and retry.
    exit /b 1
)
call "%VENV_DIR%\Scripts\activate.bat"

REM Only install requirements if they changed or venv is new
set "REQ_HASH=%VENV_DIR%\requirements.hash"
if not exist "launcher\requirements.txt" goto :skip_requirements
set "NEEDS_INSTALL=1"
if exist "%REQ_HASH%" (
    fc /b "launcher\requirements.txt" "%REQ_HASH%" >nul 2>&1
    if not errorlevel 1 set "NEEDS_INSTALL=0"
)

if "!NEEDS_INSTALL!"=="0" goto :skip_requirements
echo Updating launcher dependencies...
python -m pip install --upgrade pip >nul
python -m pip install -r launcher\requirements.txt >nul
copy /Y "launcher\requirements.txt" "%REQ_HASH%" >nul

:skip_requirements
set "LAUNCHER_OUT_DIR=%BUILD_DIR%\launcher_build"
set "LAUNCHER_BIN=%LAUNCHER_OUT_DIR%\main.dist\launcher.exe"

set "REBUILD_LAUNCHER=0"
if not exist "%LAUNCHER_BIN%" set "REBUILD_LAUNCHER=1"
if "%~1"=="force" set "REBUILD_LAUNCHER=1"
if "%~2"=="force" set "REBUILD_LAUNCHER=1"

if "!REBUILD_LAUNCHER!"=="0" (
    echo Launcher is up to date. (Use '.\build-windows.bat force' or '.\build-windows.bat clean force' to rebuild it)
    goto :launcher_done
)

echo Building launcher (standalone)...
python -m nuitka --standalone --assume-yes-for-downloads --lto=no --windows-console-mode=disable --enable-plugin=tk-inter --include-package=customtkinter --output-dir="%LAUNCHER_OUT_DIR%" --output-filename=launcher launcher\main.py
if errorlevel 1 exit /b 1

:launcher_done

REM ============================
REM Prepare Clean Distribution
REM ============================
set "DIST_DIR=dist"
if not exist "%DIST_DIR%" (
    mkdir "%DIST_DIR%"
)
if not exist "%DIST_DIR%\game" (
    mkdir "%DIST_DIR%\game"
)

echo Polishing distribution folder...
set "LAUNCHER_DIST=%LAUNCHER_OUT_DIR%\main.dist"
if exist "%LAUNCHER_DIST%" (
    robocopy "%LAUNCHER_DIST%" "%DIST_DIR%" /E /XO /NJH /NJS /NP >nul
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
    if not exist "%DIST_DIR%\game\assets" mkdir "%DIST_DIR%\game\assets"
    robocopy "assets" "%DIST_DIR%\game\assets" /E /XO /NJH /NJS /NP >nul
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
