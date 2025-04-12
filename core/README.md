## Сборка под Windows
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
```sh
cmake --preset=win-release
```
И наконец сборка
```sh
cmake --build build/win-release
```
И сгенерить питоновское колесо:
```sh
python setup.py bdist_wheel
```
Свеженькае колесо будет в папке дистрибутивов питона:
```sh
$ ls dist/
pycore-0.1.0-cp313-cp313-win_amd64.whl
```
Установка из колеса (в своём виртуальном окружении):
```sh
pip install pycore-0.1.0-cp313-cp313-win_amd64.whl
```
Пример кода:
`example.py`
```py

```
