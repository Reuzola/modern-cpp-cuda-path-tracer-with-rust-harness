//! Orchestrates the validation pipeline: parse, structure, model, semantics.
use crate::error::ToolError;
use crate::report::Report;
use crate::scene::Scene;
use crate::semantics::validate_semantics;
use serde_json::Value;
use std::path::Path;

/// Reads and parses a raw JSON document from the given file path.
pub fn load_document(path: &Path) -> Result<Value, ToolError> {
    let content = std::fs::read_to_string(path).map_err(|source| ToolError::Io {
        path: path.to_path_buf(),
        source,
    })?;

    let value = serde_json::from_str(&content).map_err(|source| ToolError::Json {
        path: path.to_path_buf(),
        source,
    })?;

    Ok(value)
}

pub fn validate_scene_file(path: &Path) -> Result<Report, ToolError> {
    let document = load_document(path)?;
    let mut report = Report::default();

    report.extend(crate::schema::validate_structure(&document)?);

    // A structurally invalid document cannot produce a meaningful model.
    if report.has_errors() {
        return Ok(report);
    }

    let scene: Scene = serde_json::from_value(document).map_err(|source| ToolError::Model {
        path: path.to_path_buf(),
        source,
    })?;

    let base_dir = path.parent().unwrap_or_else(|| Path::new(""));
    report.extend(validate_semantics(&scene, base_dir));

    Ok(report)
}
