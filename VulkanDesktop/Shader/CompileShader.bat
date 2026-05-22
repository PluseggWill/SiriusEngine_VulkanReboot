@echo off
REM VS Custom Build entry — compiles GLSL sources to TriangleVert.spv / TrianglePix.spv.
call "%~dp0CompileShader_Glslc.bat"
exit /b %ERRORLEVEL%
