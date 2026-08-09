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
