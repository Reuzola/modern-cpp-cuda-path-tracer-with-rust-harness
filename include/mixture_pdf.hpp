#pragma once
#include "pdf.hpp"
#include "random.hpp"
#include "vec3.hpp"

class mixture_pdf final : public pdf {
    public:
        mixture_pdf(const pdf& p1, const pdf& p2) : p1(p1), p2(p2) {}

        [[nodiscard]] double value(const vec3& direction) const override {
            return weight * p1.value(direction) + (1.0 - weight) * p2.value(direction);
        }

        [[nodiscard]] vec3 generate() const override {
            return (random_double() < weight) ? p1.generate() : p2.generate();
        }
    private:
        const pdf& p1;
        const pdf& p2;
        static constexpr double weight = 0.5;
};