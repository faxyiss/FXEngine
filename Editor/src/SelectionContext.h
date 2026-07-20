#pragma once

// ===========================================================================
// FXEditor - SelectionContext
//
// "Hangi entity'ler secili" sorusunun TEK sahibi.
//
// NEDEN AYRI BIR SINIF? Secim daha once SceneHierarchyPanel'in icindeydi;
// viewport, gizmo, inspector ve prefab kaydetme hepsi o panelden okuyordu.
// Yani panel, kendisiyle ilgisi olmayan bir durumun sahibiydi ve ondan
// bagimsiz calismasi gereken her sey ona bagimli hale gelmisti.
//
// Ayirmanin asil sebebi ileriye donuk: Undo/Redo komutlari "hangi
// entity'ler uzerinde?" sorusunu cevaplar. Tek secim varsayimiyla yazilan
// her komut, coklu secim gelince yeniden yazilir.
//
// Sira ONEMLI: vector kullaniyoruz, set degil. Kullanicinin sectigi sira
// anlamli (ilk secilen "birincil"dir - gizmo onun etrafinda durur, Unity
// de boyle yapar) ve secim listeleri her zaman kucuk.
// ===========================================================================

#include <FXEngine/Scene/Entity.h>

#include <algorithm>
#include <vector>

namespace FXEd
{
    class SelectionContext
    {
    public:
        // Birincil secim: gizmo ve inspector bunu kullanir. Secim bossa
        // gecersiz entity doner - cagiran `if (entity)` ile kontrol eder.
        FX::Entity GetPrimary() const
        {
            return m_Entities.empty() ? FX::Entity{} : m_Entities.front();
        }

        const std::vector<FX::Entity>& GetAll() const { return m_Entities; }

        std::size_t Count() const { return m_Entities.size(); }
        bool        IsEmpty() const { return m_Entities.empty(); }

        bool IsSelected(FX::Entity entity) const
        {
            return std::find(m_Entities.begin(), m_Entities.end(), entity) !=
                   m_Entities.end();
        }

        // Secimi TEK bir entity'ye ayarlar. Gecersiz entity verilirse
        // secimi temizler; cagiranlarin ayrica Clear() dusunmesi gerekmesin.
        void Select(FX::Entity entity)
        {
            m_Entities.clear();
            if (entity)
                m_Entities.push_back(entity);
        }

        void Add(FX::Entity entity)
        {
            if (entity && !IsSelected(entity))
                m_Entities.push_back(entity);
        }

        void Remove(FX::Entity entity)
        {
            m_Entities.erase(std::remove(m_Entities.begin(), m_Entities.end(), entity),
                             m_Entities.end());
        }

        void Toggle(FX::Entity entity)
        {
            if (IsSelected(entity))
                Remove(entity);
            else
                Add(entity);
        }

        void Clear() { m_Entities.clear(); }

        // Silinmis entity'leri atar. Sahne degistiginde veya entity
        // silindiginde cagrilir: elde tutulan gecersiz bir entity'ye
        // component sormak tanimsiz davranis.
        void Prune()
        {
            m_Entities.erase(
                std::remove_if(m_Entities.begin(), m_Entities.end(),
                               [](FX::Entity e) { return !e; }),
                m_Entities.end());
        }

    private:
        std::vector<FX::Entity> m_Entities;
    };
}
