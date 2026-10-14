# Скачать версию 0.4

Пакеты собраны из исходников коммита `cdad6b6`.

- [Windows 10/11 x64](PixelSandbox-v0.4-Windows-x64.zip): распакуйте весь архив и запустите `StartGame.bat`. Для проверки используйте `CheckGame.bat`.
- [Linux x86_64](PixelSandbox-v0.4-Linux-x64.zip): установите SDL2 runtime, распакуйте архив, выполните `chmod +x StartGame.sh PixelSandbox`, затем `./StartGame.sh`.
- [Исходники](StartApp2-v0.4-source.zip): код движка, игры, тесты и инструкции сборки.

Linux: `sudo apt install libsdl2-2.0-0` для Ubuntu/Debian. Нужны графический сеанс и glibc 2.31+.

В игре: A/D — ходьба, пробел — прыжок, 1–6 — материалы, мышь — кисть, F — ускорение, F5 — сохранение, F9 — загрузка.

Четыре набора тестов прошли на Linux. Windows-пакет собран кросс-компилятором; запуск на настоящей Windows пока не подтверждён.
Контрольные суммы архивов находятся в [SHA256SUMS.txt](SHA256SUMS.txt).
