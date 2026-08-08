//! Rules a JSON Schema cannot express: name resolution, ordering, geometry, then importance targets, then whole-scene lints.
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
        | Object::Group { name, .. }
        | Object::Translate { name, .. }
        | Object::RotateY { name, .. }
        | Object::ConstantMedium { name, .. } => {
            name.as_ref()
        }
    }
}

fn walk_child(child: &ObjectOrRef, location: &str, scene: &Scene, names: &mut NameTable, diagnostics: &mut Vec<Diagnostic>) {
    match child {
        ObjectOrRef::Ref(name) => {
            if !names.defined.contains(name) {
                diagnostics.push(Diagnostic::error(location.to_string(), format!("undefined object '{name}'")));
            }
        }

        ObjectOrRef::Inline(object) => {
            walk_object(object, location, scene, names, diagnostics);
        }
    }
}

fn walk_object(obj: &Object, location: &str, scene: &Scene, names: &mut NameTable, diagnostics: &mut Vec<Diagnostic>) {
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

        Object::Group { children, .. } => {
            for (index, child) in children.iter().enumerate() {
                let child_location = format!("{location}/children/{index}");
                walk_child(child, &child_location, scene, names, diagnostics);
            }
        }

        Object::Translate { object, .. }
        | Object::RotateY { object, .. } => {
            let child_location = format!("{location}/object");
            walk_child(object, &child_location, scene, names, diagnostics);
        }

        Object::ConstantMedium { boundary, phase_function, .. } => {
            check_material(phase_function, location, scene, names, diagnostics);

            let child_location = format!("{location}/boundary");
            walk_child(boundary, &child_location, scene, names, diagnostics);
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

fn check_object_refs(scene: &Scene, diagnostics: &mut Vec<Diagnostic>) -> NameTable {
    let mut names = NameTable::default();

    for (index, object) in scene.objects.iter().enumerate() {
        let location = format!("/objects/{index}");
        walk_object(object, &location, scene, &mut names, diagnostics);
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
    let names = check_object_refs(scene, &mut diagnostics);
    check_importance_targets(scene, &names, &mut diagnostics);
    check_texture_files(scene, base_dir, &mut diagnostics);
    check_black_scene(scene, &names, &mut diagnostics);

    diagnostics
}
