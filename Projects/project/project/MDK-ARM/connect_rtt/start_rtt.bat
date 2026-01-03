@echo off
chcp 65001 >nul
 
:: 路径配置 
set JLINK_EXE=D:\J-link v8.76\JLink_V876\JLink.exe
set RTT_CLIENT_EXE=D:\J-link v8.76\JLink_V876\JLinkRTTClient.exe
set SCRIPT_DIR=%~dp0
set JLINK_SCRIPT=%SCRIPT_DIR%connect_rtt.jlink
 
if not exist "%JLINK_SCRIPT%" (
    echo [警告] 文件 %JLINK_SCRIPT% 不存在，请先生成该文件！
) else (
    echo 找到 JLink 脚本: %JLINK_SCRIPT%
)
 
:: 检查 JLink.exe 是否已经运行 
tasklist /FI "IMAGENAME eq JLink.exe" | find /I "JLink.exe" >nul
if %ERRORLEVEL%==0 (
    echo JLink.exe 已经运行，先结束旧实例...
    taskkill /IM JLink.exe /F >nul 2>&1
    timeout /t 1 /nobreak >nul
)
 
:: 隐藏启动的 JLink.exe 并执行脚本 
start /B "" "%JLINK_EXE%" -CommanderScript "%JLINK_SCRIPT%" -ExitOnError 0
 
:: 启动 RTT Client 
"%RTT_CLIENT_EXE%"
echo RTT 已启动，查看 RTT Client 输出。
pause
