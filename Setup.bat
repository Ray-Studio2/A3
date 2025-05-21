@echo off

:-----------------------------------------------------------------------------------------------------------------------------
REM Check administrator privileges
REM ref : https://bebhionn.tistory.com/52
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"

REM If error flag is set, we don't have admin rights
if '%errorlevel%' NEQ '0' (goto UACPrompt) else ( goto gotAdmin )

:UACPrompt
    echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
    set params = %*:"=""
    echo UAC.ShellExecute "cmd.exe", "/c %~s0 %params%", "", "runas", 1 >> "%temp%\getadmin.vbs"

    "%temp%\getadmin.vbs"
    del "%temp%\getadmin.vbs"
    exit /B

:gotAdmin
    REM When running with admin rights, default path changes, so change it to bat file path (%~dp0)
    CD /D "%~dp0"
:-----------------------------------------------------------------------------------------------------------------------------

:-----------------------------------------------------------------------------------------------------------------------------
echo.
echo [Execute VsDevCmd.bat]
REM Find Visual Studio 2022 installation
set VS2022_PATH=

REM Try to find Visual Studio installation using vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS2022_PATH=%%i\Common7\Tools\VsDevCmd.bat"
    goto :found_vs
)

:found_vs
if "%VS2022_PATH%"=="" (
    echo Error: Visual Studio 2022 not found.
    echo Please install Visual Studio 2022 and try again.
    pause
    exit /B 1
)

echo Using Visual Studio 2022 at: %VS2022_PATH%
call "%VS2022_PATH%"
:-----------------------------------------------------------------------------------------------------------------------------

mkdir ThirdParty
pushd .\ThirdParty
echo [vcpkg task]
if not exist "ThirdParty/vcpkg" (
	git clone https://github.com/microsoft/vcpkg
	call .\vcpkg\bootstrap-vcpkg.bat
)

.\vcpkg\vcpkg install glfw3:x64-windows
.\vcpkg\vcpkg install glslang:x64-windows
.\vcpkg\vcpkg install vulkan:x64-windows
.\vcpkg\vcpkg install vulkan-validationlayers:x64-windows
.\vcpkg\vcpkg install spirv-tools:x64-windows
echo [VK_ADD_LAYER_PATH]
echo %cd%\vcpkg\installed\x64-windows\bin
setx VK_ADD_LAYER_PATH "%cd%\vcpkg\installed\x64-windows\bin"

.\vcpkg\vcpkg integrate install
popd

echo [Done]
pause