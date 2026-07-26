#pragma once

namespace pt {

template <typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

} // namespace pt
