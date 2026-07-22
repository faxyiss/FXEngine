# ===========================================================================
# FXTests - motor cekirdeginin birim testleri (0.5)
#
# SADECE Engine test ediliyor, Editor degil: editor bir GUI uygulamasi ve
# onu birim testiyle surmek pahali. Editor tarafi ekran goruntusu + elle
# dogrulama ile test ediliyor (Notes/00-Durum-ve-Plan.md).
#
# Testler PENCERE VE OpenGL BAGLAMI OLMADAN calismak zorunda: bu yuzden
# doku yukleyen yollar test edilmiyor, sadece veri ve dosya katmani.
# ===========================================================================

add_executable(FXTests
    TestMain.cpp
    UUID.test.cpp
    Scene.test.cpp
    SceneSerializer.test.cpp
    ComponentMeta.test.cpp
    EntitySnapshot.test.cpp
    DeferredDestroy.test.cpp
    Duplicate.test.cpp
    ChildOrder.test.cpp
    Prefab.test.cpp
    AssetManager.test.cpp
    Project.test.cpp
    Input.test.cpp
    Script.test.cpp
    TestSupport.h)

target_link_libraries(FXTests PRIVATE FXEngine Catch2::Catch2WithMain)
target_include_directories(FXTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ctest'in testleri tek tek gormesi icin (Catch2'nin kendi entegrasyonu).
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(FXTests)
