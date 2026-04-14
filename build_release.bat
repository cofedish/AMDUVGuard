@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
msbuild AMDUVGuard.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
exit /b %errorlevel%
