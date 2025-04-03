Сборка на винде:
- Поставить MSVC
- Активировать окружение в powershell
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
```
Если окружение не хочется активировать каждый раз, можно пожертвовать временем запуска powershell
и добавить эту команду в профиль:
```powershell
if (!(Test-Path $PROFILE)) { New-Item -ItemType File -Path $PROFILE -Force }
notepad $PROFILE
```
Теперь можно сконфигурировать проект под винду через пресет:
```powershell
cmake --preset=win-release
```
И наконец сборка
```powershell
cmake --build build/win-release
```