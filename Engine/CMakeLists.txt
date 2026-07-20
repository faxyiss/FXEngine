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
    src/Scene/Entity.cpp
    src/Scene/Systems.cpp
    src/Scene/SceneSerializer.cpp
)

set(FXENGINE_HEADERS
    include/FXEngine/Core/Version.h
    include/FXEngine/Core/Log.h
    include/FXEngine/Core/Window.h
    include/FXEngine/Core/Application.h
    include/FXEngine/Core/FileSystem.h
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
    include/FXEngine/Scene/Entity.h
    include/FXEngine/Scene/Scene.h
    include/FXEngine/Scene/Systems.h
    include/FXEngine/Scene/SceneSerializer.h
)

add_library(FXEngine STATIC
    ${FXENGINE_SOURCES}
    ${FXENGINE_HEADERS})

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
