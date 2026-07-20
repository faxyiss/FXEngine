#version 330 core

// ===========================================================================
// VERTEX SHADER - her KOSE icin bir kez calisir.
// Isi: kosenin ekranda nereye dusecegini belirlemek.
// ===========================================================================

// "layout(location = 0)" -> C++ tarafinda glVertexAttribPointer'a verdigimiz
// index ile eslesir. VertexArray.cpp'de m_VertexBufferIndex 0'dan basliyordu;
// layout'umuzun ilk elemani bu yuzden 0 numarali attribute.
//
// "in" = bu shader'a DISARIDAN gelen veri (kose basina degisir).
layout(location = 0) in vec3 a_Position;

// Nesneyi dunyada konumlandiran matris. Tum koseler icin ayni -> uniform.
uniform mat4 u_Transform;

void main()
{
    // gl_Position ozel bir degiskendir: vertex shader'in ZORUNLU ciktisi.
    // OpenGL bu degeri "clip space" koordinati olarak bekler.
    //
    // vec4'un son bileseni (w) 1.0: bu, noktanin bir KONUM oldugunu soyler.
    // 0.0 olsaydi bir YON olurdu ve tasima (translation) matrisi onu etkilemezdi.
    // Bu ayrim homojen koordinatlarin puf noktasidir.
    gl_Position = u_Transform * vec4(a_Position, 1.0);
}
