# Windows x64 paketleme

Bu depo Qt/C++ uygulamasıdır. PyInstaller kullanılmaz.

## Gereksinimler

- Visual Studio 2022 Build Tools (MSVC)
- CMake + Ninja
- Qt 6.5+ (`win64_msvc2022_64`)
- Inno Setup 6

## Yerel derleme

```powershell
$env:QT_PREFIX = "C:\Qt\6.8.1\msvc2022_64"
.\packaging\windows\build.ps1
```

Çıktılar:

- `build-windows/TicketGoIzleme-Windows-x64-Setup.exe`
- `build-windows/TicketGoIzleme-update-manifest.windows.json`

Kurulum yönetici gerektirmez (`PrivilegesRequired=lowest`). Kullanıcı verisi `%AppData%\TicketGoTeknoloji\SistemIzleme` altında korunur.

## Güncelleme

Windows istemcisi yalnızca `TicketGoIzleme-Windows-x64-Setup.exe` paketini seçer.
macOS istemcisi yalnızca `TicketGoIzleme-macOS-*.zip` paketlerini seçer.
Manifestte `platform` alanı önerilir; eski macOS kayıtlarında alan yoksa macos kabul edilir.

Kod imzalama sertifikası yoksa paket **imzasız**dır; SmartScreen uyarısı görülebilir.
Sertifika yalnızca GitHub Secrets üzerinden kullanılmalıdır; kaynak koda gömülmez.

CI artifact adı: `TicketGoIzleme-Windows-x64`
(`Setup.exe`, `.sha256`, `TicketGoIzleme-update-manifest.windows.json`)

Mevcut macOS Release varlıkları bu workflow tarafından silinmez veya ezilmez.
