@echo off
:: 실행 파일 경로 설정
set EXE=Debug\IrrlichtDemo_Server_CP.exe
:: 실행할 더미 클라이언트 수
set COUNT=5

if not exist %EXE% (
    echo [ERROR] %EXE% 파일을 찾을 수 없습니다. 
    echo 프로젝트를 먼저 빌드해 주세요.
    pause
    exit /b
)

echo Starting %COUNT% Dummy Clients (Headless, No Bot)...
for /l %%i in (1, 1, %COUNT%) do (
    echo Launching Dummy Client %%i...
    :: 클라이언트 실행: -console(메뉴 스킵), -driver 5(NULL 드라이버)
    :: -server와 -bot 옵션을 제외하여 순수 클라이언트로 동작합니다.
    start "Dummy Client %%i" %EXE% -console -driver 5
)

echo.
echo %COUNT% instances are running in background (Headless).
echo To stop them, use 'taskkill /f /im IrrlichtDemo_Server_CP.exe'.
pause