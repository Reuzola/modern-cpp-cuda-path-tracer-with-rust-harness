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
