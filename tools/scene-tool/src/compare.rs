//! Golden image comparison: loads two images, measures the difference, optionally writes a diff.
use crate::diff::write_diff_png;
use crate::error::ToolError;
use crate::image::{Image, ImageFormat, block_mean, format_from_path, load_image};
use crate::metrics::{Metrics, compute_metrics, first_non_finite, psnr};
use std::path::Path;

#[derive(Debug)]
pub struct CompareOutcome {
    pub metrics: Metrics,
    pub psnr_db: Option<f64>,
    pub passed: bool,
    pub diff_written: bool,
    pub block_metrics: Metrics,
    pub block_size: u32,
}

fn ensure_finite(image: &Image, path: &Path) -> Result<(), ToolError> {
    if let Some(index) = first_non_finite(image) {
        return Err(ToolError::NonFiniteValue {
            path: path.to_path_buf(),
            index,
        });
    }
    Ok(())
}

/// Reports whether the block RMSE stays within the threshold.
pub fn compare_images(
    reference_path: &Path,
    actual_path: &Path,
    threshold: f64,
    block_size: u32,
    diff_path: Option<&Path>,
    diff_gain: f32,
) -> Result<CompareOutcome, ToolError> {
    let reference_format = format_from_path(reference_path);
    let actual_format = format_from_path(actual_path);
    if reference_format != actual_format {
        return Err(ToolError::FormatMismatch {
            reference: reference_path.to_path_buf(),
            actual: actual_path.to_path_buf(),
        });
    }

    let reference = load_image(reference_path)?;
    let actual = load_image(actual_path)?;

    if reference.dimensions() != actual.dimensions() {
        return Err(ToolError::DimensionMismatch {
            reference: reference_path.to_path_buf(),
            reference_width: reference.width,
            reference_height: reference.height,
            actual: actual_path.to_path_buf(),
            actual_width: actual.width,
            actual_height: actual.height,
        });
    }

    ensure_finite(&reference, reference_path)?;
    ensure_finite(&actual, actual_path)?;

    let metrics = compute_metrics(&reference, &actual);
    let psnr_db = if reference_format == Some(ImageFormat::Png) {
        psnr(metrics.rmse)
    } else {
        None
    };

    let block_metrics = compute_metrics(
        &block_mean(&reference, block_size),
        &block_mean(&actual, block_size),
    );

    let passed = block_metrics.rmse <= threshold;
    let mut diff_written = false;
    if !passed && let Some(diff_path) = diff_path {
        write_diff_png(&reference, &actual, diff_gain, diff_path)?;
        diff_written = true;
    }

    Ok(CompareOutcome {
        metrics,
        psnr_db,
        passed,
        diff_written,
        block_metrics,
        block_size,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::image::load_png;
    use tempfile::TempDir;

    fn image(width: u32, height: u32, channels: &[f32]) -> Image {
        Image {
            width,
            height,
            pixels: channels.to_vec(),
        }
    }

    /// Writes an image as PNG by diffing it against black, which leaves the
    /// values untouched. Saves the tests from carrying a second encoder.
    fn write_png(path: &Path, source: &Image) {
        let black = image(source.width, source.height, &vec![0.0; source.pixels.len()]);
        write_diff_png(&black, source, 1.0, path).expect("the fixture must be writable");
    }

    /// A checkerboard and its inverse: every channel differs by the maximum
    /// possible amount, yet every 2x2 block holds the same mean in both. This
    /// is Monte Carlo noise in its most extreme form.
    fn checkerboard() -> (Image, Image) {
        let reference = image(
            2,
            2,
            &[1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0],
        );
        let actual = image(
            2,
            2,
            &[0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0],
        );
        (reference, actual)
    }

    #[test]
    fn identical_images_pass_and_write_no_diff() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");
        let diff = dir.path().join("diff.png");

        let source = image(1, 1, &[0.25, 0.5, 0.75]);
        write_png(&reference, &source);
        write_png(&actual, &source);

        let outcome = compare_images(&reference, &actual, 0.0, 1, Some(&diff), 10.0)
            .expect("both images must load");

        assert!(outcome.passed);
        assert!(!outcome.diff_written);
        assert!(!diff.exists());
        assert_eq!(outcome.metrics.rmse, 0.0);
        assert_eq!(outcome.psnr_db, None);
    }

    #[test]
    fn a_difference_beyond_the_threshold_fails_and_writes_the_diff() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");
        let diff = dir.path().join("diff.png");

        write_png(&reference, &image(1, 1, &[0.0, 0.0, 0.0]));
        write_png(&actual, &image(1, 1, &[1.0, 0.0, 0.0]));

        let outcome = compare_images(&reference, &actual, 0.5, 1, Some(&diff), 1.0)
            .expect("both images must load");

        assert!(!outcome.passed);
        assert!(outcome.diff_written);
        assert!(diff.is_file());
    }

    // One channel of three differs by 1, so the RMSE is sqrt(1/3): the same
    // pair passes or fails depending only on where the threshold sits.
    #[test]
    fn the_threshold_decides_the_verdict() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");

        write_png(&reference, &image(1, 1, &[0.0, 0.0, 0.0]));
        write_png(&actual, &image(1, 1, &[1.0, 0.0, 0.0]));

        let lenient = compare_images(&reference, &actual, 0.6, 1, None, 1.0).expect("must load");
        let strict = compare_images(&reference, &actual, 0.5, 1, None, 1.0).expect("must load");

        assert!(lenient.passed);
        assert!(!strict.passed);
        assert!((lenient.metrics.rmse - (1.0f64 / 3.0).sqrt()).abs() < 1e-9);
    }

    #[test]
    fn a_failure_without_a_diff_path_writes_nothing() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");

        write_png(&reference, &image(1, 1, &[0.0, 0.0, 0.0]));
        write_png(&actual, &image(1, 1, &[1.0, 1.0, 1.0]));

        let outcome = compare_images(&reference, &actual, 0.0, 1, None, 1.0).expect("must load");

        assert!(!outcome.passed);
        assert!(!outcome.diff_written);
    }

    #[test]
    fn psnr_is_reported_for_ldr_images() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");

        write_png(&reference, &image(1, 1, &[0.0, 0.0, 0.0]));
        write_png(&actual, &image(1, 1, &[1.0, 0.0, 0.0]));

        let outcome = compare_images(&reference, &actual, 1.0, 1, None, 1.0).expect("must load");

        assert!(outcome.psnr_db.is_some());
    }

    #[test]
    fn images_of_different_sizes_are_an_error() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");

        write_png(&reference, &image(2, 1, &[0.0; 6]));
        write_png(&actual, &image(1, 1, &[0.0; 3]));

        let err = compare_images(&reference, &actual, 0.0, 8, None, 1.0)
            .expect_err("mismatched dimensions cannot be compared");

        assert!(matches!(err, ToolError::DimensionMismatch { .. }), "{err}");
    }

    // Rejected before either file is opened: PNG is gamma-encoded and EXR is
    // linear, so there is no meaningful metric across the two.
    #[test]
    fn comparing_a_png_against_an_exr_is_an_error() {
        let err = compare_images(Path::new("a.png"), Path::new("b.exr"), 0.0, 8, None, 1.0)
            .expect_err("formats must match");

        assert!(matches!(err, ToolError::FormatMismatch { .. }), "{err}");
    }

    // Two unknown extensions match each other, so the format check passes and
    // the loader is the one that refuses.
    #[test]
    fn an_unsupported_extension_is_an_error() {
        let err = compare_images(Path::new("a.jpg"), Path::new("b.jpg"), 0.0, 8, None, 1.0)
            .expect_err("jpg is not a supported format");

        assert!(matches!(err, ToolError::UnknownImageFormat { .. }), "{err}");
    }

    #[test]
    fn a_zero_mean_difference_is_absorbed_by_the_block_average() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");

        let (r, a) = checkerboard();
        write_png(&reference, &r);
        write_png(&actual, &a);

        let outcome =
            compare_images(&reference, &actual, 0.0, 2, None, 1.0).expect("both images must load");

        assert!(outcome.passed);
        assert_eq!(outcome.block_metrics.rmse, 0.0);
    }

    // The full-resolution figures stay in the report after they stop deciding:
    // the same pair that passes above is as different as two images can be.
    #[test]
    fn the_full_resolution_metrics_are_still_measured() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");

        let (r, a) = checkerboard();
        write_png(&reference, &r);
        write_png(&actual, &a);

        let outcome =
            compare_images(&reference, &actual, 0.0, 2, None, 1.0).expect("both images must load");

        assert_eq!(outcome.metrics.rmse, 1.0);
        assert_eq!(outcome.metrics.max_abs_diff, 1.0);
        assert_eq!(outcome.block_size, 2);
    }

    // The same pair, gated per pixel: a block size of one turns the new gate
    // back into the old one.
    #[test]
    fn a_block_size_of_one_gates_on_the_full_resolution_error() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");

        let (r, a) = checkerboard();
        write_png(&reference, &r);
        write_png(&actual, &a);

        let outcome =
            compare_images(&reference, &actual, 0.0, 1, None, 1.0).expect("both images must load");

        assert!(!outcome.passed);
        assert_eq!(outcome.block_metrics.rmse, outcome.metrics.rmse);
    }

    // The other half of the contract: a difference that moves the mean is not
    // averaged away, however large the block.
    #[test]
    fn a_systematic_shift_survives_the_block_average() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");

        write_png(&reference, &image(2, 2, &[0.0; 12]));
        write_png(&actual, &image(2, 2, &[1.0; 12]));

        let outcome =
            compare_images(&reference, &actual, 0.5, 2, None, 1.0).expect("both images must load");

        assert!(!outcome.passed);
        assert_eq!(outcome.block_metrics.rmse, 1.0);
    }

    // The diff is for a human to look at, so it keeps the resolution the
    // renderer produced rather than the grid the gate measured on.
    #[test]
    fn the_difference_image_is_written_at_full_resolution() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");
        let diff = dir.path().join("diff.png");

        write_png(&reference, &image(2, 2, &[0.0; 12]));
        write_png(&actual, &image(2, 2, &[1.0; 12]));

        let outcome = compare_images(&reference, &actual, 0.0, 2, Some(&diff), 1.0)
            .expect("both images must load");

        assert!(outcome.diff_written);
        assert_eq!(load_png(&diff).expect("readable").dimensions(), (2, 2));
    }
}
