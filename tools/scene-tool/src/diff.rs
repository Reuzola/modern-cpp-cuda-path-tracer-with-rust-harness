//! Amplified per-channel difference image, written as PNG.
use crate::error::ToolError;
use crate::image::Image;
use std::fs::File;
use std::io::{BufWriter, Write};
use std::path::Path;

/// Writes an amplified absolute-difference image: |reference - actual| * gain, clamped to [0, 1].
/// Precondition: both images have identical dimensions.
pub fn write_diff_png(reference: &Image, actual: &Image, gain: f32, path: &Path) -> Result<(), ToolError> {
    debug_assert_eq!(reference.pixels.len(), actual.pixels.len());

    let mut bytes = Vec::with_capacity(reference.pixels.len());
    for (r, a) in reference.pixels.iter().copied().zip(actual.pixels.iter().copied()) {
        let d = ((r - a).abs() * gain).clamp(0.0, 1.0);
        bytes.push((d * 255.0) as u8);
    }

    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent).map_err(|source| ToolError::Io { path: parent.to_path_buf(), source })?;
    }

    let file = File::create(path).map_err(|source| ToolError::Io { path: path.to_path_buf(), source })?;
    let mut writer = BufWriter::new(file);

    let mut encoder = png::Encoder::new(&mut writer, reference.width, reference.height);
    encoder.set_color(png::ColorType::Rgb);
    encoder.set_depth(png::BitDepth::Eight);

    let mut png_writer = encoder.write_header().map_err(|source| ToolError::PngWrite { path: path.to_path_buf(), source })?;
    png_writer.write_image_data(&bytes).map_err(|source| ToolError::PngWrite { path: path.to_path_buf(), source })?;

    drop(png_writer);
    writer.flush().map_err(|source| ToolError::Io { path: path.to_path_buf(), source })?;

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::image::load_png;
    use tempfile::TempDir;

    // PNG quantises to 8 bits, so a round-tripped channel can drift by up to
    // one level (1/255). The tolerance sits just above that.
    const ONE_LEVEL: f32 = 1.0 / 255.0;

    fn image(width: u32, height: u32, channels: &[f32]) -> Image {
        Image {
            width,
            height,
            pixels: channels.to_vec(),
        }
    }

    fn assert_close(actual: f32, expected: f32) {
        assert!(
            (actual - expected).abs() <= ONE_LEVEL,
            "expected {expected}, got {actual}"
        );
    }

    #[test]
    fn round_trip_preserves_dimensions() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let path = dir.path().join("diff.png");

        let reference = image(2, 1, &[0.0, 0.0, 0.0, 0.0, 0.0, 0.0]);
        let actual = image(2, 1, &[0.1, 0.2, 0.3, 0.4, 0.5, 0.6]);

        write_diff_png(&reference, &actual, 1.0, &path).expect("the diff must be writable");

        let written = load_png(&path).expect("the diff must be readable back");

        assert_eq!(written.dimensions(), (2, 1));
        assert_eq!(written.pixels.len(), 6);
    }

    #[test]
    fn identical_images_produce_a_black_diff() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let path = dir.path().join("diff.png");

        let reference = image(1, 1, &[0.25, 0.5, 0.75]);

        write_diff_png(&reference, &reference, 10.0, &path).expect("the diff must be writable");

        let written = load_png(&path).expect("the diff must be readable back");

        assert!(written.pixels.iter().all(|v| *v == 0.0), "{:?}", written.pixels);
    }

    #[test]
    fn the_difference_is_scaled_by_the_gain() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let unit_gain = dir.path().join("unit.png");
        let amplified = dir.path().join("amplified.png");

        let reference = image(1, 1, &[0.0, 0.0, 0.0]);
        let actual = image(1, 1, &[0.05, 0.0, 0.0]);

        write_diff_png(&reference, &actual, 1.0, &unit_gain).expect("the diff must be writable");
        write_diff_png(&reference, &actual, 4.0, &amplified).expect("the diff must be writable");

        assert_close(load_png(&unit_gain).expect("readable").pixels[0], 0.05);
        assert_close(load_png(&amplified).expect("readable").pixels[0], 0.20);
    }

    #[test]
    fn the_difference_is_unsigned() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let path = dir.path().join("diff.png");

        // Channel 0 is brighter in the reference, channel 1 in the actual.
        let reference = image(1, 1, &[0.5, 0.0, 0.0]);
        let actual = image(1, 1, &[0.0, 0.5, 0.0]);

        write_diff_png(&reference, &actual, 1.0, &path).expect("the diff must be writable");

        let written = load_png(&path).expect("the diff must be readable back");

        assert_close(written.pixels[0], 0.5);
        assert_close(written.pixels[1], 0.5);
    }

    #[test]
    fn an_oversized_difference_clamps_to_white() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let path = dir.path().join("diff.png");

        let reference = image(1, 1, &[0.0, 0.0, 0.0]);
        let actual = image(1, 1, &[0.5, 1.0, 0.9]);

        write_diff_png(&reference, &actual, 10.0, &path).expect("the diff must be writable");

        let written = load_png(&path).expect("the diff must be readable back");

        assert!(written.pixels.iter().all(|v| *v == 1.0), "{:?}", written.pixels);
    }

    #[test]
    fn missing_parent_directories_are_created() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let path = dir.path().join("nested/deeper/diff.png");

        let reference = image(1, 1, &[0.0, 0.0, 0.0]);

        write_diff_png(&reference, &reference, 1.0, &path).expect("the diff must be writable");

        assert!(path.is_file());
    }
}
