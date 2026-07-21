# ===========================================================================
# FXEditor - editor uygulamasi (executable)
#
# Engine'e baglidir. Engine ise Editor'u bilmez -> tek yonlu bagimlilik.
# ===========================================================================

add_executable(FXEditor
    src/main.cpp
    # EditorApp tek sinif, dort dosyaya bolunmus tanimlari:
    # cekirdek yasam dongusu / proje-dosya islemleri / paneller / viewport
    src/EditorApp.cpp
    src/EditorApp_Project.cpp
    src/EditorApp_UI.cpp
    src/EditorApp_Viewport.cpp
    src/EditorApp.h
    src/SampleScene.cpp
    src/SampleScene.h
    src/ImGuiLayer.cpp
    src/ImGuiLayer.h
    src/EditorCamera.cpp
    src/EditorCamera.h
    src/Game/GameProject.cpp
    src/Game/GameProject.h
    src/Game/GameLibrary.cpp
    src/Game/GameLibrary.h
    src/Panels/ComponentDrawer.cpp
    src/Panels/ComponentDrawer.h
    src/Panels/SceneHierarchyPanel.cpp
    src/Panels/SceneHierarchyPanel.h
    src/Panels/ContentBrowserPanel.cpp
    src/Panels/ContentBrowserPanel.h
    src/Platform/FileDialogs.cpp
    src/Platform/FileDialogs.h)

# ---------------------------------------------------------------------------
# Script kayitlari (A4)
# ---------------------------------------------------------------------------
# Scripts/ altindaki her <Ad>.h icin bir include ve bir Register satiri
# uretiliyor. Elle yazsaydik yeni bir script yazan kisi kayit satirini
# unutabilirdi ve hata SESSIZ olurdu: script Inspector listesinde hic
# gorunmezdi.
#
# CONFIGURE_DEPENDS: yeni bir dosya eklendiginde derleme sirasinda
# yeniden taraniyor. GLOB'un normalde tehlikeli olmasinin sebebi
# (yeni dosyayi fark etmemesi) boylece ortadan kalkiyor.
file(GLOB FXEDITOR_SCRIPT_HEADERS CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/Scripts/*.h")

set(FX_SCRIPT_INCLUDES "")
set(FX_SCRIPT_REGISTRATIONS "")

foreach(script_header ${FXEDITOR_SCRIPT_HEADERS})
    get_filename_component(script_name ${script_header} NAME_WE)
    string(APPEND FX_SCRIPT_INCLUDES "#include \"Scripts/${script_name}.h\"
")
    string(APPEND FX_SCRIPT_REGISTRATIONS
           "        FX::ScriptRegistry::Register<FXEd::${script_name}>(\"${script_name}\");
")
endforeach()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Scripts/ScriptRegistrations.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ScriptRegistrations.h"
    @ONLY)

target_sources(FXEditor PRIVATE ${FXEDITOR_SCRIPT_HEADERS})

# "Yeni Script" bu klasore yaziyor. KAYNAK agacindaki yol - uretilen
# dosyanin derlenebilmesi icin kaynakta durmasi gerekiyor.
target_compile_definitions(FXEditor PRIVATE
    FX_EDITOR_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/Scripts"
    # B-2: oyun projesi motoru buradan buluyor. Editor gelistirme
    # zamaninda calisiyor ve zaten derleyici sart kosuyor; kurulu bir
    # dagitimda bunun yerine install agaci gecer.
    FX_ENGINE_BUILD_DIR="${CMAKE_BINARY_DIR}"
    FX_BUILD_CONFIG="$<CONFIG>")

target_include_directories(FXEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated)

# FXEngine'i PUBLIC olarak link ettigimiz her sey (SDL3, glm, EnTT, json, glad)
# buraya otomatik akar. Bu, CMake'in "usage requirements" mekanizmasidir:
# bagimliliklari tek tek tekrar yazmak zorunda kalmayiz.
# imgui'yi ayrica ekliyoruz cunku o Engine'in degil, EDITOR'un araci.
target_link_libraries(FXEditor PRIVATE
    FXEngine
    imgui
    ImGuizmo)

# ---------------------------------------------------------------------------
# Windows: SDL3.dll'i exe'nin yanina kopyala
# ---------------------------------------------------------------------------
# SDL3'u paylasimli (shared) kutuphane olarak derliyoruz. Windows'ta exe,
# DLL'i kendi klasorunde veya PATH'te arar. Bu adim olmadan program
# "SDL3.dll bulunamadi" diye acilmaz. $<TARGET_RUNTIME_DLLS> CMake'e
# "bu exe'nin ihtiyac duydugu tum DLL'leri bul" der.
# comdlg32: GetOpenFileName/GetSaveFileName (yerel dosya diyaloglari).
# Sadece Windows'ta; FileDialogs.cpp diger platformlarda bos govde derliyor.
if(WIN32)
    target_link_libraries(FXEditor PRIVATE comdlg32 shell32)
endif()

if(WIN32)
    add_custom_command(TARGET FXEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:FXEditor> $<TARGET_FILE_DIR:FXEditor>
        COMMAND_EXPAND_LISTS)
endif()

# ---------------------------------------------------------------------------
# assets/ klasorunu exe'nin yanina kopyala
# ---------------------------------------------------------------------------
# Shader'lar calisma zamaninda dosyadan okunuyor. Program onlari CALISMA
# DIZININE gore arar - bu yuzden exe ile ayni yerde olmalilar.
#
# Neden shader'lari koda gomup bu derdi tamamen ortadan kaldirmiyoruz?
# Cunku dosyada olunca shader'i degistirip programi YENIDEN DERLEMEDEN
# calistirabilirsin. Grafik kodu yazarken bu cok zaman kazandirir.
#
# copy_directory_if_different: sadece degisen dosyalari kopyalar,
# her derlemede tum klasoru bosuna yazmaz.
add_custom_command(TARGET FXEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:FXEditor>/assets"
    COMMENT "assets/ kopyalaniyor")

# Shader dosyalari IDE'de gorunsun (derlenmezler, sadece listelenirler).
file(GLOB_RECURSE FXEDITOR_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
target_sources(FXEditor PRIVATE ${FXEDITOR_ASSETS})
set_source_files_properties(${FXEDITOR_ASSETS} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${FXEDITOR_ASSETS})
