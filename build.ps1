$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$compiler=Join-Path $PSScriptRoot '.tools\llvm-mingw-20261020-ucrt-x86_64\bin\clang++.exe'
if(!(Test-Path $compiler)){throw 'Install llvm-mingw 20261020 in .tools, or use CMake with MSVC.'}
New-Item -ItemType Directory -Force build | Out-Null
& $compiler -std=c++20 -O2 -static -Wall -Wextra -Ithird_party src\recorder.cpp -o build\recorder.exe -luser32
if($LASTEXITCODE -ne 0){throw 'Build failed'}
& '.\build\recorder.exe' --self-test
if($LASTEXITCODE -ne 0){throw 'Tests failed'}
foreach($testName in @('projection_tests','placement_geometry_tests','placement_binding_tests','strategy_recording_tests','autoplay_policy_tests','road_navigation_tests')){
 & $compiler -std=c++20 -O2 -static -Wall -Wextra -Isrc -Ithird_party ('tests\'+$testName+'.cpp') -o ('build\'+$testName+'.exe')
 if($LASTEXITCODE -ne 0){throw ('Test build failed: '+$testName)}
 & ('.\build\'+$testName+'.exe')
 if($LASTEXITCODE -ne 0){throw ('Tests failed: '+$testName)}
}
& $compiler -std=c++20 -O2 -static -municode -mwindows src\launcher.cpp -o AnimeMacro.exe -luser32
if($LASTEXITCODE -ne 0){throw 'Launcher build failed'}
Copy-Item -LiteralPath AnimeMacro.exe -Destination Starshine.exe -Force
