//! Fixtures shared by the unit tests. Compiled only under `cargo test`.
use crate::scene::Scene;

/// The smallest scene the schema and the model both accept. Tests derive their own documents
/// from it with `str::replace`, so each one states only the field it is actually about.
pub(crate) const MINIMAL_SCENE: &str = r#"{
    "version": 2,
    "camera": { "lookfrom": [0, 0, 1], "lookat": [0, 0, 0], "vfov": 40 },
    "render": {
        "width": 4, "height": 4, "samples_per_pixel": 1,
        "max_depth": 2, "background": [0, 0, 0]
    },
    "materials": { "grey": { "type": "metal", "albedo": [0.5, 0.5, 0.5] } },
    "objects": [
        { "type": "sphere", "center": [0, 0, 0], "radius": 1, "material": "grey" }
    ]
}"#;

pub(crate) fn parse_scene(json: &str) -> Scene {
    serde_json::from_str(json).expect("the document must deserialize")
}

/// The minimal scene under a sky. Most rules are unrelated to lighting, and the
/// black-scene lint would otherwise fire in every fixture derived from it.
pub(crate) fn lit_scene() -> String {
    MINIMAL_SCENE.replace(r#""background": [0, 0, 0]"#, r#""background": [0.7, 0.8, 1.0]"#)
}

/// Absolute path to the repository's `scenes/` directory. Derived from the
/// crate root at compile time, so it does not depend on the working directory.
pub(crate) fn scenes_dir() -> std::path::PathBuf {
    std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../scenes")
}

/// Every `scenes/*.json` path, sorted so failures are reported in a stable order.
pub(crate) fn shipped_scene_files() -> Vec<std::path::PathBuf> {
    let mut paths: Vec<_> = std::fs::read_dir(scenes_dir())
        .expect("the scenes directory must exist")
        .map(|entry| entry.expect("the directory entry must be readable").path())
        .filter(|path| path.extension().and_then(|e| e.to_str()) == Some("json"))
        .collect();

    paths.sort();

    assert!(paths.len() >= 9, "expected the full scene set, found {}", paths.len());
    paths
}
