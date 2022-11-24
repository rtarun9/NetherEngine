powershell -Command "Invoke-WebRequest -Uri https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.602.0 -OutFile agility.zip"
powershell -Command "& {Expand-Archive agility.zip bin/agilitySDK/}"

xcopy bin\agilitySDK\build\native\bin\x64\* bin\Debug\D3D12\
xcopy bin\agilitySDK\build\native\bin\x64\* bin\Release\D3D12\