#pragma once
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "support/test_support.hpp"
#include <algorithm>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// Statistical machinery for the (value, generate) contract. Written against
// callables rather than against Pdf, so Sampleable - which implements the same
// contract under different member names - is covered by the same helpers.
namespace pt_test {

using pt::operator""_f;
using DensityFn = std::function<pt::Float(const pt::Vec3&)>;
using SampleFn = std::function<pt::Vec3(pt::Sampler&)>;

inline constexpr double pi_d = 3.14159265358979323846;
inline constexpr double two_pi_d = 2.0 * pi_d;
inline constexpr double sphere_measure = 4.0 * pi_d;

// Equal-area binning of the sphere, uniform in cos(theta) rather than in theta.
// Bins of equal angular height near a pole cover far less solid angle than at
// the equator; an unequal-area grid would need a per-bin weight to compare at all.
inline constexpr int cos_theta_bins = 16;
inline constexpr int phi_bins = 32;
inline constexpr int total_bins = cos_theta_bins * phi_bins;

inline constexpr double bin_solid_angle = sphere_measure / total_bins;

// Points per axis inside each bin when integrating the density over it. A
// regular grid rather than random probes: see expected_bin_probabilities.
inline constexpr int quadrature_resolution = 16;

[[nodiscard]] inline pt::Vec3 direction_from(double cos_theta, double phi) noexcept {
    const double r = std::sqrt(std::max(0.0, 1.0 - cos_theta * cos_theta));
    return pt::Vec3(narrow(r * std::cos(phi)), narrow(r * std::sin(phi)), narrow(cos_theta));
}

[[nodiscard]] inline std::size_t bin_index(const pt::Vec3& direction) noexcept {
    const pt::Vec3 unit = unit_vector(direction);

    // acos is avoided: cos(theta) is already the z component, and binning on it
    // directly keeps the bins equal-area without a trigonometric round trip.
    const double cos_theta = widen(unit.z());
    const double phi = std::atan2(widen(unit.y()), widen(unit.x())) + pi_d;

    // Both ends are closed, so the maximum value lands one past the last cell.
    const int row = std::min(static_cast<int>((cos_theta + 1.0) * 0.5 * cos_theta_bins), cos_theta_bins - 1);
    const int column = std::min(static_cast<int>(phi / two_pi_d * phi_bins), phi_bins - 1);

    return static_cast<std::size_t>(row * phi_bins + column);
}

// A direction drawn uniformly over the sphere: the measure the integral below
// is taken against. Deliberately not random_unit_vector - the engine's own
// routine is part of what these tests check, so the reference must not share it.
[[nodiscard]] inline pt::Vec3 uniform_direction(pt::Sampler& sampler) noexcept {
    return direction_from(widen(sampler.next_scalar(-1.0_f, 1.0_f)), widen(sampler.next_scalar()) * two_pi_d - pi_d);
}

// Monte Carlo estimate of the density's integral over the sphere, which is 1
// for any normalised pdf. Sampling the measure uniformly means dividing by
// 1/(4*pi), hence the factor. Checks value() alone, independently of generate().
[[nodiscard]] inline double integrate_density(const DensityFn& density, pt::Sampler& sampler, int samples) {
    double sum = 0.0;
    for (int i = 0; i < samples; ++i) {
        sum += widen(density(uniform_direction(sampler)));
    }
    return sphere_measure * sum / samples;
}

// The reciprocal estimator: for X drawn from p, E[1/p(X)] is the measure of p's
// support, because the density cancels against itself. It is the one check that
// fails when value and generate disagree while each looks plausible alone.
//
// Only usable where 1/p is bounded on the support. For a cosine-weighted
// density 1/p grows without limit towards the horizon and the estimator has
// infinite variance: it converges to the right number, but no sample count
// makes it reliable. Such distributions rely on the chi-square test instead.
[[nodiscard]] inline double estimate_support_measure(const DensityFn& density, const SampleFn& sample, pt::Sampler& sampler, int samples) {
    double sum = 0.0;

    for (int i = 0; i < samples; ++i) {
        const double p = widen(density(sample(sampler)));

        // A generated direction whose own density is zero means the two members
        // disagree outright; report that rather than dividing by zero.
        REQUIRE(p > 0.0);

        sum += 1.0 / p;
    }

    return sum / samples;
}

// Probability mass value() assigns to each bin, by integrating it over the bin
// on a regular grid.
//
// Deterministic on purpose. Estimating these with random probes puts sampling
// noise on the *expected* side of the chi-square statistic, where it is
// indistinguishable from a real deviation and inflates the result until even a
// correct distribution is rejected. A grid contributes no variance at all; its
// only error is quadrature error where the density is discontinuous, and that
// stays far below the counting noise it is compared against.
[[nodiscard]] inline std::vector<double> expected_bin_probabilities(const DensityFn& density) {
    std::vector<double> probability(total_bins, 0.0);

    constexpr double inverse_resolution = 1.0 / quadrature_resolution;
    constexpr int probes_per_bin = quadrature_resolution * quadrature_resolution;

    for (int row = 0; row < cos_theta_bins; ++row) {
        for (int column = 0; column < phi_bins; ++column) {
            double sum = 0.0;

            for (int i = 0; i < quadrature_resolution; ++i) {
                for (int j = 0; j < quadrature_resolution; ++j) {
                    // Cell centres, so no probe lands on a bin boundary.
                    const double row_offset = static_cast<double>(row) + (static_cast<double>(i) + 0.5) * inverse_resolution;
                    const double column_offset = static_cast<double>(column) + (static_cast<double>(j) + 0.5) * inverse_resolution;

                    const double cos_theta = -1.0 + 2.0 * row_offset / cos_theta_bins;
                    const double phi = two_pi_d * column_offset / phi_bins - pi_d;

                    sum += widen(density(direction_from(cos_theta, phi)));
                }
            }

            // Mean density over the bin, times the solid angle it covers.
            probability[static_cast<std::size_t>(row * phi_bins + column)] = sum / probes_per_bin * bin_solid_angle;
        }
    }

    return probability;
}

struct ChiSquareResult {
    double statistic{};
    int degrees_of_freedom{};
};

// Pearson's goodness-of-fit statistic between generate()'s histogram and the
// bin probabilities implied by value(). This is the strongest of the three
// checks: the two estimators above compare scalar expectations, while this one
// compares the shape of the distribution bin by bin.
[[nodiscard]] inline ChiSquareResult chi_square(const DensityFn& density, const SampleFn& sample, pt::Sampler& sampler, int samples) {
    const std::vector<double> probability = expected_bin_probabilities(density);

    std::vector<int> observed(total_bins, 0);
    for (int i = 0; i < samples; ++i) {
        observed[bin_index(sample(sampler))] += 1;
    }

    double statistic = 0.0;
    int cells = 0;
    int dropped_observations = 0;

    for (std::size_t i = 0; i < static_cast<std::size_t>(total_bins); ++i) {
        const double expected_count = probability[i] * samples;

        // The chi-square approximation needs a handful of counts per cell.
        if (expected_count < 5.0) {
            dropped_observations += observed[i];
            continue;
        }

        const double difference = observed[i] - expected_count;
        statistic += difference * difference / expected_count;
        ++cells;
    }

    // Samples landing where value() says almost nothing should are exactly the
    // support mismatch the pooled cells would otherwise hide.
    REQUIRE(dropped_observations < samples / 100);

    // Too few surviving cells means the distribution is narrower than the grid
    // can resolve, and the statistic below would be meaningless.
    REQUIRE(cells >= 5);

    return ChiSquareResult{.statistic = statistic, .degrees_of_freedom = cells - 1};
}

// Upper-tail probability of the chi-square distribution, the regularised upper
// incomplete gamma Q(k/2, x/2). Written out rather than pulled from a library:
// the test binary links nothing but Catch2 and the engine, and one special
// function is not worth a dependency.
[[nodiscard]] inline double chi_square_p_value(double statistic, int degrees_of_freedom) {
    const double a = 0.5 * degrees_of_freedom;
    const double x = 0.5 * statistic;

    if (x <= 0.0) return 1.0;

    const double log_prefactor = a * std::log(x) - x - std::lgamma(a);

    if (x < a + 1.0) {
        // Series for the lower tail P(a, x), then complemented.
        double term = 1.0 / a;
        double sum = term;
        for (int i = 1; i < 1000; ++i) {
            term *= x / (a + i);
            sum += term;
            if (term < sum * 1.0e-15) break;
        }
        return 1.0 - sum * std::exp(log_prefactor);
    }

    // Lentz's continued fraction for the upper tail Q(a, x).
    constexpr double tiny = 1.0e-300;
    double b = x + 1.0 - a;
    double c = 1.0 / tiny;
    double d = 1.0 / b;
    double h = d;

    for (int i = 1; i < 1000; ++i) {
        const double an = -i * (i - a);
        b += 2.0;
        d = an * d + b;
        if (std::fabs(d) < tiny) d = tiny;
        c = b + an / c;
        if (std::fabs(c) < tiny) c = tiny;
        d = 1.0 / d;
        const double delta = d * c;
        h *= delta;
        if (std::fabs(delta - 1.0) < 1.0e-15) break;
    }

    return h * std::exp(log_prefactor);
}

struct DistributionCheck {
    // Measure of the support, for the reciprocal estimator. Left at zero when
    // 1/p is unbounded or the support has no closed form worth deriving; the
    // estimator is then skipped and the chi-square carries the consistency check.
    double support_measure = 0.0;

    int integration_samples = 400000;
    double integration_tolerance = 0.03;

    // Kept well below the integration count: the statistic scales with the
    // sample size, so a larger histogram magnifies the quadrature error in the
    // expected probabilities without buying much extra sensitivity.
    int chi_square_samples = 50000;

    // Ten thousand to one. A correct distribution trips this once in ten
    // thousand runs, while a real bias drives the p-value many orders of
    // magnitude lower - wide, but still discriminating.
    double p_value_floor = 1.0e-4;

    std::uint64_t stream = 0;
};

inline void require_consistent_distribution(const DensityFn& density, const SampleFn& sample, const DistributionCheck& check) {
    // Failing this alone means value() is not a normalised density.
    pt::Sampler integration_sampler = make_sampler(check.stream);
    const double integral = integrate_density(density, integration_sampler, check.integration_samples);
    INFO("integral of the density over the sphere: " << integral);
    REQUIRE_THAT(integral, Catch::Matchers::WithinAbs(1.0, check.integration_tolerance));

    // Failing this alone means generate() does not draw from value().
    if (check.support_measure > 0.0) {
        pt::Sampler support_sampler = make_sampler(check.stream + 1);
        const double measured = estimate_support_measure(density, sample, support_sampler, check.chi_square_samples);
        INFO("support measure: " << measured << " against " << check.support_measure);
        REQUIRE_THAT(measured / check.support_measure, Catch::Matchers::WithinAbs(1.0, 0.02));
    }

    // Failing this alone means the two agree on average but not in shape.
    pt::Sampler chi_sampler = make_sampler(check.stream + 2);
    const ChiSquareResult result = chi_square(density, sample, chi_sampler, check.chi_square_samples);
    const double p_value = chi_square_p_value(result.statistic, result.degrees_of_freedom);

    INFO("chi-square " << result.statistic << " on " << result.degrees_of_freedom << " degrees of freedom, p = " << p_value);
    REQUIRE(p_value > check.p_value_floor);
}

} // namespace pt_test
