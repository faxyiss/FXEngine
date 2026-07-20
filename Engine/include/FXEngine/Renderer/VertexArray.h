#pragma once

// ===========================================================================
// FXEngine - VertexArray (VAO)
//
// EN SIK YANLIS ANLASILAN OPENGL NESNESI BUDUR, o yuzden net olalim:
//
// VAO VERI TUTMAZ. Veri VBO'da. VAO, o verinin NASIL OKUNACAGINI tutar:
//   "0 numarali attribute = 3 float, 12 bayt aralikla, 0. bayttan basla"
//
// Ayrica hangi index buffer'in (EBO) kullanilacagini da hatirlar.
//
// FAYDASI: Bu tarifi bir kez kurarsin; cizim aninda tek bir glBindVertexArray
// cagrisi tum ayari geri getirir. VAO olmasaydi her cizimden once
// glVertexAttribPointer cagrilarini tekrar tekrar yapman gerekirdi.
// ===========================================================================

#include "FXEngine/Renderer/Buffer.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace FX
{
    class VertexArray
    {
    public:
        VertexArray();
        ~VertexArray();

        VertexArray(const VertexArray&)            = delete;
        VertexArray& operator=(const VertexArray&) = delete;

        void Bind() const;
        void Unbind() const;

        // VBO ekler ve layout'una gore attribute'lari kurar.
        // shared_ptr cunku ayni vertex buffer birden fazla VAO tarafindan
        // paylasilabilir (ornegin ayni model, farkli attribute duzenleriyle).
        void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer);

        void SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer);

        const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }

    private:
        std::uint32_t m_RendererID = 0;

        // Bir sonraki VBO'nun attribute'lari kacinci indexten baslasin?
        // Iki VBO eklenirse ikincisi 0'dan degil, birincinin bittigi yerden baslar.
        std::uint32_t m_VertexBufferIndex = 0;

        // Buffer'lari burada tutuyoruz ki VAO yasadigi surece yok edilmesinler.
        // Bu tutmasaydik, kullanici VBO'yu kapsam disina cikardiginda GPU
        // tamponu silinir, VAO gecersiz veriye isaret eder ve ekran bos kalir.
        std::vector<std::shared_ptr<VertexBuffer>> m_VertexBuffers;
        std::shared_ptr<IndexBuffer>               m_IndexBuffer;
    };
}
