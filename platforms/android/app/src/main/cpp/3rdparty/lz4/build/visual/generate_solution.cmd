:: Requires 1 parameter == GENERATOR
@echo off
setlocal

:: Set path
set "BUILD_BASE_DIR=%~dp0"  :: Use the directory where the script is located
set "CMAKELIST_DIR=..\..\cmake"

if "%~1"=="" (
    echo No generator specified as first parameter
    exit /b 1
)
set "GENERATOR=%~1"

:: Check if a user-defined CMAKE_PATH is set and prioritize it
if defined CMAKE_PATH (
    set "CMAKE_EXECUTABLE=%CMAKE_PATH%\cmake.exe"
    echo Using user-defined cmake at %CMAKE_PATH%
) else (
    :: Attempt to find cmake in the system PATH
    where cmake >nul 2>&1
    if %ERRORLEVEL% neq 0 (
        :: Use the default standard cmake installation directory if not found in PATH
        set "CMAKE_PATH=C:\Program Files\CMake\bin"
        echo CMake not in system PATH => using default CMAKE_PATH=%CMAKE_PATH%
        set "CMAKE_EXECUTABLE=%CMAKE_PATH%\cmake.exe"
    ) else (
        set "CMAKE_EXECUTABLE=cmake"
        echo CMake found in system PATH.
    )
)

:: Set the build directory to a subdirectory named after the generator
set "BUILD_DIR=%BUILD_BASE_DIR%\%GENERATOR%"

:: Create the build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

:: Run CMake to configure the project and generate the solution
pushd "%BUILD_DIR%"
"%CMAKE_EXECUTABLE%" -G "%GENERATOR%" "%CMAKELIST_DIR%"
if %ERRORLEVEL% neq 0 goto :error

:: If successful, end script
echo Build configuration successful for %GENERATOR%.
goto :end

:error
echo Failed to configure build for %GENERATOR%.
exit /b 1

:end
popd
endlocal
@echo on
