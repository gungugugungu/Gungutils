@echo off
setlocal enabledelayedexpansion
echo starting compilation

for /r %%f in (*.glsl) do (
    echo -----
    rem
    set "output_file=%%f.h"

    echo %%f -^> !output_file!
    sokol-shdc.exe --input "%%f" --output "!output_file!" --slang glsl430
)

echo finished compilation