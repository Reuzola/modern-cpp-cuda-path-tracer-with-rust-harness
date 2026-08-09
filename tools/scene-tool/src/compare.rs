//! Golden image comparison: loads two images, measures the difference, optionally writes a diff.
use crate::diff::write_diff_png;
use crate::error::ToolError;
use crate::image::{Image, ImageFormat, format_from_path, load_image};
use crate::metrics::{Metrics, compute_metrics, first_non_finite, psnr};
use std::path::Path;

#[derive(Debug)]
pub struct CompareOutcome {
    pub metrics: Metrics,
    pub psnr_db: Option<f64>,
    pub passed: bool,
    pub diff_written: bool,
}

fn ensure_finite(image: &Image, path: &Path) -> Result<(), ToolError> {
    if let Some(index) = first_non_finite(image) {
        return Err(ToolError::NonFiniteValue { path: path.to_path_buf(), index });
    }
    Ok(())
}

/// Compares two images and reports whether the RMSE stays within the threshold.
pub fn compare_images(
    reference_path: &Path,
    actual_path: &Path,
    threshold: f64,
    diff_path: Option<&Path>,
    diff_gain: f32,
) -> Result<CompareOutcome, ToolError> {
    let reference_format = format_from_path(reference_path);
    let actual_format = format_from_path(actual_path);
    if reference_format != actual_format {
        return Err(ToolError::FormatMismatch { reference: reference_path.to_path_buf(), actual: actual_path.to_path_buf() });
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
            actual_height: actual.height
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

    let passed = metrics.rmse <= threshold;
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
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    fn image(width: u32, height: u32, channels: &[f32]) -> Image {
        Image { width, height, pixels: channels.to_vec() }
    }

    /// Writes an image as PNG by diffing it against black, which leaves the
    /// values untouched. Saves the tests from carrying a second encoder.
    fn write_png(path: &Path, source: &Image) {
        let black = image(source.width, source.height, &vec![0.0; source.pixels.len()]);
        write_diff_png(&black, source, 1.0, path).expect("the fixture must be writable");
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

        let outcome = compare_images(&reference, &actual, 0.0, Some(&diff), 10.0)
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

        let outcome = compare_images(&reference, &actual, 0.5, Some(&diff), 1.0)
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

        let lenient = compare_images(&reference, &actual, 0.6, None, 1.0).expect("must load");
        let strict = compare_images(&reference, &actual, 0.5, None, 1.0).expect("must load");

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

        let outcome = compare_images(&reference, &actual, 0.0, None, 1.0).expect("must load");

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

        let outcome = compare_images(&reference, &actual, 1.0, None, 1.0).expect("must load");

        assert!(outcome.psnr_db.is_some());
    }

    #[test]
    fn images_of_different_sizes_are_an_error() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let reference = dir.path().join("reference.png");
        let actual = dir.path().join("actual.png");

        write_png(&reference, &image(2, 1, &[0.0; 6]));
        write_png(&actual, &image(1, 1, &[0.0; 3]));

        let err = compare_images(&reference, &actual, 0.0, None, 1.0)
            .expect_err("mismatched dimensions cannot be compared");

        assert!(matches!(err, ToolError::DimensionMismatch { .. }), "{err}");
    }

    // Rejected before either file is opened: PNG is gamma-encoded and EXR is
    // linear, so there is no meaningful metric across the two.
    #[test]
    fn comparing_a_png_against_an_exr_is_an_error() {
        let err = compare_images(Path::new("a.png"), Path::new("b.exr"), 0.0, None, 1.0)
            .expect_err("formats must match");

        assert!(matches!(err, ToolError::FormatMismatch { .. }), "{err}");
    }

    // Two unknown extensions match each other, so the format check passes and
    // the loader is the one that refuses.
    #[test]
    fn an_unsupported_extension_is_an_error() {
        let err = compare_images(Path::new("a.jpg"), Path::new("b.jpg"), 0.0, None, 1.0)
            .expect_err("jpg is not a supported format");

        assert!(matches!(err, ToolError::UnknownImageFormat { .. }), "{err}");
    }
}
