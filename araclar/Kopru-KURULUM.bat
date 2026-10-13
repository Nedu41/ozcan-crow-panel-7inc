@echo off
setlocal
REM CrowPanel HUB - yapay zeka efekt koprusu kurulumu (Windows). Cift tikla.
REM Yapar: Python kontrol/kurulum, numpy+pillow, Windows acilisinda otomatik baslatma, hemen baslatma.
cd /d "%~dp0"
echo.
echo === CrowPanel efekt koprusu kurulumu ===
echo.

set PY=
where py >nul 2>nul && set PY=py -3
if not defined PY ( where python >nul 2>nul && set PY=python )
if not defined PY (
  echo Python bulunamadi, kuruluyor ^(internet gerekir^)...
  winget install -e --id Python.Python.3.12 --accept-package-agreements --accept-source-agreements
  echo.
  echo Python kuruldu. Bu pencereyi kapatip KURULUM.bat'i BIR KEZ DAHA cift tiklayin.
  pause
  exit /b 0
)

echo Python: %PY%
%PY% -m pip install --quiet --upgrade pip
%PY% -m pip install --quiet numpy pillow
if errorlevel 1 (
  echo HATA: numpy/pillow kurulamadi. Internet baglantisini kontrol edin.
  pause
  exit /b 1
)

REM Acilista otomatik baslat (Baslangic klasorune kisayol)
set STARTUP=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup
set PYW=pythonw
where pyw >nul 2>nul && set PYW=pyw -3
> "%STARTUP%\CrowPanelEfekt.bat" echo @echo off
>> "%STARTUP%\CrowPanelEfekt.bat" echo cd /d "%~dp0"
>> "%STARTUP%\CrowPanelEfekt.bat" echo start "" /min %PYW% "%~dp0efekt_koprusu.py"
echo Otomatik baslatma eklendi.

REM Simdi baslat (eskisi calisiyorsa once kapat)
powershell -NoProfile -Command "Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*efekt_koprusu.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }" >nul 2>nul
start "" /min %PYW% "%~dp0efekt_koprusu.py"

echo.
echo Hazir. Onemli:
echo  - Windows guvenlik duvari soru sorarsa "Ozel aglara izin ver" deyin.
echo  - Bilgisayar acik ve ekranla ayni WiFi/agda olmali.
echo  - Ekranda Ayar ^> Yapay zeka anahtari: ucretsiz Gemini anahtarini girin ^(aistudio.google.com/apikey^).
echo  - Efekt sekmesinde "Kopru bagli" yazisi gorunmeli ^(10-20 sn surebilir^).
echo.
pause
