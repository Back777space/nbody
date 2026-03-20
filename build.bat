@echo off
setlocal

:: Setup VS environment (auto-detect location)
for %%E in (Community Professional Enterprise) do (
    for %%D in (C D E) do (
        if exist "%%D:\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
            call "%%D:\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
            goto :vcvars_found
        )
    )
)
:vcvars_found

:: Set VCPKG_ROOT (auto-detect vcpkg location)
if not defined VCPKG_ROOT (
    :: Try Visual Studio installations (Community, Professional, Enterprise)
    for %%E in (Community Professional Enterprise) do (
        for %%D in (C D E) do (
            if exist "%%D:\Microsoft Visual Studio\2022\%%E\VC\vcpkg\vcpkg.exe" (
                set "VCPKG_ROOT=%%D:\Microsoft Visual Studio\2022\%%E\VC\vcpkg"
                goto :vcpkg_found
            )
        )
    )

    :: Try common standalone locations
    if exist "C:\vcpkg\vcpkg.exe" set "VCPKG_ROOT=C:\vcpkg" && goto :vcpkg_found
    if exist "%USERPROFILE%\vcpkg\vcpkg.exe" set "VCPKG_ROOT=%USERPROFILE%\vcpkg" && goto :vcpkg_found

    echo WARNING: vcpkg not found. CMake may fail.
    goto :vcpkg_found

    :vcpkg_found
    if defined VCPKG_ROOT echo Found vcpkg at: %VCPKG_ROOT%
)
if not defined VCPKG_INSTALLATION_ROOT (
    set "VCPKG_INSTALLATION_ROOT=%VCPKG_ROOT%"
)

:: Default config
set CONFIG=Debug
if not "%1"=="" set CONFIG=%1

:: Configure if needed
if not exist "out\build\x64-%CONFIG%\CMakeCache.txt" (
    echo Configuring CMake...
    cmake --preset x64-%CONFIG%
    if errorlevel 1 exit /b 1
)

:: Build
echo Building [%CONFIG%]...
cmake --build "out/build/x64-%CONFIG%" --config %CONFIG% --parallel
if errorlevel 1 exit /b 1

echo.
echo Build complete! Binary: out\build\x64-%CONFIG%\%CONFIG%\nbody.exe
