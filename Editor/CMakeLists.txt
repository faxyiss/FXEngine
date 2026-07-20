# ===========================================================================
# FXEditor - editor uygulamasi (executable)
#
# Engine'e baglidir. Engine ise Editor'u bilmez -> tek yonlu bagimlilik.
# ===========================================================================

add_executable(FXEditor
    src/main.cpp
    src/EditorApp.cpp
    src/EditorApp.h
    src/ImGuiLayer.cpp
    src/ImGuiLayer.h
    src/EditorCamera.cpp
    src/EditorCamera.h
    src/Panels/SceneHierarchyPanel.cpp
    src/Panels/SceneHierarchyPanel.h
    src/Panels/ContentBrowserPanel.cpp
    src/Panels/ContentBrowserPanel.h
    src/Platform/FileDialogs.cpp
    src/Platform/FileDialogs.h)

target_include_directories(FXEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

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
