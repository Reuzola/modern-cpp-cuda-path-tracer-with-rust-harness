//! Difference metrics between two images of identical dimensions.
use crate::image::Image;

#[derive(Debug, Clone, Copy)]
pub struct Metrics {
    pub rmse: f64,
    pub max_abs_diff: f64,
}

/// Compares two images channel by channel.
/// Precondition: both images have identical dimensions.
#[must_use]
pub fn compute_metrics(reference: &Image, actual: &Image) -> Metrics {
    debug_assert_eq!(reference.pixels.len(), actual.pixels.len());

    let mut sum_squared = 0.0f64;
    let mut max_abs_diff = 0.0f64;

    for (r, a) in reference.pixels.iter().copied().zip(actual.pixels.iter().copied()) {
        let diff = f64::from(r) - f64::from(a);
        sum_squared += diff * diff;
        let abs = diff.abs();

        if abs > max_abs_diff {
            max_abs_diff = abs;
        }
    }

    let count = reference.pixels.len() as f64;
    let rmse = if count > 0.0 {
        (sum_squared / count).sqrt()
    } else { 0.0 };

    Metrics { rmse, max_abs_diff }
}

/// Peak signal-to-noise ratio in decibels, for LDR images where MAX = 1.0.
/// Returns None for identical images, where PSNR is infinite.
#[must_use]
pub fn psnr(rmse: f64) -> Option<f64> {
    if rmse <= 0.0 { None }
    else { Some(-20.0 * rmse.log10()) }
}

/// Index of the first NaN or infinite channel value, if any.
/// EXR output can contain them: the engine skips tone mapping (and its NaN guard) for HDR formats.
#[must_use]
pub fn first_non_finite(image: &Image) -> Option<usize> {
    image.pixels.iter().position(|v| !v.is_finite())
}
