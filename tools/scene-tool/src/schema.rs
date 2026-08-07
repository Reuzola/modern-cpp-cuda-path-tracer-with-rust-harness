//! Structural validation against the schema embedded at build time.
use crate::error::ToolError;
use crate::report::Diagnostic;
use serde_json::Value;
use std::collections::HashSet;

// Embedded at build time so the binary is self-contained: CI can run it from
// any working directory without a --schema path to keep in sync.
const SCHEMA_SOURCE: &str = include_str!("../../../schema/scene.schema.json");

pub fn validate_structure(document: &Value) -> Result<Vec<Diagnostic>, ToolError> {
    let schema: Value =
        serde_json::from_str(SCHEMA_SOURCE).expect("the embedded schema must be valid JSON");

    let validator = jsonschema::validator_for(&schema)?;

    // When an object's `type` is unrecognised, no if/then branch applies and the
    // schema reports every sibling field as unevaluated. Those follow from the bad
    // `type` and only distract, so they are dropped when a `type` error is present.
    let mut objects_with_bad_type = HashSet::new();
    for err in validator.iter_errors(document) {
        let location = err.instance_path().to_string();
        if let Some(parent) = location.strip_suffix("/type") {
            objects_with_bad_type.insert(parent.to_string());
        }
    }

    let mut diagnostics = Vec::new();
    for err in validator.iter_errors(document) {
        let location = err.instance_path().to_string();

        let is_unevaluated = err
            .schema_path()
            .to_string()
            .ends_with("unevaluatedProperties");
        if is_unevaluated && objects_with_bad_type.contains(&location) {
            continue;
        }

        diagnostics.push(Diagnostic::error(location, err.to_string()));
    }

    Ok(diagnostics)
}
