#pragma once

// ===========================================================================
// FXEngine - SceneSerializer
//
// Sahneyi JSON'a yazar ve geri okur.
//
// BU FAZ, FAZ 5'TEKI "COMPONENT SADECE VERI OLMALI" KURALININ SINAVIDIR.
// Component'ler saf veri oldugu icin serilestirme MEKANIK bir istir:
// her alani yaz, geri okurken doldur. Icinde isaretciler, geri cagrimlar
// veya sistemlere referanslar olsaydi bu imkansiza yakin olurdu -
// bir std::function'i diske nasil yazarsin?
//
// Iyi bir veri modelinin testi, serilestirilebilir olmasidir.
//
// NEDEN AYRI BIR SINIF, Scene::Save() degil?
// Cunku sahne, "nasil saklandigini" bilmek zorunda degil. Yarin JSON
// yerine binary bir format istersek Scene'e hic dokunmayiz; ikinci bir
// serilestirici yazariz. Veri ile onun TEMSILI ayri sorumluluklardir.
// ===========================================================================

#include "FXEngine/Scene/Scene.h"
#include "FXEngine/Renderer/TextureLibrary.h"

#include <string>

namespace FX
{
    class SceneSerializer
    {
    public:
        // textureLibrary: yukleme sirasinda yollari texture'a cevirmek icin.
        // Serializer texture YUKLEMEZ, kutuphaneye sorar - bir sinifin
        // iki isi olmamali.
        SceneSerializer(Scene* scene, TextureLibrary* textureLibrary);

        // Basarili mi? Dosya yazma/okuma hata verebilir (izin, disk dolu,
        // bozuk JSON). Istisna atmiyoruz; motorlarda genelde kapali olur.
        bool Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);

    private:
        Scene*          m_Scene   = nullptr;
        TextureLibrary* m_Library = nullptr;
    };
}
