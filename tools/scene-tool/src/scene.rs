use serde::Deserialize;
use std::collections::BTreeMap;

#[derive(Debug, Deserialize, Default)]
#[serde(rename_all = "snake_case")]
pub enum ToneMapOperator {
    #[default]
    None,

    Reinhard,
    Aces,
}

#[derive(Debug, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
#[serde(deny_unknown_fields)]
pub enum Texture {
    SolidColor {
        albedo: [f64; 3],
    },
    Checker {
        scale: f64,
        even: Box<Texture>,
        odd: Box<Texture>,
    },
    Noise {
        scale: f64,

        #[serde(default)]
        seed: u64,
    },
    Image {
        filename: String,
    },
}

#[derive(Debug, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
#[serde(deny_unknown_fields)]
pub enum Material {
    Lambertian {
        texture: String,
    },
    Metal {
        albedo: [f64; 3],

        #[serde(default)]
        fuzz: f64,
    },
    Dielectric {
        refraction_index: f64,
    },
    DiffuseLight {
        texture: String,
    },
    Isotropic {
        texture: String,
    },
}

#[derive(Debug, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
#[serde(deny_unknown_fields)]
pub enum Object {
    Sphere {
        name: Option<String>,
        center: [f64; 3],
        center_end: Option<[f64; 3]>,
        radius: f64,
        material: String,
    },
    Quad {
        name: Option<String>,
        q: [f64; 3],
        u: [f64; 3],
        v: [f64; 3],
        material: String,
    },
    Box {
        name: Option<String>,
        a: [f64; 3],
        b: [f64; 3],
        material: String,
    },
    Group {
        name: Option<String>,
        children: Vec<ObjectOrRef>,
    },
    Translate {
        name: Option<String>,
        object: ObjectOrRef,
        offset: [f64; 3],
    },
    RotateY {
        name: Option<String>,
        object: ObjectOrRef,
        angle: f64,
    },
    ConstantMedium {
        name: Option<String>,
        boundary: ObjectOrRef,
        density: f64,
        phase_function: String,
    },
}

#[derive(Debug, Deserialize)]
#[serde(untagged)]
pub enum ObjectOrRef {
    Ref(String),
    Inline(Box<Object>),
}

impl Default for ToneMap {
    fn default() -> Self {
        Self {
            exposure: 1.0,
            operator: ToneMapOperator::default(),
        }
    }
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
#[serde(default)]
pub struct ToneMap {
    pub exposure: f64,             // default -> 1.0
    pub operator: ToneMapOperator, // default -> none
}

fn default_vup() -> [f64; 3] {
    [0.0, 1.0, 0.0]
}

fn default_focus_dist() -> f64 {
    10.0
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Camera {
    pub lookfrom: [f64; 3],
    pub lookat: [f64; 3],

    #[serde(default = "default_vup")]
    pub vup: [f64; 3],

    pub vfov: f64,

    #[serde(default)]
    pub defocus_angle: f64,

    #[serde(default = "default_focus_dist")]
    pub focus_dist: f64,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Render {
    pub width: u32,
    pub height: u32,
    pub samples_per_pixel: u32,
    pub max_depth: u32,
    pub background: [f64; 3],

    #[serde(default)]
    pub seed: u64,

    #[serde(default)]
    pub tone_map: ToneMap,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Scene {
    #[serde(rename = "$schema")]
    pub schema: Option<String>,

    pub version: u32,
    pub camera: Camera,
    pub render: Render,

    #[serde(default)]
    pub textures: BTreeMap<String, Texture>,

    pub materials: BTreeMap<String, Material>,
    pub objects: Vec<Object>,

    #[serde(default)]
    pub importance_targets: Vec<String>,
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::test_support::{MINIMAL_SCENE as MINIMAL, parse_scene as parse};

    #[test]
    fn every_shipped_scene_deserializes() {
        for path in crate::test_support::shipped_scene_files() {
            let text = std::fs::read_to_string(&path).expect("the scene must be readable");
            serde_json::from_str::<Scene>(&text)
                .unwrap_or_else(|e| panic!("{} failed to deserialize: {e}", path.display()));
        }
    }

    // The default chain the C++ loader also applies; a silent drift here would
    // make the two implementations disagree on scenes that omit these fields.
    #[test]
    fn omitted_optional_fields_fall_back_to_their_defaults() {
        let scene = parse(MINIMAL);

        assert_eq!(scene.camera.vup, [0.0, 1.0, 0.0]);
        assert_eq!(scene.camera.defocus_angle, 0.0);
        assert_eq!(scene.camera.focus_dist, 10.0);
        assert_eq!(scene.render.seed, 0);
        assert!(scene.textures.is_empty());
        assert!(scene.importance_targets.is_empty());
        assert_eq!(scene.schema, None);
    }

    #[test]
    fn an_absent_tone_map_still_yields_the_neutral_default() {
        let scene = parse(MINIMAL);

        assert_eq!(scene.render.tone_map.exposure, 1.0);
        assert!(matches!(scene.render.tone_map.operator, ToneMapOperator::None));
    }

    // `#[serde(default)]` on the struct: a partial tone_map keeps the defaults
    // for the fields it omits, rather than resetting them to zero.
    #[test]
    fn a_partial_tone_map_defaults_only_the_missing_field() {
        let json = MINIMAL.replace(
            r#""background": [0, 0, 0]"#,
            r#""background": [0, 0, 0], "tone_map": { "operator": "aces" }"#,
        );

        let scene = parse(&json);

        assert_eq!(scene.render.tone_map.exposure, 1.0);
        assert!(matches!(scene.render.tone_map.operator, ToneMapOperator::Aces));
    }

    #[test]
    fn type_tags_are_read_in_snake_case() {
        let json = MINIMAL.replace(
            r#""objects": ["#,
            r#""objects": [
                { "type": "constant_medium", "boundary": "shell", "density": 0.1,
                  "phase_function": "grey" },"#,
        );

        let scene = parse(&json);

        assert!(matches!(scene.objects[0], Object::ConstantMedium { .. }));
    }

    // Untagged: a child is either a name referring to an earlier object or an
    // inline definition, and the two are told apart by shape alone.
    #[test]
    fn a_child_is_either_a_reference_or_an_inline_object() {
        let json = MINIMAL.replace(
            r#""objects": ["#,
            r#""objects": [
                { "type": "sphere", "name": "shell", "center": [0, 0, 0],
                  "radius": 2, "material": "grey" },
                { "type": "group", "children": [
                    "shell",
                    { "type": "sphere", "center": [3, 0, 0], "radius": 1, "material": "grey" }
                ]},"#,
        );

        let scene = parse(&json);

        let Object::Group { children, .. } = &scene.objects[1] else {
            panic!("the second object must be a group");
        };

        assert!(matches!(&children[0], ObjectOrRef::Ref(name) if name == "shell"));
        assert!(matches!(children[1], ObjectOrRef::Inline(_)));
    }

    #[test]
    fn nested_textures_deserialize_recursively() {
        let json = MINIMAL.replace(
            r#""materials": {"#,
            r#""textures": {
                "board": { "type": "checker", "scale": 2,
                    "even": { "type": "solid_color", "albedo": [0, 0, 0] },
                    "odd": { "type": "noise", "scale": 4 } }
            },
            "materials": {"#,
        );

        let scene = parse(&json);

        let Some(Texture::Checker { even, odd, scale }) = scene.textures.get("board") else {
            panic!("the checker texture must be present");
        };

        assert_eq!(*scale, 2.0);
        assert!(matches!(**even, Texture::SolidColor { .. }));
        assert!(matches!(**odd, Texture::Noise { seed: 0, .. }));
    }

    // `deny_unknown_fields`, the guard that keeps this model and the C++ loader
    // from quietly diverging when one of them gains a field.
    #[test]
    fn an_unknown_field_is_rejected() {
        let json = MINIMAL.replace(r#""radius": 1"#, r#""radius": 1, "colour": "red""#);

        assert!(serde_json::from_str::<Scene>(&json).is_err());
    }

    #[test]
    fn a_missing_required_field_is_rejected() {
        let json = MINIMAL.replace(r#""vfov": 40"#, r#""vfov_typo": 40"#);

        assert!(serde_json::from_str::<Scene>(&json).is_err());
    }
}
