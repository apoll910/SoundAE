@echo off
echo =====================================================================
echo    Kompilator SoundKey After Effects Native C++ Plugin (Windows x64)
echo =====================================================================
echo.

:: 1. Sprawdzenie sciezki do Adobe After Effects SDK
if "%AESDK_ROOT%"=="" (
    echo [INFORMACJA] Zmienna AESDK_ROOT nie jest ustawiona.
    echo Uzywam domyslnej sciezki: C:\AdobeAfterEffectsSDK
    set AESDK_ROOT=C:\AdobeAfterEffectsSDK
) else (
    echo [OK] Uzywam After Effects SDK: %AESDK_ROOT%
)

:: 2. Utworzenie katalogu wyjsciowego build
if not exist build (
    mkdir build
)

cd build

:: 3. Konfiguracja projektu przez CMake dla Visual Studio 2022
echo [1/3] Generowanie rozwiazania Visual Studio x64...
cmake -G "Visual Studio 17 2022" -A x64 -DAE_SDK_PATH="%AESDK_ROOT%" ..
if %ERRORLEVEL% NEQ 0 (
    echo [PROBA] Proba uzycia Visual Studio 2019...
    cmake -G "Visual Studio 16 2019" -A x64 -DAE_SDK_PATH="%AESDK_ROOT%" ..
    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo [BLAD] Nie odnaleziono Visual Studio 2022 ani 2019 z narzedziami C++ x64.
        echo Zainstaluj 'Desktop development with C++' w Visual Studio Installer.
        pause
        exit /b 1
    )
)

:: 4. Kompilacja binarna pliku SoundKey.aex
echo.
echo [2/3] Kompilacja SoundKey.aex w trybie Release...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo [BLAD] Kompilacja zakonczona bledem. Sprawdz logi powyzej.
    pause
    exit /b 1
)

:: 5. Potwierdzenie sukcesu
echo.
echo =====================================================================
echo [SUKCES] Plik SoundKey.aex zostal pomyslnie wygenerowany!
echo Lokalizacja: %CD%\Release\SoundKey.aex
echo.
echo Aby zainstalowac plugin, skopiuj go do:
echo C:\Program Files\Adobe\Adobe After Effects [Wersja]\Support Files\Plug-ins\
echo =====================================================================
pause
