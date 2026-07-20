# Faz 0 — CMake, klasör yapısı, bağımlılıklar

## Amaç
Tek bir soruya "evet" dedirtmek: **7 bağımlılık + Engine kütüphanesi derleniyor ve link oluyor mu?**
Pencere yok, OpenGL çağrısı yok. Sadece build zinciri.

## Öğrenilecek kavramlar
- **STATIC library vs executable**: Engine'i kütüphane yapmak, mimari kuralı linker seviyesinde zorunlu kılar.
- **PUBLIC / PRIVATE / INTERFACE**: CMake "usage requirements".
  - `PUBLIC`  → hem ben kullanırım hem bana link olan
  - `PRIVATE` → sadece ben kullanırım (stb böyle)
  - `INTERFACE` → ben derlenmem, sadece bana link olan kullanır (header-only)
- **FetchContent** → configure zamanında kaynak çeker.
- **glad her zaman diğer GL header'larından önce include edilir.**

## Bağımlılık sürümleri
| Kütüphane | Sürüm | Tip |
|---|---|---|
| SDL3 | release-3.2.10 | shared lib |
| glm | 1.0.1 | header-only |
| EnTT | v3.13.2 | header-only |
| nlohmann/json | v3.11.3 | header-only |
| glad | v0.1.36 (gl=3.3 core) | üretilen C kodu, Python gerekir |
| Dear ImGui | docking branch | elle kurulan static lib |
| stb | master | header-only |

## Ortam
- MSVC 14.50 (VS 2026 Community) ✅
- Python 3.10 ✅ (glad üreteci için)
- CMake ❌ → kurulması gerekti

## Karşılaşılan hatalar

### 1. `cmake: command not found`
CMake aslında kuruluydu (4.4.0), sadece açık olan terminalin PATH'i eskiydi.
→ Terminali yeniden aç.

### 2. `Generator "Visual Studio 18 2026" could not find any instance of Visual Studio`
Kök sebep: VS kurulumu yarım.
- `vswhere` → `"isComplete": false`
- `VC\Auxiliary\Build\` içinde **`vcvarsall.bat` yok** (sadece `vcvars64.bat` var)
- Ama `cl.exe`, `link.exe`, `nmake.exe` mevcut (MSVC 14.50.35717)

→ Çözüm: VS Installer → Repair. Sonra "Desktop development with C++"
  workload'ı + "C++ CMake tools for Windows" bileşeni.

### 3. CMake 4.x + eski bağımlılıklar
CMake 4, `cmake_minimum_required(VERSION 3.0)` yazan projeleri reddediyor.
**glad v0.1.36** bu duruma giriyor.
→ ÇÖZÜLDÜ: `cmake/Dependencies.cmake` içine kalıcı olarak eklendi.
  Artık komut satırında bayrak vermeye gerek yok.

PowerShell notu: `-DX=3.5` tırnaksız yazılırsa PowerShell `.5`'i ayrı argüman
sanıyor. Komut satırında verilecekse her zaman tırnak içinde.

## Doğrulanmış build komutları
```powershell
cd C:\FXEngine
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
.\build\bin\FXEditor.exe
```
İlk configure ~3.5 dk (bağımlılık indirme), sonrakiler saniyeler.

## Onay
- [x] Derlendi (FXEngine.lib + FXEditor.exe)
- [x] Çalıştı, çıktı doğru
- [ ] Kullanıcı onayı → Faz 1'e geçiş
