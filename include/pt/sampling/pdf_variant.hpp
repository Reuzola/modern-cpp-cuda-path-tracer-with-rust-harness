#pragma once
#include "pt/sampling/cosine_pdf.hpp"
#include "pt/sampling/pdf.hpp"
#include "pt/sampling/sphere_pdf.hpp"
#include <variant>

namespace pt {

using PdfVariant = std::variant<CosinePdf, SpherePdf>;

[[nodiscard]] inline const Pdf& as_pdf(const PdfVariant& v) {
    return std::visit([](const Pdf& p) -> const Pdf& { return p; }, v);
}

} // namespace pt
