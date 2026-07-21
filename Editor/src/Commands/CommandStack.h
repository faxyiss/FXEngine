#pragma once

// ===========================================================================
// FXEditor - CommandStack (A5, Undo/Redo)
//
// Her duzenleme adimi bir EditCommand: uygulanmis bir degisikligi GERI ALAN
// ve YENIDEN UYGULAYAN iki fonksiyon. Komutlar gereken durumu kendileri
// yakalar (closure); yigin yalnizca sirayi tutar.
//
// ONEMLI KABUL: Push edilen komut ZATEN UYGULANMIS sayilir (deger arayuzde
// degismis durumda). Bu yuzden Push, Redo'yu tekrar CALISTIRMAZ - sadece
// kaydeder. Boylece "editorde surukledim" ile "komut" ayni sonucu iki kez
// uygulamaz.
// ===========================================================================

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace FXEd
{
    struct EditCommand
    {
        std::string           Name;   // durum cubugunda gosterilir
        std::function<void()> Undo;
        std::function<void()> Redo;
    };

    class CommandStack
    {
    public:
        void Push(EditCommand cmd)
        {
            // Yeni bir dal: ileri gecmis (redo) artik gecersiz.
            m_Redo.clear();
            m_Undo.push_back(std::move(cmd));

            // Sinirsiz gecmis belleği sisirir; en eskiyi at.
            if (m_Undo.size() > kMax)
                m_Undo.erase(m_Undo.begin());
        }

        bool CanUndo() const { return !m_Undo.empty(); }
        bool CanRedo() const { return !m_Redo.empty(); }

        // Doner: geri alinan komutun adi (durum mesaji icin), yoksa bos.
        std::string Undo()
        {
            if (m_Undo.empty())
                return {};

            EditCommand cmd = std::move(m_Undo.back());
            m_Undo.pop_back();
            if (cmd.Undo)
                cmd.Undo();

            std::string name = cmd.Name;
            m_Redo.push_back(std::move(cmd));
            return name;
        }

        std::string Redo()
        {
            if (m_Redo.empty())
                return {};

            EditCommand cmd = std::move(m_Redo.back());
            m_Redo.pop_back();
            if (cmd.Redo)
                cmd.Redo();

            std::string name = cmd.Name;
            m_Undo.push_back(std::move(cmd));
            return name;
        }

        void Clear()
        {
            m_Undo.clear();
            m_Redo.clear();
        }

    private:
        // 200 adim: pratikte fazlasiyla yeterli, bellek endisesi de yok.
        static constexpr std::size_t kMax = 200;

        std::vector<EditCommand> m_Undo;
        std::vector<EditCommand> m_Redo;
    };
}
