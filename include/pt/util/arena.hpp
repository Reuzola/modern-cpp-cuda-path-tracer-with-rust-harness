#pragma once
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace pt {

template <typename Base>
    requires std::has_virtual_destructor_v<Base>
class Arena {
public:
    template <typename T, typename... Args>
        requires std::derived_from<T, Base> && std::constructible_from<T, Args...>
    [[nodiscard]] const T* create(Args&&... args) {
        std::unique_ptr<T> owned = std::make_unique<T>(std::forward<Args>(args)...);

        const T* observer = owned.get();

        items_.push_back(std::move(owned));
        return observer;
    }

private:
    std::vector<std::unique_ptr<Base>> items_;
};

} // namespace pt
