# ===========================================================================
# Tum 3rd-party bagimliliklar burada tanimlanir.
#
# FetchContent: CMake configure asamasinda kaynak kodu git'ten ceker ve
# projeye dahil eder. Ilk configure yavas (indirme), sonrakiler cache'li.
# ===========================================================================
include(FetchContent)

# CMake 4.x, `cmake_minimum_required(VERSION 3.0)` yazan eski projeleri
# reddediyor. Bagimliliklarimizdan glad v0.1.36 ve glm 1.0.1 bu duruma giriyor.
# Bu degisken CMake'e "eski policy surumlerini kabul et" der. Bagimliliklar
# yukseltilene kadar gerekli - kendi kodumuzu etkilemez.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

# Bagimliliklarin kaynak kodu build/ icine degil, proje kokunde _deps/ icine
# insin. Boylece build klasorunu silsen bile tekrar indirmek zorunda kalmazsin.
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/_deps" CACHE PATH "" FORCE)

# ---------------------------------------------------------------------------
# 1) SDL3 - pencere, input, olay dongusu
# ---------------------------------------------------------------------------
# SDL'in kendi testleri/orneklerini derlemesini engelliyoruz (build suresi).
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL_SHARED       ON  CACHE BOOL "" FORCE)
set(SDL_STATIC       OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.10
    GIT_SHALLOW    TRUE)

# ---------------------------------------------------------------------------
# 2) glm - matematik (vec2/vec3/mat4). Header-only.
# ---------------------------------------------------------------------------
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE)

# ---------------------------------------------------------------------------
# 3) EnTT - ECS. Header-only.
# ---------------------------------------------------------------------------
FetchContent_Declare(EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.2
    GIT_SHALLOW    TRUE)

# ---------------------------------------------------------------------------
# 4) nlohmann/json - serilestirme. Header-only.
# ---------------------------------------------------------------------------
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE)

# ---------------------------------------------------------------------------
# 5) glad - OpenGL fonksiyon isaretcilerini yukler
# ---------------------------------------------------------------------------
# glad'in kendi CMake'i var ama .c/.h dosyalarini Python ile URETIR.
# Bu yuzden makinede Python gerekiyor. Asagida GL 3.3 Core istiyoruz.
set(GLAD_PROFILE     "core"    CACHE STRING "" FORCE)
set(GLAD_API         "gl=3.3"  CACHE STRING "" FORCE)
set(GLAD_GENERATOR   "c"       CACHE STRING "" FORCE)
set(GLAD_INSTALL     OFF       CACHE BOOL   "" FORCE)

# UZANTILAR (extensions) - onemli bir ayrim:
# "gl=3.3" sadece CEKIRDEK 3.3 API'sini uretir. glDebugMessageCallback gibi
# fonksiyonlar cekirdekte DEGIL, GL_KHR_debug UZANTISINDA yasar (GL 4.3'te
# cekirdege alinmis, ama biz 3.3 hedefliyoruz). Istemezsek uretilmez.
# Bos birakmak "hicbir uzanti" demektir, "hepsi" degil.
set(GLAD_EXTENSIONS "GL_KHR_debug" CACHE STRING "" FORCE)

FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v0.1.36
    GIT_SHALLOW    TRUE)

# ---------------------------------------------------------------------------
# 6) Dear ImGui (docking branch) - editor UI
# ---------------------------------------------------------------------------
# ImGui'nin CMakeLists'i YOK. Sadece kaynagi cekiyoruz, target'i biz kuruyoruz.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE)

# ---------------------------------------------------------------------------
# 7) stb - stb_image.h (texture yukleme)
# ---------------------------------------------------------------------------
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)

# ---------------------------------------------------------------------------
# Indirme + dahil etme
# ---------------------------------------------------------------------------
message(STATUS "[FXEngine] Bagimliliklar cekiliyor (ilk sefer birkac dakika surebilir)...")
FetchContent_MakeAvailable(SDL3 glm EnTT nlohmann_json glad)

# imgui ve stb icin: sadece indir, add_subdirectory YAPMA (CMakeLists'leri yok).
FetchContent_MakeAvailable(imgui stb)

# ---------------------------------------------------------------------------
# imgui target'ini elle kuruyoruz
# ---------------------------------------------------------------------------
# Neden STATIC lib? Cunku imgui .cpp dosyalari her seferinde yeniden derlenmesin,
# ve Engine/Editor sadece link etsin diye.
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)

# imgui_impl_sdl3.cpp icinde <SDL3/SDL.h> var -> SDL3'e link etmeli.
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# ---------------------------------------------------------------------------
# stb target'i (header-only => INTERFACE)
# ---------------------------------------------------------------------------
# INTERFACE library derlenmez; sadece "bana link edersen su include yolunu
# alirsin" der. Header-only kutuphaneler icin dogru arac budur.
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

message(STATUS "[FXEngine] Bagimliliklar hazir.")
