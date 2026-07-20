#version 330 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int  o_EntityID;

in vec4 v_Color;
flat in int v_EntityID;

void main()
{
    if (v_Color.a < 0.01)
        discard;

    o_Color = v_Color;

    // Ikinci ek YAZILMAK ZORUNDA. Bos birakilirsa o piksellerde onceki
    // karenin ID'si kalir ve secim yanlis entity'yi bulur. Cizgiler
    // secilemez: -1 = "entity'ye ait degil".
    o_EntityID = v_EntityID;
}
