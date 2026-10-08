@echo off
rem Builds 4Stim's Scaleform movies with the Apache Flex SDK's mxmlc:
rem   FourStimPickerMenu.swf, FourStimHUDMenu.swf    -> Data\Interface\
rem   HUDLogo.swf                                    -> Data\Interface\4Stim\
rem   Search/Align/Utility/Navigation.swf (tab icons) -> Data\Interface\4Stim\Icons\Tabs\
rem The built files go into .\out\ in that same folder layout, ready to copy
rem into Data\.
rem
rem FLEX_HOME:      the folder the Flex SDK installer put the SDK in.
rem PLAYER_VERSION: the folder name under %FLEX_HOME%\frameworks\libs\player\
rem                 holding playerglobal.swc (11.0). The output SWF format is
rem                 pinned separately by -swf-version=13, which is what FO4's
rem                 Scaleform needs.
rem Needs Java on your PATH.

set FLEX_HOME=F:\Documents\ApacheSDK
set PLAYER_VERSION=11.0

cd /d "%~dp0"
set OUT=%~dp0out\Interface
if not exist "%OUT%\4Stim\Icons\Tabs" mkdir "%OUT%\4Stim\Icons\Tabs"
set FAILED=0

call :build . FourStimPickerMenu.as "%OUT%\FourStimPickerMenu.swf" 1280 720
call :build . FourStimHUDMenu.as "%OUT%\FourStimHUDMenu.swf" 1280 720
call :build logo HUDLogo.as "%OUT%\4Stim\HUDLogo.swf" 256 256
for %%I in (Search Align Utility Navigation) do call :build icons %%I.as "%OUT%\4Stim\Icons\Tabs\%%I.swf" 64 64

if %FAILED%==1 (
  echo.
  echo Some movies failed to build - see the errors above.
) else (
  echo.
  echo Built everything into %~dp0out\Interface - copy that folder into Data\.
)
pause
exit /b

:build
rem %1 source folder, %2 main class file, %3 output, %4 %5 stage size
pushd "%~1"
call "%FLEX_HOME%\bin\mxmlc.bat" ^
  -source-path=. ^
  -target-player=%PLAYER_VERSION% ^
  -swf-version=13 ^
  -default-size %4 %5 ^
  -default-frame-rate 60 ^
  -static-link-runtime-shared-libraries=true ^
  -output=%3 ^
  %2
if errorlevel 1 set FAILED=1
popd
exit /b
