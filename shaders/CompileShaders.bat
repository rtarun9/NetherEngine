WHERE dxc
IF %ERRORLEVEL% NEQ 0 ECHO DirectX Shader Compiler was not found. Consider installing it for shader compilation.


dxc -E VsMain -T vs_6_6 TriangleShader.hlsl -Fo TriangleShaderVS.cso
dxc -E PsMain -T ps_6_6 TriangleShader.hlsl -Fo TriangleShaderPS.cso