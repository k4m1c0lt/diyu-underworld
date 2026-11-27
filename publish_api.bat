@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo ============================================
echo   Publish via GitHub REST API
echo   (use this when git push to github.com is blocked)
echo ============================================
echo.
echo A Personal Access Token is required. It will NOT be echoed or saved.
echo Recommended: classic PAT with repo scope, revoke it after publishing.
echo.
powershell -NoProfile -ExecutionPolicy Bypass -Command "$s = Read-Host 'GitHub PAT' -AsSecureString; $env:GITHUB_TOKEN = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($s)); python .\publish_api.py"
echo.
pause
