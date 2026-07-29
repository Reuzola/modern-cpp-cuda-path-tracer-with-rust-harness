#pragma once

namespace pt {

template <typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

} // namespace pt
