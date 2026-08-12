//! Structural validation against the schema embedded at build time.
use crate::error::ToolError;
use crate::report::Diagnostic;
use serde_json::Value;

// Embedded at build time so the binary is self-contained: CI can run it from
// any working directory without a --schema path to keep in sync.
const SCHEMA_SOURCE: &str = include_str!("../../../schema/scene.schema.json");

/// True when the error comes from the schema's `unevaluatedProperties` keyword.
fn is_unevaluated(schema_path: &str) -> bool {
    schema_path.ends_with("unevaluatedProperties")
}

pub fn validate_structure(document: &Value) -> Result<Vec<Diagnostic>, ToolError> {
    let schema: Value =
        serde_json::from_str(SCHEMA_SOURCE).expect("the embedded schema must be valid JSON");

    let validator = jsonschema::validator_for(&schema)?;

    // Unevaluated errors are symptoms, not causes. Collect precise errors
    // first, then drop any unevaluated noise they explain.
    let mut precise_locations = Vec::new();
    for err in validator.iter_errors(document) {
        if !is_unevaluated(&err.schema_path().to_string()) {
            precise_locations.push(err.instance_path().to_string());
        }
    }

    let mut diagnostics = Vec::new();
    for err in validator.iter_errors(document) {
        let location = err.instance_path().to_string();

        if is_unevaluated(&err.schema_path().to_string()) {
            let prefix = format!("{location}/");
            if precise_locations
                .iter()
                .any(|p| p == &location || p.starts_with(&prefix))
            {
                // Suppressed only when a precise error sits at this location or below it; an unexpected
                // field on an otherwise correct object has no such explanation and must still be reported.
                continue;
            }
        }

        diagnostics.push(Diagnostic::error(location, err.to_string()));
    }

    Ok(diagnostics)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_support::{MINIMAL_SCENE, shipped_scene_files};

    /// The one object in the minimal scene, replaced by fixtures that need a different primitive.
    const SPHERE: &str = r#"{ "type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "grey" }"#;

    fn with_object(object: &str) -> String {
        MINIMAL_SCENE.replace(SPHERE, object)
    }

    fn diagnostics(json: &str) -> Vec<Diagnostic> {
        let document: Value = serde_json::from_str(json).expect("the fixture must be valid JSON");
        validate_structure(&document).expect("the embedded schema must compile")
    }

    fn locations(diagnostics: &[Diagnostic]) -> Vec<&str> {
        diagnostics.iter().map(|d| d.location.as_str()).collect()
    }

    #[test]
    fn a_minimal_scene_is_structurally_valid() {
        assert_eq!(locations(&diagnostics(MINIMAL_SCENE)), Vec::<&str>::new());
    }

    // The schema and the shipped scenes are two halves of one contract: this
    // catches a schema tightened without updating the scenes, and the reverse.
    #[test]
    fn every_shipped_scene_is_structurally_valid() {
        for path in shipped_scene_files() {
            let text = std::fs::read_to_string(&path).expect("the scene must be readable");
            let found = diagnostics(&text);

            assert!(found.is_empty(), "{}: {:?}", path.display(), locations(&found));
        }
    }

    #[test]
    fn an_empty_document_reports_the_missing_sections() {
        let found = diagnostics("{}");

        assert!(!found.is_empty());
        assert!(found.iter().all(|d| d.location.is_empty()), "{:?}", locations(&found));
    }

    #[test]
    fn a_wrong_version_is_reported_at_its_own_location() {
        let found = diagnostics(&MINIMAL_SCENE.replace(r#""version": 1"#, r#""version": 2"#));

        assert_eq!(locations(&found), ["/version"]);
    }

    #[test]
    fn a_value_outside_its_range_is_reported_at_the_field() {
        let found = diagnostics(&MINIMAL_SCENE.replace(r#""radius": 1"#, r#""radius": -1"#));

        assert_eq!(locations(&found), ["/objects/0/radius"]);
    }

    #[test]
    fn a_value_of_the_wrong_type_is_reported_at_the_field() {
        let found = diagnostics(&MINIMAL_SCENE.replace(r#""vfov": 40"#, r#""vfov": "wide""#));

        assert_eq!(locations(&found), ["/camera/vfov"]);
    }

    // The if/then branches only evaluate `type` and `name`, so anything the
    // matched branch does not name is unevaluated and therefore rejected.
    #[test]
    fn an_unknown_field_on_a_well_typed_object_is_rejected() {
        let found = diagnostics(&MINIMAL_SCENE.replace(r#""radius": 1"#, r#""radius": 1, "colour": "red""#));

        assert_eq!(locations(&found), ["/objects/0"]);
    }

    // With an unrecognised `type` no branch applies, so every sibling field
    // becomes unevaluated. Those follow from the bad type and are suppressed:
    // the one useful diagnostic is the type itself.
    #[test]
    fn a_bad_type_suppresses_the_unevaluated_noise_it_causes() {
        let found = diagnostics(&MINIMAL_SCENE.replace(r#""type": "sphere""#, r#""type": "cube""#));

        assert_eq!(locations(&found), ["/objects/0/type"]);
    }

    // Suppression is scoped to the object that carries the bad type; a real
    // error anywhere else must still surface.
    #[test]
    fn suppression_does_not_hide_errors_elsewhere() {
        let json = MINIMAL_SCENE
            .replace(r#""type": "sphere""#, r#""type": "cube""#)
            .replace(r#""width": 4"#, r#""width": 0"#);

        // Bound, not inlined: `found` borrows from this vector, so it must
        // outlive the statement that produces it.
        let reported = diagnostics(&json);
        let found = locations(&reported);

        assert!(found.contains(&"/objects/0/type"), "{found:?}");
        assert!(found.contains(&"/render/width"), "{found:?}");
    }

    // The precise error sits at the object itself here, not below it, so
    // suppression has to treat an equal location as an explanation too.
    #[test]
    fn a_missing_required_field_reports_only_the_requirement() {
        let found = diagnostics(&MINIMAL_SCENE.replace(r#""radius": 1, "#, ""));

        assert_eq!(locations(&found), ["/objects/0"]);
    }

    // The counterpart to suppression: with nothing wrong inside the object,
    // the unevaluated error is the only evidence of the stray field.
    #[test]
    fn an_unknown_field_survives_when_the_object_is_otherwise_correct() {
        let json = MINIMAL_SCENE
            .replace(r#""radius": 1"#, r#""radius": 1, "colour": "red""#)
            .replace(r#""width": 4"#, r#""width": 0"#);

        let reported = diagnostics(&json);
        let found = locations(&reported);

        assert!(found.contains(&"/objects/0"), "{found:?}");
        assert!(found.contains(&"/render/width"), "{found:?}");
    }

    #[test]
    fn a_nested_error_suppresses_the_noise_of_its_own_object_only() {
        let json = MINIMAL_SCENE.replace(
            r#""materials": { "grey": { "type": "metal", "albedo": [0.5, 0.5, 0.5] } }"#,
            r#""materials": {
                "grey": { "type": "metal", "albedo": [0.5, 0.5, 0.5], "shine": 1 },
                "rough": { "type": "metal", "albedo": [0.5, 0.5, 0.5], "fuzz": 5 }
            }"#,
        );

        let reported = diagnostics(&json);
        let found = locations(&reported);

        assert!(found.contains(&"/materials/grey"), "{found:?}");
        assert!(found.contains(&"/materials/rough/fuzz"), "{found:?}");
        assert!(!found.contains(&"/materials/rough"), "{found:?}");
    }

    // The mesh branch of the object schema: `filename` and `material` are both
    // required, and the failure has to land on the object rather than on the
    // whole `objects` array, which is what makes the CLI's location useful.
    #[test]
    fn a_mesh_without_a_filename_is_rejected_at_the_object() {
        let found = diagnostics(&with_object(r#"{ "type": "mesh", "material": "grey" }"#));

        assert_eq!(locations(&found), ["/objects/0"]);
    }

    // minLength on the path: a present but empty field is the schema's problem,
    // not something the loader should have to defend against at runtime.
    #[test]
    fn an_empty_mesh_filename_is_rejected() {
        let found = diagnostics(&with_object(r#"{ "type": "mesh", "filename": "", "material": "grey" }"#));

        assert_eq!(locations(&found), ["/objects/0/filename"]);
    }

    // unevaluatedProperties reaches the new branch too: the mesh `then` block
    // names only `filename` and `material`, so a sphere's field on a mesh is
    // unevaluated and therefore rejected.
    #[test]
    fn a_field_from_another_primitive_is_rejected_on_a_mesh() {
        let found = diagnostics(&with_object(
            r#"{ "type": "mesh", "filename": "bunny.obj", "material": "grey", "radius": 1 }"#,
        ));

        assert_eq!(locations(&found), ["/objects/0"]);
    }
}
