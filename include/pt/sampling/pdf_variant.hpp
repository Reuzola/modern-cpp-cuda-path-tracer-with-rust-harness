#pragma once
#include "pt/sampling/cosine_pdf.hpp"
#include "pt/sampling/pdf.hpp"
#include "pt/sampling/sphere_pdf.hpp"
#include <variant>

namespace pt {

using pdf_variant = std::variant<cosine_pdf, sphere_pdf>;

[[nodiscard]] inline const pdf& as_pdf(const pdf_variant& v) {
    return std::visit([](const pdf& p) -> const pdf& { return p; }, v);
}

} // namespace pt
