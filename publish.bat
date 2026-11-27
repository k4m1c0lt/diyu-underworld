@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo ============================================
echo   发布 diyu-underworld 到 GitHub
echo ============================================
echo.

set "GHUSER=%~1"
if "%GHUSER%"=="" (
    set "GHUSER=k4m1c0lt"
    set /p "GHUSER=请输入 GitHub 用户名（直接回车用 k4m1c0lt）: "
)
if "%GHUSER%"=="" (
    echo 未输入用户名，已取消。
    exit /b 1
)

set "REPO=diyu-underworld"
echo 目标仓库: https://github.com/%GHUSER%/%REPO%
echo.

rem 有 GitHub CLI 就自动建公开仓库并推送
where gh >nul 2>nul
if %errorlevel%==0 (
    echo 检测到 GitHub CLI，正在创建公开仓库并推送...
    gh repo create %REPO% --public --source=. --remote=origin --push
    goto :done
)

echo 未检测到 gh。请先在 GitHub 网页新建一个空仓库：
echo     https://github.com/new
echo     仓库名 %REPO% / Public / 不要勾选 README、LICENSE、.gitignore
echo.
pause

git remote remove origin >nul 2>nul
git remote add origin https://github.com/%GHUSER%/%REPO%.git
git branch -M main
git push -u origin main

:done
if %errorlevel%==0 (
    echo.
    echo [完成] https://github.com/%GHUSER%/%REPO%
) else (
    echo.
    echo [失败] 请检查网络，或按 Git Credential Manager 弹窗完成 GitHub 登录。
)
endlocal
