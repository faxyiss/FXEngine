#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Scene/Components.h"

#include <algorithm>

namespace FX
{
    UUID Entity::GetUUID() const
    {
        return m_Scene->m_Registry.get<IDComponent>(m_EntityHandle).ID;
    }

    const std::string& Entity::GetName() const
    {
        return m_Scene->m_Registry.get<TagComponent>(m_EntityHandle).Tag;
    }

    Entity Entity::GetParent() const
    {
        if (!HasComponent<RelationshipComponent>())
            return {};

        const auto& rc = m_Scene->m_Registry.get<RelationshipComponent>(m_EntityHandle);
        if (!rc.Parent.IsValid())
            return {};

        return m_Scene->FindEntityByUUID(rc.Parent);
    }

    std::vector<Entity> Entity::GetChildren() const
    {
        std::vector<Entity> result;
        if (!HasComponent<RelationshipComponent>())
            return result;

        const auto& rc = m_Scene->m_Registry.get<RelationshipComponent>(m_EntityHandle);
        result.reserve(rc.Children.size());

        for (const auto childID : rc.Children)
        {
            if (Entity child = m_Scene->FindEntityByUUID(childID))
                result.push_back(child);
        }
        return result;
    }

    bool Entity::HasChildren() const
    {
        if (!HasComponent<RelationshipComponent>())
            return false;
        return !m_Scene->m_Registry.get<RelationshipComponent>(m_EntityHandle).Children.empty();
    }

    bool Entity::IsAncestorOf(Entity other) const
    {
        Entity current = other.GetParent();
        while (current)
        {
            if (current == *this)
                return true;
            current = current.GetParent();
        }
        return false;
    }

    void Entity::SetParent(Entity parent)
    {
        if (!*this)
            return;

        // Kendini kendine ya da kendi torununa baglamak sonsuz donguye
        // yol acar: TransformSystem zinciri hic bitiremez.
        if (parent == *this || (parent && IsAncestorOf(parent)))
        {
            FX_CORE_WARN("SetParent reddedildi: dongusel hiyerarsi.");
            return;
        }

        auto& rc = AddOrReplaceIfMissing<RelationshipComponent>();

        if (rc.Parent.IsValid())
        {
            if (Entity oldParent = m_Scene->FindEntityByUUID(rc.Parent))
            {
                auto& prc = oldParent.GetComponent<RelationshipComponent>();
                prc.Children.erase(
                    std::remove(prc.Children.begin(), prc.Children.end(), GetUUID()),
                    prc.Children.end());
            }
        }

        if (!parent)
        {
            rc.Parent = UUID(0);
            return;
        }

        rc.Parent = parent.GetUUID();

        auto& prc = parent.AddOrReplaceIfMissing<RelationshipComponent>();
        prc.Children.push_back(GetUUID());
    }
}
