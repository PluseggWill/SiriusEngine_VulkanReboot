@echo off
REM VS Custom Build entry — compiles GLSL sources to Shader_Generated\*.spv.
call "%~dp0CompileShader_Glslc.bat"
exit /b %ERRORLEVEL%
