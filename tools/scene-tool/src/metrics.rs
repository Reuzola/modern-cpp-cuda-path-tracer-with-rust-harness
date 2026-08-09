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

#[cfg(test)]
mod tests {
    use super::*;

    /// Builds a single-row image from raw RGB channels; the metrics ignore layout.
    fn image(channels: &[f32]) -> Image {
        Image {
            width: (channels.len() / 3) as u32,
            height: 1,
            pixels: channels.to_vec(),
        }
    }

    /// Float comparison with a tolerance: f64 arithmetic rarely lands on an exact value.
    fn assert_close(actual: f64, expected: f64) {
        assert!(
            (actual - expected).abs() < 1e-12,
            "expected {expected}, got {actual}"
        );
    }

    #[test]
    fn identical_images_have_zero_error() {
        let a = image(&[0.1, 0.5, 0.9]);
        let b = image(&[0.1, 0.5, 0.9]);

        let metrics = compute_metrics(&a, &b);

        assert_close(metrics.rmse, 0.0);
        assert_close(metrics.max_abs_diff, 0.0);
    }

    #[test]
    fn uniform_offset_rmse_equals_the_offset() {
        let reference = image(&[0.0, 0.0, 0.0]);
        let actual = image(&[0.5, 0.5, 0.5]);

        let metrics = compute_metrics(&reference, &actual);

        assert_close(metrics.rmse, 0.5);
        assert_close(metrics.max_abs_diff, 0.5);
    }

    #[test]
    fn max_abs_diff_reports_the_largest_channel() {
        let reference = image(&[0.0, 0.0, 0.0]);
        let actual = image(&[0.25, 0.5, 0.125]);

        let metrics = compute_metrics(&reference, &actual);

        assert_close(metrics.max_abs_diff, 0.5);
    }

    #[test]
    fn metrics_are_symmetric() {
        let a = image(&[0.2, 0.4, 0.6]);
        let b = image(&[0.9, 0.1, 0.5]);

        let forward = compute_metrics(&a, &b);
        let backward = compute_metrics(&b, &a);

        assert_close(forward.rmse, backward.rmse);
        assert_close(forward.max_abs_diff, backward.max_abs_diff);
    }

    #[test]
    fn empty_images_do_not_divide_by_zero() {
        let empty = image(&[]);

        let metrics = compute_metrics(&empty, &empty);

        assert_close(metrics.rmse, 0.0);
    }

    #[test]
    fn psnr_is_none_for_identical_images() {
        assert!(psnr(0.0).is_none());
    }

    #[test]
    fn psnr_of_one_tenth_rmse_is_twenty_decibels() {
        let db = psnr(0.1).expect("a non-zero rmse has a finite psnr");

        assert!((db - 20.0).abs() < 1e-9, "expected 20 dB, got {db}");
    }

    #[test]
    fn a_finite_image_has_no_bad_channel() {
        assert!(first_non_finite(&image(&[0.0, 1.0, 1e30])).is_none());
    }

    #[test]
    fn first_non_finite_reports_the_first_bad_index() {
        let broken = image(&[0.0, 1.0, f32::NAN, 2.0, f32::INFINITY, 0.0]);

        assert_eq!(first_non_finite(&broken), Some(2));
    }
}
