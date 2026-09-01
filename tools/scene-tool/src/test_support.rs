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

// One measured scene, as the two benchmark passes write it: timing from release, counters from release-stats
pub(crate) const TIMING_RECORD: &str = r#"{"build":{"build_type":"Release","compiler":"18.1.3 (1ubuntu1)","revision":"e28bfde4412b","scalar":"double","stats_enabled":false},"bvh":{"build_ms":0.003538,"leaves":8,"max_depth":6,"nodes":15,"trees":1},"host":{"arch":"x86_64","cpu_model":"11th Gen Intel(R) Core(TM) i7-11700K @ 3.60GHz","logical_cores":16},"memory":{"peak_rss_bytes":9961472},"render":{"height":300,"max_depth":50,"samples_per_pixel":16,"seed":0,"width":300},"runtime":{"threads":1},"scene":"./scenes/cornell_box.json","schema_version":2,"timestamp":"2026-09-01T12:30:24Z","timing":{"primary_rays":1440000,"primary_rays_per_second":847353.7210667474,"render_seconds":[1.710946273,1.705827358,1.699408363],"render_seconds_min":1.699408363,"runs":3},"traversal":null}"#;
pub(crate) const STATS_RECORD: &str = r#"{"build":{"build_type":"Release","compiler":"18.1.3 (1ubuntu1)","revision":"e28bfde4412b","scalar":"double","stats_enabled":true},"bvh":{"build_ms":0.003826,"leaves":8,"max_depth":6,"nodes":15,"trees":1},"host":{"arch":"x86_64","cpu_model":"11th Gen Intel(R) Core(TM) i7-11700K @ 3.60GHz","logical_cores":16},"memory":{"peak_rss_bytes":9961472},"render":{"height":300,"max_depth":50,"samples_per_pixel":16,"seed":0,"width":300},"runtime":{"threads":1},"scene":"./scenes/cornell_box.json","schema_version":2,"timestamp":"2026-09-01T12:31:15Z","timing":{"primary_rays":1440000,"primary_rays_per_second":841581.9340448895,"render_seconds":[1.767066673,1.713064407,1.711063346],"render_seconds_min":1.711063346,"runs":3},"traversal":{"leaf_tests":10912061,"node_tests":104549521,"ray_queries":7890607}}"#;

pub(crate) fn parse_scene(json: &str) -> Scene {
    serde_json::from_str(json).expect("the document must deserialize")
}

/// The minimal scene under a sky. Most rules are unrelated to lighting, and the
/// black-scene lint would otherwise fire in every fixture derived from it.
pub(crate) fn lit_scene() -> String {
    MINIMAL_SCENE.replace(
        r#""background": [0, 0, 0]"#,
        r#""background": [0.7, 0.8, 1.0]"#,
    )
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

    assert!(
        paths.len() >= 9,
        "expected the full scene set, found {}",
        paths.len()
    );
    paths
}
