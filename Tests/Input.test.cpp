#include <catch2/catch_test_macros.hpp>

#include <FXEngine/Core/KeyCodes.h>

#include <SDL3/SDL.h>

// Key ve MouseButton degerleri SDL sabitleriyle AYNI olmak zorunda:
// ceviri katmani ve Input::IsKeyPressed bunu varsayip cast ediyor.
// Bir gun SDL surumu degistirse ya da enum'a yanlis sayi yazilsa,
// hata sessizce "bazi tuslar calismiyor" seklinde gorunurdu.

TEST_CASE("Key degerleri SDL scancode ile ayni", "[input]")
{
    CHECK(static_cast<int>(FX::Key::A)      == SDL_SCANCODE_A);
    CHECK(static_cast<int>(FX::Key::Z)      == SDL_SCANCODE_Z);
    CHECK(static_cast<int>(FX::Key::D1)     == SDL_SCANCODE_1);
    CHECK(static_cast<int>(FX::Key::D0)     == SDL_SCANCODE_0);
    CHECK(static_cast<int>(FX::Key::Space)  == SDL_SCANCODE_SPACE);
    CHECK(static_cast<int>(FX::Key::Escape) == SDL_SCANCODE_ESCAPE);
    CHECK(static_cast<int>(FX::Key::Enter)  == SDL_SCANCODE_RETURN);
    CHECK(static_cast<int>(FX::Key::Delete) == SDL_SCANCODE_DELETE);
    CHECK(static_cast<int>(FX::Key::F1)     == SDL_SCANCODE_F1);
    CHECK(static_cast<int>(FX::Key::F12)    == SDL_SCANCODE_F12);
    CHECK(static_cast<int>(FX::Key::Left)   == SDL_SCANCODE_LEFT);
    CHECK(static_cast<int>(FX::Key::Up)     == SDL_SCANCODE_UP);
}

TEST_CASE("Modifier tuslari SDL scancode ile ayni", "[input]")
{
    CHECK(static_cast<int>(FX::Key::LeftCtrl)   == SDL_SCANCODE_LCTRL);
    CHECK(static_cast<int>(FX::Key::LeftShift)  == SDL_SCANCODE_LSHIFT);
    CHECK(static_cast<int>(FX::Key::LeftAlt)    == SDL_SCANCODE_LALT);
    CHECK(static_cast<int>(FX::Key::RightCtrl)  == SDL_SCANCODE_RCTRL);
    CHECK(static_cast<int>(FX::Key::RightShift) == SDL_SCANCODE_RSHIFT);
    CHECK(static_cast<int>(FX::Key::RightAlt)   == SDL_SCANCODE_RALT);
}

TEST_CASE("MouseButton degerleri SDL ile ayni", "[input]")
{
    CHECK(static_cast<int>(FX::MouseButton::Left)   == SDL_BUTTON_LEFT);
    CHECK(static_cast<int>(FX::MouseButton::Middle) == SDL_BUTTON_MIDDLE);
    CHECK(static_cast<int>(FX::MouseButton::Right)  == SDL_BUTTON_RIGHT);
    CHECK(static_cast<int>(FX::MouseButton::X1)     == SDL_BUTTON_X1);
    CHECK(static_cast<int>(FX::MouseButton::X2)     == SDL_BUTTON_X2);
}

TEST_CASE("Key araligi SDL klavye dizisine sigiyor", "[input]")
{
    // IsKeyPressed diziyi Key degeriyle INDEKSLIYOR; sinirin disina
    // tasan bir deger bellek okuma hatasi olurdu.
    CHECK(static_cast<int>(FX::Key::RightAlt) < SDL_SCANCODE_COUNT);
}
