#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <string>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
 wchar_t path[32768];auto n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768)return 1;
 auto root=std::filesystem::path(path).parent_path();auto script=root/L"Launcher.ps1";
 std::wstring command=L"powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File \""+script.wstring()+L"\"";
 STARTUPINFOW info{};info.cb=sizeof(info);info.dwFlags=STARTF_USESHOWWINDOW;info.wShowWindow=SW_SHOWNORMAL;PROCESS_INFORMATION process{};
 if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.c_str(),&info,&process)){MessageBoxW(nullptr,L"Cannot start Launcher.ps1",L"Starshine",MB_ICONERROR);return 1;}
 CloseHandle(process.hThread);CloseHandle(process.hProcess);return 0;
}

