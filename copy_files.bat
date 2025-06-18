@echo off
echo [INFO] 복사 시작...

xcopy /s /y /d ^
  "C:\GitHub\RakNet\Source\*" ^
  "C:\GitHub\RakNet\DependentExtensions\IrrlichtDemo_Server_CP\LinuxLib\RakNet_Include\"

echo [INFO] 복사 완료.