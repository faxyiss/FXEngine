# ===========================================================================
# FXEngine - motor cekirdegi (statik kutuphane)
#
# Bu target Editor'u HIC BILMEZ. Editor'e dair tek bir include, tek bir
# link satiri burada olmayacak. Mimari kural budur.
# ===========================================================================

# GLOB kullanmiyoruz: dosyalari elle listeliyoruz. Neden? GLOB'da yeni dosya
# ekleyince CMake bunu fark etmez ve "neden derlenmiyor?" diye saatler kaybedersin.
# Elle liste = ne derlendigini her zaman bilirsin.
set(FXENGINE_SOURCES
    src/Core/Version.cpp
    src/Core/Log.cpp
    src/Core/Window.cpp
    src/Core/Application.cpp
    src/Core/FileSystem.cpp
    src/Core/Project.cpp
    src/Core/FileWatcher.cpp
    src/Core/Input.cpp
    src/Events/EventTranslator.cpp
    src/Asset/AssetManager.cpp
    src/Core/UUID.cpp
    src/Renderer/Shader.cpp
    src/Renderer/Buffer.cpp
    src/Renderer/VertexArray.cpp
    src/Renderer/RenderCommand.cpp
    src/Renderer/Texture.cpp
    src/Renderer/OrthographicCamera.cpp
    src/Renderer/Renderer2D.cpp
    src/Renderer/Framebuffer.cpp
    src/Renderer/TextureLibrary.cpp
    src/Scene/Scene.cpp
    src/Scene/ComponentMeta.cpp
    src/Scene/Entity.cpp
    src/Scene/Systems.cpp
    src/Scene/ScriptRegistry.cpp
    src/Scene/EntitySerialization.cpp
    src/Scene/SceneSerializer.cpp
    src/Scene/PrefabSerializer.cpp
)

set(FXENGINE_HEADERS
    include/FXEngine/Core/Version.h
    include/FXEngine/Core/Log.h
    include/FXEngine/Core/Window.h
    include/FXEngine/Core/Application.h
    include/FXEngine/Core/FileSystem.h
    include/FXEngine/Core/Project.h
    include/FXEngine/Core/FileWatcher.h
    include/FXEngine/Core/KeyCodes.h
    include/FXEngine/Core/Input.h
    include/FXEngine/Events/Event.h
    include/FXEngine/Events/KeyEvent.h
    include/FXEngine/Events/MouseEvent.h
    include/FXEngine/Events/WindowEvent.h
    src/Events/EventTranslator.h
    include/FXEngine/Asset/Asset.h
    include/FXEngine/Asset/AssetManager.h
    include/FXEngine/Core/UUID.h
    include/FXEngine/Renderer/Shader.h
    include/FXEngine/Renderer/Buffer.h
    include/FXEngine/Renderer/VertexArray.h
    include/FXEngine/Renderer/RenderCommand.h
    include/FXEngine/Renderer/Texture.h
    include/FXEngine/Renderer/OrthographicCamera.h
    include/FXEngine/Renderer/Renderer2D.h
    include/FXEngine/Renderer/Framebuffer.h
    include/FXEngine/Renderer/TextureLibrary.h
    include/FXEngine/Scene/Components.h
    include/FXEngine/Scene/ComponentMeta.h
    include/FXEngine/Scene/Entity.h
    include/FXEngine/Scene/Scene.h
    include/FXEngine/Scene/Systems.h
    include/FXEngine/Scene/ScriptableEntity.h
    include/FXEngine/Scene/ScriptRegistry.h
    include/FXEngine/Scene/SceneSerializer.h
    include/FXEngine/Scene/PrefabSerializer.h
    src/Scene/EntitySerialization.h
)

add_library(FXEngine SHARED
    ${FXENGINE_SOURCES}
    ${FXENGINE_HEADERS})

# SHARED olmasinin sebebi Asama B: oyun kodu ayri bir DLL'e tasinacak ve
# motorun ScriptRegistry / ComponentMeta / FileSystem / Project / Renderer2D
# durumlari surec genelinde TEK olmak zorunda. Statik kutuphanede exe ve
# Game.dll bu durumlarin ayri kopyalarini tasirdi.
#
# WINDOWS_EXPORT_ALL_SYMBOLS: __declspec(dllexport) yazmadan tum fonksiyon
# sembollerini disa aktarir. VERI sembollerini kapsamaz; motorun paylasilan
# durumu zaten dosya kapsamli ve yalnizca fonksiyonlar uzerinden erisiliyor.
set_target_properties(FXEngine PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Editor'un IDE'de duzgun agac gormesi icin (kozmetik ama isi kolaylastirir)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${FXENGINE_SOURCES} ${FXENGINE_HEADERS})

# ---------------------------------------------------------------------------
# Include yollari
# ---------------------------------------------------------------------------
#  PUBLIC  include/ : bize link olan herkes (Editor) burayi gorur -> public API
#  PRIVATE src/     : sadece bizim .cpp'lerimiz gorur -> ic implementasyon
target_include_directories(FXEngine
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# Bagimliliklar
# ---------------------------------------------------------------------------
#  PUBLIC  = hem biz kullaniriz hem de header'larimizda gorunur, dolayisiyla
#            Editor de otomatik erisir (orn. glm tipleri Transform header'inda).
#  PRIVATE = sadece .cpp'lerimizde kullaniriz, Editor'e sizmaz (orn. stb_image).
target_link_libraries(FXEngine
    PUBLIC
        SDL3::SDL3
        glm::glm
        EnTT::EnTT
        nlohmann_json::nlohmann_json
        glad
    PRIVATE
        stb)

target_compile_features(FXEngine PUBLIC cxx_std_17)

# ---------------------------------------------------------------------------
# Derleme tanimlari (defines)
# ---------------------------------------------------------------------------
# FX_DEBUG makrosu: Log.h icindeki FX_ASSERT'in aktif olup olmadigini belirler.
# $<CONFIG:Debug> bir "generator expression"dir: Visual Studio gibi
# multi-config generator'larda hangi konfigurasyonun derlendigi ancak BUILD
# zamaninda bellidir, configure zamaninda degil. Bu yuzden `if(CMAKE_BUILD_TYPE)`
# ile yapamayiz - generator expression tam olarak bu sorunu cozer.
#
# PUBLIC: Editor'un de FX_ASSERT kullanabilmesi icin bu tanim ona da gecmeli.
target_compile_definitions(FXEngine PUBLIC
    $<$<CONFIG:Debug>:FX_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:FX_DEBUG>
    $<$<CONFIG:Release>:FX_RELEASE>)

# ---------------------------------------------------------------------------
# Oyun projesi icin motor tarifi (Asama B-2)
# ---------------------------------------------------------------------------
# Game.dll AYRI bir CMake projesi olarak, kullanicinin proje klasorunde
# derleniyor. Motora baglanmak icin include yollarina ve import
# kutuphanesine ihtiyaci var. Bunlari orada elle yazsaydik ikinci bir
# kopya olurdu ve er gec ayrisirdi; onun yerine hedeften OKUYUP bir
# dosyaya yaziyoruz.
#
# INCLUDE_DIRECTORIES (INTERFACE_ degil): tek transitif olarak hesaplanan
# olan bu - SDL3/glm/EnTT/json/glad'in yollari da icinde geliyor. Bedeli
# PRIVATE src/ yolunun da listeye girmesi; tuketen taraf onu eliyor.
#
# Dosya adinda $<CONFIG>: Debug ile Release'in FX_DEBUG'i ve .lib yolu
# farkli. Editor kendi config'ine ait olani secer.
file(GENERATE
    OUTPUT  "${CMAKE_BINARY_DIR}/FXEngineConfig-$<CONFIG>.cmake"
    CONTENT
"# URETILEN DOSYA - Engine/CMakeLists.txt tarafindan yazildi.
add_library(FXEngine SHARED IMPORTED GLOBAL)

set(FXENGINE_INCLUDE_DIRS \"$<TARGET_PROPERTY:FXEngine,INCLUDE_DIRECTORIES>\")
set(FXENGINE_DEFINITIONS  \"$<TARGET_PROPERTY:FXEngine,COMPILE_DEFINITIONS>\")

# src/ PRIVATE'ti, disari sizmasin. Bos girisler: kapali generator
# ifadelerinden kaliyor ve CMake bos include yolunu hata sayiyor.
list(FILTER FXENGINE_INCLUDE_DIRS EXCLUDE REGEX \"/Engine/src$\")
list(FILTER FXENGINE_INCLUDE_DIRS EXCLUDE REGEX \"^$\")
list(FILTER FXENGINE_DEFINITIONS  EXCLUDE REGEX \"^$\")

set_target_properties(FXEngine PROPERTIES
    IMPORTED_LOCATION             \"$<TARGET_FILE:FXEngine>\"
    IMPORTED_IMPLIB               \"$<TARGET_LINKER_FILE:FXEngine>\"
    INTERFACE_INCLUDE_DIRECTORIES \"\${FXENGINE_INCLUDE_DIRS}\"
    INTERFACE_COMPILE_DEFINITIONS \"\${FXENGINE_DEFINITIONS}\")

set(FXENGINE_BIN_DIR \"$<TARGET_FILE_DIR:FXEngine>\")
")
