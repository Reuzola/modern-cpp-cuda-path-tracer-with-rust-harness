use crate::error::ToolError;
use exr::prelude::read_first_rgba_layer_from_file;
use std::fs::File;
use std::io::BufReader;
use std::path::Path;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ImageFormat {
    Png,
    Exr,
}

/// Detects the image format from the file extension, case-insensitively.
#[must_use]
pub fn format_from_path(path: &Path) -> Option<ImageFormat> {
    let ext = path.extension().and_then(|e| e.to_str())?;
    let ext_lower = ext.to_ascii_lowercase();
    match ext_lower.as_str() {
        "png" => Some(ImageFormat::Png),
        "exr" => Some(ImageFormat::Exr),
        _ => None,
    }
}

#[derive(Debug, Clone)]
pub struct Image {
    pub width: u32,
    pub height: u32,

    /// RGB triples, row-major, y = 0 is the top scanline: the engine's Film layout.
    /// Invariant: len() == width * height * 3, upheld by the loaders.
    pub pixels: Vec<f32>,
}

impl Image {
    #[must_use]
    pub fn dimensions(&self) -> (u32, u32) {
        (self.width, self.height)
    }
}

/// Loads an 8-bit RGB or RGBA PNG. Alpha is dropped; values are normalised to [0, 1].
pub fn load_png(path: &Path) -> Result<Image, ToolError> {
    let file = File::open(path).map_err(|source| ToolError::Io {
        path: path.to_path_buf(),
        source,
    })?;
    let decoder = png::Decoder::new(BufReader::new(file));

    let mut reader = decoder.read_info().map_err(|source| ToolError::Png {
        path: path.to_path_buf(),
        source,
    })?;
    let mut buffer = vec![0u8; reader.output_buffer_size()];

    let info = reader
        .next_frame(&mut buffer)
        .map_err(|source| ToolError::Png {
            path: path.to_path_buf(),
            source,
        })?;
    if info.bit_depth != png::BitDepth::Eight {
        return Err(ToolError::UnsupportedPng {
            path: path.to_path_buf(),
            details: format!("expected 8-bit channels, found {:?}", info.bit_depth),
        });
    }

    let stride = match info.color_type {
        png::ColorType::Rgb => 3,
        png::ColorType::Rgba => 4,
        other => {
            return Err(ToolError::UnsupportedPng {
                path: path.to_path_buf(),
                details: format!("expected RGB or RGBA, found {:?}", other),
            });
        }
    };

    let bytes = &buffer[..info.buffer_size()];
    let pixel_count = (info.width as usize) * (info.height as usize);
    let mut pixels = Vec::with_capacity(pixel_count * 3);
    for chunk in bytes.chunks_exact(stride) {
        pixels.push(f32::from(chunk[0]) / 255.0);
        pixels.push(f32::from(chunk[1]) / 255.0);
        pixels.push(f32::from(chunk[2]) / 255.0);
    }

    Ok(Image {
        width: info.width,
        height: info.height,
        pixels,
    })
}

// Carries the row width alongside the buffer: the per-pixel closure needs it to index.
struct ExrBuffer {
    width: usize,
    pixels: Vec<f32>,
}

/// Loads a scanline EXR's first RGB layer. Alpha is dropped; values stay linear and unbounded.
pub fn load_exr(path: &Path) -> Result<Image, ToolError> {
    let image = read_first_rgba_layer_from_file(
        path,
        |resolution, _channels| ExrBuffer {
            width: resolution.width(),
            pixels: vec![0.0f32; resolution.width() * resolution.height() * 3],
        },
        |buffer, position, (r, g, b, _a): (f32, f32, f32, f32)| {
            let index = (position.y() * buffer.width + position.x()) * 3;
            buffer.pixels[index] = r;
            buffer.pixels[index + 1] = g;
            buffer.pixels[index + 2] = b;
        },
    )
    .map_err(|source| ToolError::Exr {
        path: path.to_path_buf(),
        source,
    })?;

    let buffer = image.layer_data.channel_data.pixels;

    Ok(Image {
        width: buffer.width as u32,
        height: (buffer.pixels.len() / (buffer.width * 3)) as u32,
        pixels: buffer.pixels,
    })
}

/// Loads an image, choosing the decoder from the file extension.
pub fn load_image(path: &Path) -> Result<Image, ToolError> {
    let format = format_from_path(path).ok_or_else(|| ToolError::UnknownImageFormat {
        path: path.to_path_buf(),
    })?;

    match format {
        ImageFormat::Png => load_png(path),
        ImageFormat::Exr => load_exr(path),
    }
}

#[must_use]
pub fn block_mean(image: &Image, block: u32) -> Image {
    assert!(block > 0, "block size must be greater than 0");
    if block == 1 {
        return image.clone();
    }

    let out_w = image.width.div_ceil(block);
    let out_h = image.height.div_ceil(block);

    let mut pixels: Vec<f32> = Vec::with_capacity((out_w * out_h * 3) as usize);
    for by in 0..out_h {
        for bx in 0..out_w {
            let x_start = bx * block;
            let y_start = by * block;
            let x_end = (x_start + block).min(image.width);
            let y_end = (y_start + block).min(image.height);
            let pixel_count = (x_end - x_start) * (y_end - y_start);

            let mut sum = [0.0f64; 3];
            for y in y_start..y_end {
                for x in x_start..x_end {
                    let index = (y * image.width + x) as usize * 3;
                    sum[0] += f64::from(image.pixels[index]);
                    sum[1] += f64::from(image.pixels[index + 1]);
                    sum[2] += f64::from(image.pixels[index + 2]);
                }
            }

            let avg_r = sum[0] / f64::from(pixel_count);
            let avg_g = sum[1] / f64::from(pixel_count);
            let avg_b = sum[2] / f64::from(pixel_count);

            pixels.push(avg_r as f32);
            pixels.push(avg_g as f32);
            pixels.push(avg_b as f32);
        }
    }

    Image {
        width: out_w,
        height: out_h,
        pixels,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn image(width: u32, height: u32, channels: &[f32]) -> Image {
        Image {
            width,
            height,
            pixels: channels.to_vec(),
        }
    }

    #[test]
    fn a_block_of_one_returns_the_image_unchanged() {
        let source = image(2, 1, &[0.25, 0.5, 0.75, 1.0, 0.0, 0.5]);

        let averaged = block_mean(&source, 1);

        assert_eq!(averaged.dimensions(), source.dimensions());
        assert_eq!(averaged.pixels, source.pixels);
    }

    // Every mean below lands on a value f32 represents exactly, so these
    // compare exactly: a rounding difference here would be a real defect.
    #[test]
    fn a_full_block_is_the_mean_of_its_pixels() {
        let source = image(
            2,
            2,
            &[0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0],
        );

        let averaged = block_mean(&source, 2);

        assert_eq!(averaged.dimensions(), (1, 1));
        assert_eq!(averaged.pixels, vec![0.25, 0.25, 0.25]);
    }

    #[test]
    fn the_channels_are_averaged_independently() {
        let source = image(2, 1, &[1.0, 0.0, 0.0, 0.0, 1.0, 0.0]);

        let averaged = block_mean(&source, 2);

        assert_eq!(averaged.pixels, vec![0.5, 0.5, 0.0]);
    }

    // The case a fixed block*block divisor gets wrong: the trailing block holds
    // one pixel, so dividing by four would darken it to a quarter and turn the
    // image's own edge into a difference.
    #[test]
    fn an_edge_block_divides_by_its_own_pixel_count() {
        let source = image(3, 1, &[0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]);

        let averaged = block_mean(&source, 2);

        assert_eq!(averaged.dimensions(), (2, 1));
        assert_eq!(averaged.pixels, vec![0.5, 0.5, 0.5, 1.0, 1.0, 1.0]);
    }

    #[test]
    fn the_output_dimensions_round_up() {
        let source = image(5, 3, &[0.0; 5 * 3 * 3]);

        let averaged = block_mean(&source, 2);

        assert_eq!(averaged.dimensions(), (3, 2));
        assert_eq!(averaged.pixels.len(), 3 * 2 * 3);
    }

    #[test]
    fn a_block_larger_than_the_image_averages_the_whole_image() {
        let source = image(2, 1, &[1.0, 1.0, 1.0, 0.0, 0.0, 0.0]);

        let averaged = block_mean(&source, 64);

        assert_eq!(averaged.dimensions(), (1, 1));
        assert_eq!(averaged.pixels, vec![0.5, 0.5, 0.5]);
    }

    #[test]
    #[should_panic(expected = "block size must be greater than 0")]
    fn a_block_size_of_zero_is_a_programming_error() {
        let _ = block_mean(&image(1, 1, &[0.0, 0.0, 0.0]), 0);
    }

    #[test]
    fn recognises_the_two_supported_extensions() {
        assert_eq!(
            format_from_path(Path::new("render.png")),
            Some(ImageFormat::Png)
        );
        assert_eq!(
            format_from_path(Path::new("render.exr")),
            Some(ImageFormat::Exr)
        );
    }

    #[test]
    fn extension_matching_ignores_case() {
        assert_eq!(
            format_from_path(Path::new("render.PNG")),
            Some(ImageFormat::Png)
        );
        assert_eq!(
            format_from_path(Path::new("render.ExR")),
            Some(ImageFormat::Exr)
        );
    }

    #[test]
    fn other_extensions_are_rejected() {
        assert_eq!(format_from_path(Path::new("render.jpg")), None);
        assert_eq!(format_from_path(Path::new("render.ppm")), None);
    }

    #[test]
    fn a_path_without_an_extension_is_rejected() {
        assert_eq!(format_from_path(Path::new("render")), None);
    }

    // A leading dot makes a hidden file, not an extension: ".png" is a name.
    #[test]
    fn a_dotfile_is_not_an_extension() {
        assert_eq!(format_from_path(Path::new(".png")), None);
    }

    #[test]
    fn only_the_last_component_supplies_the_extension() {
        assert_eq!(
            format_from_path(Path::new("out/v1.2/render.png")),
            Some(ImageFormat::Png)
        );
        assert_eq!(format_from_path(Path::new("out.png/render")), None);
    }
}
