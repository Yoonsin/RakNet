@echo off
echo Terminating all IrrlichtDemo_Server_CP instances...
:: /f는 강제 종료, /im은 이미지 이름(파일명)으로 찾기를 의미합니다.
taskkill /f /im IrrlichtDemo_Server_CP.exe
echo.
echo Done.
pause
