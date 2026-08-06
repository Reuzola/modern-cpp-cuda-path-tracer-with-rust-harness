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
