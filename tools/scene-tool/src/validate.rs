//! Orchestrates the validation pipeline: parse, structure, semantics.
use crate::error::ToolError;
use crate::report::Report;
use crate::scene::Scene;
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

    let _scene: Scene = serde_json::from_value(document).map_err(|source| ToolError::Model {
        path: path.to_path_buf(),
        source,
    })?;

    Ok(report)
}
