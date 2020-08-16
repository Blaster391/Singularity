@echo off
set outputDir=%1
echo %outputDir%

set compiler=D:\Vulkan\1.2.148.1\Bin\glslc.exe
echo %compiler%

:: Clean up old shaders
if exist %outputDir%\Vertex\ rmdir %outputDir%\Vertex\ /q /s
if exist %outputDir%\Fragment\ rmdir %outputDir%\Fragment\ /q /s

:: Remake directories
mkdir %outputDir%
mkdir %outputDir%\Vertex\
mkdir %outputDir%\Fragment\

:: Run the compiler for each file in the directories
for /f "delims=|" %%f in ('dir /b Vertex') do %compiler% Vertex\%%f -o %outputDir%\Vertex\%%~nf_vert.spv
for /f "delims=|" %%f in ('dir /b Fragment') do %compiler% Fragment\%%f -o %outputDir%\Fragment\%%~nf_frag.spv