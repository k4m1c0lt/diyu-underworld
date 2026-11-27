@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ============================================
echo   大闹阴曹 · 归山  /  一键编译
echo ============================================

rem 1) 已经在 VS 开发人员命令行里
where cl >nul 2>nul
if %errorlevel%==0 (
    cl /nologo /O2 /utf-8 diyu.c /Fe:diyu.exe
    goto :done
)

rem 2) 自动寻找 vcvars64.bat
set "VCVARS="
for %%Y in (18 2022 2019 17 16) do (
    for %%E in (Community Professional Enterprise BuildTools Preview) do (
        set "P=%ProgramFiles%\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
        if exist "!P!" if not defined VCVARS set "VCVARS=!P!"
        set "Q=%ProgramFiles(x86)%\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
        if exist "!Q!" if not defined VCVARS set "VCVARS=!Q!"
    )
)

if defined VCVARS (
    echo 使用 MSVC: !VCVARS!
    call "!VCVARS!" >nul
    cl /nologo /O2 /utf-8 diyu.c /Fe:diyu.exe
    goto :done
)

rem 3) MinGW / GCC
where gcc >nul 2>nul
if %errorlevel%==0 (
    echo 使用 GCC
    gcc -O2 -std=c99 diyu.c -o diyu.exe
    goto :done
)

echo [失败] 没找到 cl.exe 或 gcc.exe。
echo 请安装 Visual Studio（含 C++ 生成工具）或 MinGW，或运行 make。
exit /b 1

:done
if exist diyu.exe (
    echo.
    echo [成功] 已生成 diyu.exe，运行 run.bat 即可开玩。
) else (
    echo [失败] 编译未生成可执行文件，请查看上方报错。
    exit /b 1
)
endlocal
