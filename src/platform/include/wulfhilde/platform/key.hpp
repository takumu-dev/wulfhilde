/// \file
/// The keys of a keyboard, by where they are on it.

#pragma once

#include <cstdint>

namespace wulfhilde {

/// A physical key of a keyboard, by its usage ID on the USB HID keyboard page.
///
/// A key is named for where it is, as on a US keyboard, not for what its cap says: `Key::w` is the
/// key to the right of Q on any layout, which is Z on a French keyboard. A key that has no name
/// here still has its usage ID as its value.
// NOLINTNEXTLINE(performance-enum-size): SDL numbers keys beyond the keyboard page past 255
enum class Key : std::uint16_t {
    a = 4,  ///< A.
    b = 5,  ///< B.
    c = 6,  ///< C.
    d = 7,  ///< D.
    e = 8,  ///< E.
    f = 9,  ///< F.
    g = 10, ///< G.
    h = 11, ///< H.
    i = 12, ///< I.
    j = 13, ///< J.
    k = 14, ///< K.
    l = 15, ///< L.
    m = 16, ///< M.
    n = 17, ///< N.
    o = 18, ///< O.
    p = 19, ///< P.
    q = 20, ///< Q.
    r = 21, ///< R.
    s = 22, ///< S.
    t = 23, ///< T.
    u = 24, ///< U.
    v = 25, ///< V.
    w = 26, ///< W.
    x = 27, ///< X.
    y = 28, ///< Y.
    z = 29, ///< Z.

    digit_1 = 30, ///< 1 on the row of digits.
    digit_2 = 31, ///< 2 on the row of digits.
    digit_3 = 32, ///< 3 on the row of digits.
    digit_4 = 33, ///< 4 on the row of digits.
    digit_5 = 34, ///< 5 on the row of digits.
    digit_6 = 35, ///< 6 on the row of digits.
    digit_7 = 36, ///< 7 on the row of digits.
    digit_8 = 37, ///< 8 on the row of digits.
    digit_9 = 38, ///< 9 on the row of digits.
    digit_0 = 39, ///< 0 on the row of digits.

    enter = 40,     ///< Return, or Enter, beside the letters.
    escape = 41,    ///< Esc.
    backspace = 42, ///< Backspace.
    tab = 43,       ///< Tab.
    space = 44,     ///< The space bar.

    f1 = 58,  ///< F1.
    f2 = 59,  ///< F2.
    f3 = 60,  ///< F3.
    f4 = 61,  ///< F4.
    f5 = 62,  ///< F5.
    f6 = 63,  ///< F6.
    f7 = 64,  ///< F7.
    f8 = 65,  ///< F8.
    f9 = 66,  ///< F9.
    f10 = 67, ///< F10.
    f11 = 68, ///< F11.
    f12 = 69, ///< F12.

    insert = 73,         ///< Insert.
    home = 74,           ///< Home.
    page_up = 75,        ///< Page Up.
    delete_forward = 76, ///< Delete, which deletes forward.
    end = 77,            ///< End.
    page_down = 78,      ///< Page Down.
    right = 79,          ///< The right arrow.
    left = 80,           ///< The left arrow.
    down = 81,           ///< The down arrow.
    up = 82,             ///< The up arrow.

    left_control = 224,  ///< The left Control.
    left_shift = 225,    ///< The left Shift.
    left_alt = 226,      ///< The left Alt, or Option.
    left_gui = 227,      ///< The left Windows, Command or Super key.
    right_control = 228, ///< The right Control.
    right_shift = 229,   ///< The right Shift.
    right_alt = 230,     ///< The right Alt, AltGr or Option.
    right_gui = 231,     ///< The right Windows, Command or Super key.
};

} // namespace wulfhilde
