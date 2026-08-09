//! Failures that prevent validation from running at all.
use std::path::PathBuf;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum ToolError {
    #[error("cannot access file '{path}': {source}")]
    Io {
        path: PathBuf,

        #[source]
        source: std::io::Error,
    },

    #[error("cannot parse scene file '{path}': {source}")]
    Json {
        path: PathBuf,

        #[source]
        source: serde_json::Error,
    },

    #[error("the embedded JSON schema failed to compile: {0}")]
    Schema(#[from] jsonschema::ValidationError<'static>),

    #[error(
        "internal inconsistency: '{path}' matches the schema but the tool's scene model rejected it ({source}); please report this as a bug"
    )]
    Model {
        path: PathBuf,

        #[source]
        source: serde_json::Error,
    },

    #[error("unsupported image format '{path}': expected .png or .exr")]
    UnknownImageFormat {
        path: PathBuf
    },

    #[error("cannot decode PNG '{path}': {source}")]
    Png {
        path: PathBuf,

        #[source]
        source: png::DecodingError,
    },

    #[error("cannot decode EXR '{path}': {source}")]
    Exr {
        path: PathBuf,

        #[source]
        source: exr::error::Error,
    },

    #[error("unsupported PNG in '{path}': {details}")]
    UnsupportedPng {
        path: PathBuf,
        details: String,
    },

    #[error("cannot write PNG '{path}': {source}")]
    PngWrite {
        path: PathBuf,

        #[source]
        source: png::EncodingError,
    },
}
