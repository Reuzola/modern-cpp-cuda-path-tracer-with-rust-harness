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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_support::{lit_scene, shipped_scene_files};
    use tempfile::TempDir;

    /// Writes `contents` as a scene file and returns the directory holding it.
    /// The directory is returned too: dropping it would delete the file.
    fn scene_file(contents: &str) -> (TempDir, std::path::PathBuf) {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let path = dir.path().join("scene.json");
        std::fs::write(&path, contents).expect("the fixture must be writable");
        (dir, path)
    }

    #[test]
    fn a_missing_file_is_an_io_error() {
        let err = load_document(Path::new("no/such/scene.json"))
            .expect_err("a missing file cannot be read");

        assert!(matches!(err, ToolError::Io { .. }), "{err}");
    }

    #[test]
    fn a_malformed_document_is_a_json_error() {
        let (_dir, path) = scene_file("{ \"version\": ");

        let err = validate_scene_file(&path).expect_err("truncated JSON cannot be parsed");

        assert!(matches!(err, ToolError::Json { .. }), "{err}");
    }

    #[test]
    fn a_valid_scene_file_reports_nothing() {
        let (_dir, path) = scene_file(&lit_scene());

        let report = validate_scene_file(&path).expect("the fixture must validate");

        assert!(report.is_empty(), "{report}");
    }

    // The pipeline stops after a structural failure: a document that does not
    // match the schema cannot produce a meaningful model, so every semantic
    // rule below would be reasoning about a shape it was never given.
    #[test]
    fn structural_errors_stop_the_pipeline_before_the_semantic_rules() {
        let json = lit_scene()
            .replace(r#""version": 2"#, r#""version": 3"#)
            .replace(r#""material": "grey" }"#, r#""material": "gold" }"#);
        let (_dir, path) = scene_file(&json);

        let report = validate_scene_file(&path).expect("the document parses as JSON");

        assert!(report.has_errors());
        assert!(!report.to_string().contains("gold"), "{report}");
    }

    // Texture paths are relative to the scene file, not to the working
    // directory: the same scene must validate from anywhere.
    #[test]
    fn texture_paths_resolve_relative_to_the_scene_file() {
        let json = lit_scene().replace(
            r#""materials": {"#,
            r#""textures": { "skin": { "type": "image", "filename": "skin.png" } },
               "materials": {"#,
        );
        let (dir, path) = scene_file(&json);
        std::fs::write(dir.path().join("skin.png"), b"stand-in").expect("the fixture must be writable");

        let report = validate_scene_file(&path).expect("the fixture must validate");

        assert!(report.is_empty(), "{report}");
    }

    #[test]
    fn every_shipped_scene_validates_without_errors() {
        for path in shipped_scene_files() {
            let report = validate_scene_file(&path).expect("the scene must be readable");

            assert!(!report.has_errors(), "{}: {report}", path.display());
        }
    }
}
