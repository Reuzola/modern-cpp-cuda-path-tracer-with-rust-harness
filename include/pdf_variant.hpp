#pragma once
#include "pdf.hpp"
#include "cosine_pdf.hpp"
#include "sphere_pdf.hpp"
#include <variant>

using pdf_variant = std::variant<cosine_pdf, sphere_pdf>;

[[nodiscard]] inline const pdf& as_pdf(const pdf_variant& v) {
    return std::visit([](const pdf& p) -> const pdf& { return p; }, v);
}