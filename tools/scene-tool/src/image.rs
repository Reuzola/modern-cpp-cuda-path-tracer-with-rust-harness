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

#[derive(Debug)]
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
    let file = File::open(path).map_err(|source| ToolError::Io { path: path.to_path_buf(), source })?;
    let decoder = png::Decoder::new(BufReader::new(file));

    let mut reader = decoder.read_info().map_err(|source| ToolError::Png { path: path.to_path_buf(), source })?;
    let mut buffer = vec![0u8; reader.output_buffer_size()];

    let info = reader.next_frame(&mut buffer).map_err(|source| ToolError::Png { path: path.to_path_buf(), source })?;
    if info.bit_depth != png::BitDepth::Eight {
        return Err(ToolError::UnsupportedPng {
            path: path.to_path_buf(), details: format!("expected 8-bit channels, found {:?}", info.bit_depth)
        });
    }

    let stride = match info.color_type {
        png::ColorType::Rgb => 3,
        png::ColorType::Rgba => 4,
        other => return Err(ToolError::UnsupportedPng {
            path: path.to_path_buf(), details: format!("expected RGB or RGBA, found {:?}", other)
        }),
    };

    let bytes = &buffer[..info.buffer_size()];
    let pixel_count = (info.width as usize) * (info.height as usize);
    let mut pixels = Vec::with_capacity(pixel_count * 3);
    for chunk in bytes.chunks_exact(stride) {
        pixels.push(f32::from(chunk[0]) / 255.0);
        pixels.push(f32::from(chunk[1]) / 255.0);
        pixels.push(f32::from(chunk[2]) / 255.0);
    }

    Ok(Image { width: info.width, height: info.height, pixels })
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
        |resolution, _channels| {
            ExrBuffer {width: resolution.width(), pixels: vec![0.0f32; resolution.width() * resolution.height() * 3]}
        },
        |buffer, position, (r, g, b, _a): (f32, f32, f32, f32)| {
            let index = (position.y() * buffer.width + position.x()) * 3;
            buffer.pixels[index] = r;
            buffer.pixels[index + 1] = g;
            buffer.pixels[index + 2] = b;
        },
    )
    .map_err(|source| ToolError::Exr { path: path.to_path_buf(), source })?;

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
