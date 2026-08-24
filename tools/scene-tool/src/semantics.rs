//! Rules a JSON Schema cannot express: name resolution, ordering, geometry, placement, then importance targets, then whole-scene lints.
use crate::report::Diagnostic;
use crate::scene::{Material, Object, ObjectOrRef, Scene, Texture};
use std::collections::HashSet;
use std::path::Path;

/// Names defined so far, mirroring the loader's single pass over `objects`.
#[derive(Default)]
struct NameTable {
    defined: HashSet<String>,
    sampleable: HashSet<String>,
    used_materials: HashSet<String>,
}

fn dot(a: &[f64; 3], b: &[f64; 3]) -> f64 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

fn cross(a: &[f64; 3], b: &[f64; 3]) -> [f64; 3] {
    [ a[1] * b[2] - a[2] * b[1],
      a[2] * b[0] - a[0] * b[2],
      a[0] * b[1] - a[1] * b[0], ]
}

fn check_material(name: &str, location: &str, scene: &Scene, names: &mut NameTable, diagnostics: &mut Vec<Diagnostic>) {
    if !scene.materials.contains_key(name) {
        diagnostics.push(Diagnostic::error(location.to_string(), format!("undefined material '{name}'")));
    }
    names.used_materials.insert(name.to_string());
}

fn check_texture(texture: &Texture, location: &str, base_dir: &Path, diagnostics: &mut Vec<Diagnostic>) {
    match texture {
        Texture::Image { filename } => {
            let resolved = base_dir.join(filename);
            if !resolved.is_file() {
                diagnostics.push(Diagnostic::warning(location.to_string(), format!("image file not found: '{}'", resolved.display())));
            }
        }

        Texture::Checker { even, odd, .. } => {
            let loc_even = format!("{location}/even");
            check_texture(even, &loc_even, base_dir, diagnostics);

            let loc_odd = format!("{location}/odd");
            check_texture(odd, &loc_odd, base_dir, diagnostics);
        }

        _ => {}
    }
}

fn check_texture_files(scene: &Scene, base_dir: &Path, diagnostics: &mut Vec<Diagnostic>) {
    for (name, texture) in &scene.textures {
        let location = format!("/textures/{name}");
        check_texture(texture, &location, base_dir, diagnostics);
    }
}

fn check_texture_refs(scene: &Scene, diagnostics: &mut Vec<Diagnostic>) {
    for (name, material) in &scene.materials {
        match material {
            Material::Lambertian { texture }
            | Material::DiffuseLight { texture }
            | Material::Isotropic { texture } if !scene.textures.contains_key(texture) => {
                diagnostics.push(Diagnostic::error(format!("/materials/{name}"), format!("undefined texture '{texture}'")));
            }
            _ => {}
        }
    }
}

fn check_black_scene(scene: &Scene, names: &NameTable, diagnostics: &mut Vec<Diagnostic>) {
    let background_is_black = scene.render.background.iter().all(|c| *c == 0.0);

    let has_light = names.used_materials.iter().any(|name| {
        matches!(scene.materials.get(name), Some(Material::DiffuseLight { .. }))
    });

    // Warning instead of error because scene is valid but all dark.
    if background_is_black && !has_light {
        diagnostics.push(Diagnostic::warning("/render/background".to_string(),
        "scene has no emissive material and a black background: the render will be entirely black".to_string()));
    }
}

fn object_name(obj: &Object) -> Option<&String> {
    match obj {
        Object::Sphere { name, .. }
        | Object::Quad { name, .. }
        | Object::Box { name, .. }
        | Object::Mesh { name, .. }
        | Object::Group { name, .. }
        | Object::Transform { name, .. }
        | Object::ConstantMedium { name, .. } => {
            name.as_ref()
        }
    }
}

fn walk_child(child: &ObjectOrRef, location: &str, scene: &Scene, names: &mut NameTable, base_dir: &Path, diagnostics: &mut Vec<Diagnostic>) {
    match child {
        ObjectOrRef::Ref(name) => {
            if !names.defined.contains(name) {
                diagnostics.push(Diagnostic::error(location.to_string(), format!("undefined object '{name}'")));
            }
        }

        ObjectOrRef::Inline(object) => {
            if matches!(object.as_ref(), Object::ConstantMedium { .. }) {
                diagnostics.push(Diagnostic::error(
                    location.to_string(),
                    "constant_medium may only appear as a top-level entry in 'objects'".to_string(),
                ));
            }
            walk_object(object, location, scene, names, base_dir, diagnostics);
        }
    }
}

fn walk_object(obj: &Object, location: &str, scene: &Scene, names: &mut NameTable, base_dir: &Path, diagnostics: &mut Vec<Diagnostic>) {
    match obj {
        Object::Sphere { material, .. }
        | Object::Box { material, .. } => {
            check_material(material, location, scene, names, diagnostics);
        }

        Object::Quad { material, u, v, .. } => {
            check_material(material, location, scene, names, diagnostics);

            let n = cross(u, v);

            // Relative test: |u x v| = |u||v| sin(theta), so dividing out the lengths makes
            // the threshold independent of scene scale. Squared to avoid the square roots.
            let epsilon = 1e-8;
            if dot(&n, &n) <= epsilon * epsilon * dot(u, u) * dot(v, v) {
                diagnostics.push(Diagnostic::error(location.to_string(), "degenerate quad: 'u' and 'v' are parallel".to_string()));
            }
        }

        // An error rather than the warning a missing image texture gets: the loader
        // substitutes a placeholder for an unreadable image, but a mesh that fails to
        // load leaves no geometry behind, so the render stops.
        Object::Mesh { filename, material, .. } => {
            check_material(material, location, scene, names, diagnostics);

            let resolved = base_dir.join(filename);
            if !resolved.is_file() {
                diagnostics.push(Diagnostic::error(location.to_string(), format!("mesh file not found: '{}'", resolved.display())));
            }
        }

        Object::Group { children, .. } => {
            for (index, child) in children.iter().enumerate() {
                let child_location = format!("{location}/children/{index}");
                walk_child(child, &child_location, scene, names, base_dir, diagnostics);
            }
        }

        Object::Transform { object, .. } => {
            let child_location = format!("{location}/object");
            walk_child(object, &child_location, scene, names, base_dir, diagnostics);
        }

        Object::ConstantMedium { boundary, phase_function, name, .. } => {
            if name.is_some() {
                diagnostics.push(Diagnostic::error(
                    location.to_string(),
                    "constant_medium cannot be named: media are not part of the object graph".to_string(),
                ));
            }

            check_material(phase_function, location, scene, names, diagnostics);

            let child_location = format!("{location}/boundary");
            walk_child(boundary, &child_location, scene, names, base_dir, diagnostics);
        }
    }

    if let Some(name) = object_name(obj) {
        if !names.defined.insert(name.clone()) {
            diagnostics.push(Diagnostic::error(location.to_string(), format!("duplicate object name '{name}'")));
        }
        if matches!(obj, Object::Sphere { .. } | Object::Quad { .. }) {
            names.sampleable.insert(name.clone());
        }
    }
}

fn check_object_refs(scene: &Scene, base_dir: &Path, diagnostics: &mut Vec<Diagnostic>) -> NameTable {
    let mut names = NameTable::default();

    for (index, object) in scene.objects.iter().enumerate() {
        let location = format!("/objects/{index}");
        walk_object(object, &location, scene, &mut names, base_dir, diagnostics);
    }
    names
}

fn check_importance_targets(scene: &Scene, names: &NameTable, diagnostics: &mut Vec<Diagnostic>) {
    for (index, name) in scene.importance_targets.iter().enumerate() {
        let location = format!("/importance_targets/{index}");

        if !names.defined.contains(name) {
            diagnostics.push(Diagnostic::error(location, format!("undefined object '{name}'")));
            continue;
        }

        if !names.sampleable.contains(name) {
            diagnostics.push(Diagnostic::error(location,
            format!("object '{name}' is not sampleable: only spheres and quads can be importance targets")));
        }
    }
}

/// Runs every rule the schema cannot express. Ordering mirrors the loader:
/// textures, then objects in document order, then importance targets.
pub fn validate_semantics(scene: &Scene, base_dir: &Path) -> Vec<Diagnostic> {
    let mut diagnostics = Vec::new();

    check_texture_refs(scene, &mut diagnostics);
    let names = check_object_refs(scene, base_dir, &mut diagnostics);
    check_importance_targets(scene, &names, &mut diagnostics);
    check_texture_files(scene, base_dir, &mut diagnostics);
    check_black_scene(scene, &names, &mut diagnostics);

    diagnostics
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::report::Severity;
    use crate::test_support::{MINIMAL_SCENE, parse_scene, lit_scene, scenes_dir, shipped_scene_files};
    use tempfile::TempDir;

    /// The one object in the minimal scene, as it appears in the fixture.
    const SPHERE: &str = r#"{ "type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "grey" }"#;

    /// A mesh referring to a file the fixture may or may not create.
    const MESH: &str = r#"{ "type": "mesh", "filename": "bunny.obj", "material": "grey" }"#;

    /// A well-formed medium; the rules below fire on where it sits, not what it holds.
    const MEDIUM: &str = r#"{ "type": "constant_medium", "density": 0.01, "phase_function": "grey",
                              "boundary": { "type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "grey" } }"#;

    /// Runs the rules against a directory containing `files`, for the rules that
    /// read the filesystem. The directory is removed when the call returns; the
    /// diagnostics own their strings, so nothing outlives it.
    fn analyse_in(json: &str, files: &[&str]) -> Vec<Diagnostic> {
        let dir = TempDir::new().expect("a temp dir must be creatable");

        for name in files {
            std::fs::write(dir.path().join(name), b"# not really an OBJ")
                .expect("the fixture must be writable");
        }

        validate_semantics(&parse_scene(json), dir.path())
    }

    fn with_objects(objects: &str) -> String {
        lit_scene().replace(SPHERE, objects)
    }

    /// Runs the rules with a base directory that holds no files; only the
    /// texture-file rule reads it, and the fixtures here declare no images.
    fn analyse(json: &str) -> Vec<Diagnostic> {
        validate_semantics(&parse_scene(json), Path::new("no/such/directory"))
    }

    fn locations(diagnostics: &[Diagnostic]) -> Vec<&str> {
        diagnostics.iter().map(|d| d.location.as_str()).collect()
    }

    /// The single expected finding, with the count asserted first so a failure
    /// reports what actually came back.
    fn only(diagnostics: &[Diagnostic]) -> &Diagnostic {
        assert_eq!(diagnostics.len(), 1, "{:?}", locations(diagnostics));
        &diagnostics[0]
    }

    #[test]
    fn a_minimal_scene_has_no_findings() {
        assert_eq!(locations(&analyse(&lit_scene())), Vec::<&str>::new());
    }

    // The shipped scenes are the tool's own regression corpus: every rule added
    // later must still accept the set the renderer is known to render.
    #[test]
    fn every_shipped_scene_is_free_of_errors() {
        for path in shipped_scene_files() {
            let text = std::fs::read_to_string(&path).expect("the scene must be readable");
            let found = validate_semantics(&parse_scene(&text), &scenes_dir());

            let errors: Vec<&str> = found
                .iter()
                .filter(|d| d.severity == Severity::Error)
                .map(|d| d.message.as_str())
                .collect();

            assert!(errors.is_empty(), "{}: {errors:?}", path.display());
        }
    }

    #[test]
    fn an_undefined_material_is_reported_at_the_object() {
        let json = with_objects(&SPHERE.replace(r#""material": "grey""#, r#""material": "gold""#));

        let found = analyse(&json);
        let diagnostic = only(&found);

        assert_eq!(diagnostic.severity, Severity::Error);
        assert_eq!(diagnostic.location, "/objects/0");
        assert!(diagnostic.message.contains("gold"), "{}", diagnostic.message);
    }

    #[test]
    fn an_undefined_texture_is_reported_at_the_material() {
        let json = lit_scene().replace(
            r#""materials": {"#,
            r#""materials": { "painted": { "type": "lambertian", "texture": "wood" },"#,
        );

        let found = analyse(&json);

        assert_eq!(only(&found).location, "/materials/painted");
    }

    #[test]
    fn parallel_edges_make_a_degenerate_quad() {
        let json = with_objects(
            r#"{ "type": "quad", "q": [0, 0, 0], "u": [1, 0, 0], "v": [2, 0, 0], "material": "grey" }"#,
        );

        let found = analyse(&json);

        assert_eq!(only(&found).location, "/objects/0");
    }

    // The degeneracy test divides out the edge lengths, so a quad stays valid
    // however small it is. An absolute epsilon would reject this one.
    #[test]
    fn a_tiny_quad_is_not_degenerate() {
        let json = with_objects(
            r#"{ "type": "quad", "q": [0, 0, 0], "u": [1e-5, 0, 0], "v": [0, 1e-5, 0], "material": "grey" }"#,
        );

        assert_eq!(locations(&analyse(&json)), Vec::<&str>::new());
    }

    #[test]
    fn a_reference_to_an_earlier_object_resolves() {
        let json = with_objects(&format!(
            r#"{}, {{ "type": "group", "children": ["ball"] }}"#,
            SPHERE.replace(r#""type": "sphere""#, r#""type": "sphere", "name": "ball""#)
        ));

        assert_eq!(locations(&analyse(&json)), Vec::<&str>::new());
    }

    // Names are registered after the walk reaches them, mirroring the loader's
    // single pass: a child may only name an object defined before it. This is
    // what makes a reference cycle structurally impossible.
    #[test]
    fn a_reference_to_a_later_object_does_not_resolve() {
        let json = with_objects(&format!(
            r#"{{ "type": "group", "children": ["ball"] }}, {}"#,
            SPHERE.replace(r#""type": "sphere""#, r#""type": "sphere", "name": "ball""#)
        ));

        let found = analyse(&json);

        assert_eq!(only(&found).location, "/objects/0/children/0");
    }

    #[test]
    fn a_duplicate_name_is_reported_at_the_second_definition() {
        let named = SPHERE.replace(r#""type": "sphere""#, r#""type": "sphere", "name": "ball""#);
        let json = with_objects(&format!("{named}, {named}"));

        let found = analyse(&json);

        assert_eq!(only(&found).location, "/objects/1");
    }

    // Media are sampled by the integrator rather than intersected, so they never
    // enter the object graph: they cannot be nested under a parent and cannot be
    // named. The C++ loader throws on both; this keeps the two answers together.
    #[test]
    fn a_nested_constant_medium_is_reported_at_the_child() {
        let json = with_objects(&format!(r#"{{ "type": "group", "children": [{MEDIUM}] }}"#));

        let found = analyse(&json);
        let diagnostic = only(&found);

        assert_eq!(diagnostic.severity, Severity::Error);
        assert_eq!(diagnostic.location, "/objects/0/children/0");
    }

    #[test]
    fn a_named_constant_medium_is_reported_at_the_object() {
        let json = with_objects(&MEDIUM.replace(
            r#""type": "constant_medium""#,
            r#""type": "constant_medium", "name": "haze""#,
        ));

        let found = analyse(&json);
        let diagnostic = only(&found);

        assert_eq!(diagnostic.severity, Severity::Error);
        assert_eq!(diagnostic.location, "/objects/0");
        assert!(diagnostic.message.contains("named"), "{}", diagnostic.message);
    }

    // The pair above only proves the rules fire; this one proves they do not
    // over-fire on the placement every shipped scene actually uses.
    #[test]
    fn a_top_level_unnamed_constant_medium_is_accepted() {
        assert_eq!(locations(&analyse(&with_objects(MEDIUM))), Vec::<&str>::new());
    }

    #[test]
    fn an_undefined_importance_target_is_reported() {
        let json = lit_scene().replace(r#""objects": ["#, r#""importance_targets": ["lamp"], "objects": ["#);

        let found = analyse(&json);

        assert_eq!(only(&found).location, "/importance_targets/0");
    }

    // Only spheres and quads implement the sampling interface; a box named as a
    // target would otherwise reach the integrator and be silently ignored.
    #[test]
    fn a_box_cannot_be_an_importance_target() {
        let json = with_objects(
            r#"{ "type": "box", "name": "crate", "a": [0, 0, 0], "b": [1, 1, 1], "material": "grey" }"#,
        )
        .replace(r#""objects": ["#, r#""importance_targets": ["crate"], "objects": ["#);

        let found = analyse(&json);
        let diagnostic = only(&found);

        assert_eq!(diagnostic.location, "/importance_targets/0");
        assert!(diagnostic.message.contains("sampleable"), "{}", diagnostic.message);
    }

    // A transform wraps its child but is not itself sampleable, so a light that
    // has been moved cannot be sampled directly. The C++ loader refuses the same
    // scene through a failed dynamic_cast; this keeps the two answers together.
    #[test]
    fn a_transform_cannot_be_an_importance_target() {
        let json = with_objects(
            r#"{ "type": "transform", "name": "moved", "translate": [1, 0, 0],
                 "object": { "type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "grey" } }"#,
        )
        .replace(r#""objects": ["#, r#""importance_targets": ["moved"], "objects": ["#);

        let found = analyse(&json);
        let diagnostic = only(&found);

        assert_eq!(diagnostic.location, "/importance_targets/0");
        assert!(diagnostic.message.contains("sampleable"), "{}", diagnostic.message);
    }

    #[test]
    fn a_missing_texture_file_is_a_warning_not_an_error() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let json = lit_scene().replace(
            r#""materials": {"#,
            r#""textures": { "skin": { "type": "image", "filename": "absent.jpg" } },
               "materials": {"#,
        );

        let found = validate_semantics(&parse_scene(&json), dir.path());
        let diagnostic = only(&found);

        assert_eq!(diagnostic.severity, Severity::Warning);
        assert_eq!(diagnostic.location, "/textures/skin");
    }

    // The rule asks whether the path resolves to a file, not whether the file
    // decodes: reading image formats is the renderer's job, not the validator's.
    #[test]
    fn a_texture_file_that_exists_produces_no_finding() {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        std::fs::write(dir.path().join("present.jpg"), b"not really a jpeg")
            .expect("the fixture must be writable");

        let json = lit_scene().replace(
            r#""materials": {"#,
            r#""textures": { "skin": { "type": "image", "filename": "present.jpg" } },
               "materials": {"#,
        );

        let found = validate_semantics(&parse_scene(&json), dir.path());

        assert_eq!(locations(&found), Vec::<&str>::new());
    }

    #[test]
    fn a_black_background_without_a_light_warns() {
        let found = analyse(MINIMAL_SCENE);
        let diagnostic = only(&found);

        assert_eq!(diagnostic.severity, Severity::Warning);
        assert_eq!(diagnostic.location, "/render/background");
    }

    #[test]
    fn a_light_the_scene_uses_silences_the_lint() {
        let json = MINIMAL_SCENE
            .replace(
                r#""materials": {"#,
                r#""textures": { "glow": { "type": "solid_color", "albedo": [4, 4, 4] } },
                   "materials": { "lamp": { "type": "diffuse_light", "texture": "glow" },"#,
            )
            .replace(r#""material": "grey" }"#, r#""material": "lamp" }"#);

        assert_eq!(locations(&analyse(&json)), Vec::<&str>::new());
    }

    // The lint asks which materials the objects actually reference, not which
    // ones the file declares: an unattached light emits nothing.
    #[test]
    fn a_declared_but_unused_light_does_not_silence_the_lint() {
        let json = MINIMAL_SCENE.replace(
            r#""materials": {"#,
            r#""textures": { "glow": { "type": "solid_color", "albedo": [4, 4, 4] } },
               "materials": { "lamp": { "type": "diffuse_light", "texture": "glow" },"#,
        );

        let found = analyse(&json);

        assert_eq!(only(&found).location, "/render/background");
    }

    // The severity is the whole point of this rule: the loader substitutes a
    // placeholder for a missing image but stops on a missing mesh, so reporting
    // this as a warning would let `validate` pass a scene that cannot render.
    #[test]
    fn a_missing_mesh_file_is_an_error_not_a_warning() {
        let found = analyse_in(&with_objects(MESH), &[]);
        let diagnostic = only(&found);

        assert_eq!(diagnostic.severity, Severity::Error);
        assert_eq!(diagnostic.location, "/objects/0");
    }

    // As with image textures, the rule asks whether the path resolves to a file
    // and stops there: parsing OBJ is the renderer's job, so a file of garbage
    // is enough to satisfy it.
    #[test]
    fn a_mesh_file_that_exists_produces_no_finding() {
        assert_eq!(locations(&analyse_in(&with_objects(MESH), &["bunny.obj"])), Vec::<&str>::new());
    }

    // A mesh goes through check_material like every other primitive, which also
    // registers it in used_materials and so feeds the black-scene lint. The file
    // is created so that the material error is the only finding.
    #[test]
    fn an_undefined_material_on_a_mesh_is_reported_at_the_object() {
        let json = with_objects(&MESH.replace(r#""material": "grey""#, r#""material": "gold""#));

        let found = analyse_in(&json, &["bunny.obj"]);
        let diagnostic = only(&found);

        assert_eq!(diagnostic.location, "/objects/0");
        assert!(diagnostic.message.contains("gold"), "{}", diagnostic.message);
    }

    // base_dir now has to reach the recursive walk, not just the top-level pass:
    // a mesh nested inside a transform is checked, and at its own location.
    #[test]
    fn a_nested_mesh_is_checked_at_the_location_holding_it() {
        let json = with_objects(&format!(r#"{{ "type": "transform", "translate": [0, 0, 0], "object": {MESH} }}"#));

        let found = analyse_in(&json, &[]);

        assert_eq!(only(&found).location, "/objects/0/object");
    }

    // A mesh enters the renderer as a BVH over its triangles, and neither the
    // node nor a triangle implements Sampleable. The C++ loader rejects the same
    // scene; this keeps the two from drifting apart.
    #[test]
    fn a_mesh_cannot_be_an_importance_target() {
        let json = with_objects(&MESH.replace(r#""type": "mesh""#, r#""type": "mesh", "name": "bunny""#))
            .replace(r#""objects": ["#, r#""importance_targets": ["bunny"], "objects": ["#);

        let found = analyse_in(&json, &["bunny.obj"]);
        let diagnostic = only(&found);

        assert_eq!(diagnostic.location, "/importance_targets/0");
        assert!(diagnostic.message.contains("sampleable"), "{}", diagnostic.message);
    }
}
