@echo off
chcp 65001 >nul
mode con cols=100 lines=34 >nul 2>nul
cd /d "%~dp0"
if not exist diyu.exe (
    echo 未找到 diyu.exe，请先运行 build.bat 编译。
    pause
    exit /b 1
)
diyu.exe
