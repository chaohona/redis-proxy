@echo off

set pwd=%~dp0
set GOPATH=%pwd%/src/lib;%~dp0

if [%1] == [] goto:all

if %1==clean (
call:clean
) else if %1==proto (
call:proto %2
) else (
call:build %1
)
goto:exit

:all
call:build gredis_frontboard

exit /b 0

:build
go install %1
if %errorlevel%==0 (
echo build %1 success!
) else (
echo build %1 error!
)
exit /b 0

:proto

exit /b 0

:clean
rm pkg/* -rf
rm bin/*.exe -rf
echo clean ok!
exit /b 0

:exit