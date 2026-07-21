#pragma once

// ===========================================================================
// FXEngine - ScriptableEntity
//
// Bir entity'ye DAVRANIS eklemenin yolu. Component'ler veri, sistemler
// mantik - script ise bu ikisinin arasindaki bosluk: tek bir entity'ye
// ozel, kendi durumu olan mantik.
//
// NEDEN COMPONENT'IN KENDISI DEGIL? Cunku script bir veri parcasi degil,
// component'leri DUZENLEYEN bir nesne. ECS'in "component saf veri olmali"
// kurali bozulmasin diye script'in kendisi component'in ICINDE tasiniyor
// (NativeScriptComponent), component'in kendisi olmuyor.
//
// OnCreate / OnUpdate / OnDestroy protected: bunlari MOTOR cagirir,
// kullanici degil. Disaridan cagrilabilir olsaydi "iki kez OnCreate"
// gibi hatalar mumkun olurdu.
// ===========================================================================

#include "FXEngine/Scene/Entity.h"

namespace FX
{
    class ScriptableEntity
    {
    public:
        virtual ~ScriptableEntity() = default;

        // Script kendi entity'sine buradan ulasir; GetComponent<T>()
        // yazmak entity.GetComponent<T>() yazmaktan kisa ve script
        // kodunun cogu bunu yapiyor.
        template<typename T>
        T& GetComponent() { return m_Entity.GetComponent<T>(); }

        template<typename T>
        bool HasComponent() const { return m_Entity.HasComponent<T>(); }

        Entity GetEntity() const { return m_Entity; }

        // Sahne, baska entity'leri bulmak icin (FindEntityByUUID).
        Scene* GetScene() const { return m_Entity.GetScene(); }

    protected:
        // Play basladiginda BIR KEZ. Kaynak bulma, baslangic durumu.
        virtual void OnCreate() {}

        // Her sabit adimda. dt her zaman ayni deger (1/60).
        virtual void OnUpdate(float dt) { (void)dt; }

        // Stop'ta veya entity silindiginde bir kez.
        virtual void OnDestroy() {}

    private:
        Entity m_Entity;

        // Entity'yi ve yasam dongusunu yalnizca ScriptSystem yonetir.
        friend class ScriptSystem;
    };
}
