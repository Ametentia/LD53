@echo off
setlocal

if not exist "..\build" (mkdir "..\build")

pushd "..\build" > nul

set compiler_flags=-nologo -Od -Zi -I"include"
set linker_flags=-incremental:no -libpath:. xid.lib

del ludum_*.pdb

rem build ludum.dll
rem
cl %compiler_flags% -LD "..\code\ludum.c" -Fe"ludum.dll" -link %linker_flags% -pdb:ludum_%random%.pdb

rem build ludum.exe
rem
cl %compiler_flags% "..\code\ludum_main.c" -Fe"ludum.exe" -link %linker_flags%

popd > nul

endlocal
