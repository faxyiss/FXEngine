# ===========================================================================
# KURULU dagitimin motor tarifi (sdk/FXEngineConfig.cmake olarak kurulur).
#
# Gelistirme agacindaki karsiligi Engine/CMakeLists.txt'nin file(GENERATE)
# ile urettigi FXEngineConfig-<Config>.cmake'tir - o build agacindaki mutlak
# yollari tasir. Bu dosya ise TASINABILIR: her yol kendi konumuna
# (CMAKE_CURRENT_LIST_DIR) gore cozulur, kurulum klasoru nereye kopyalanirsa
# kopyalansin calisir.
#
# Kurulu dagitim yalnizca Release tasir; oyun projeleri de Release derlenir
# (FX_RELEASE tanimi + /MD calisma zamani eslesir).
# ===========================================================================

get_filename_component(FX_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

add_library(FXEngine SHARED IMPORTED GLOBAL)

# Motor basliklarinin kullandigi header-only bagimliliklar SDK ile birlikte
# tasinir (glm, EnTT, nlohmann_json). SDL/glad public basliklara sizmadigi
# icin gerekmez.
set(FXENGINE_INCLUDE_DIRS
    "${FX_SDK_DIR}/include"
    "${FX_SDK_DIR}/third_party/glm"
    "${FX_SDK_DIR}/third_party/entt"
    "${FX_SDK_DIR}/third_party/json")

set_target_properties(FXEngine PROPERTIES
    IMPORTED_LOCATION             "${FX_SDK_DIR}/../bin/FXEngine.dll"
    IMPORTED_IMPLIB               "${FX_SDK_DIR}/lib/FXEngine.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${FXENGINE_INCLUDE_DIRS}"
    INTERFACE_COMPILE_DEFINITIONS "FX_RELEASE")

set(FXENGINE_BIN_DIR "${FX_SDK_DIR}/../bin")
