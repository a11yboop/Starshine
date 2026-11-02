@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Replay.ps1" -WaitForMatch -Strategy

