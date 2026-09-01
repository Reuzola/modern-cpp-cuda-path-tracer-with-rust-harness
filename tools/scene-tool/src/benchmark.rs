//! The benchmark record as written by `pathtracer --bench`, and NDJSON loading.
use crate::error::ToolError;
use serde::Deserialize;
use std::path::Path;

// Must match the record's schema_version exactly; an additive change bumps it too.
pub const SCHEMA_VERSION: u32 = 2;

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Record {
    pub schema_version: u32,
    pub scene: String,
    pub timestamp: String,
    pub host: Host,
    pub build: Build,
    pub runtime: Runtime,
    pub render: Render,
    pub timing: Timing,
    pub memory: Memory,
    pub bvh: Bvh,
    pub traversal: Option<Traversal>,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Host {
    pub cpu_model: String,
    pub arch: String,
    pub logical_cores: u32,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Build {
    pub compiler: String,
    pub build_type: String,
    pub scalar: String,
    pub stats_enabled: bool,
    pub revision: Option<String>,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Runtime {
    pub threads: u32,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Render {
    pub width: u32,
    pub height: u32,
    pub samples_per_pixel: u32,
    pub max_depth: u32,
    pub seed: u64,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Timing {
    pub runs: usize,
    pub render_seconds: Vec<f64>,
    pub render_seconds_min: Option<f64>,
    pub primary_rays: u64,
    pub primary_rays_per_second: Option<f64>,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Memory {
    pub peak_rss_bytes: Option<u64>,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Bvh {
    pub trees: u64,
    pub nodes: u64,
    pub leaves: u64,
    pub max_depth: u32,
    pub build_ms: f64,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Traversal {
    pub node_tests: u64,
    pub leaf_tests: u64,
    pub ray_queries: u64,
}

/// Reads an NDJSON benchmark file. Blank lines are
/// skipped; every other line must be a complete record.
pub fn load_records(path: &Path) -> Result<Vec<Record>, ToolError> {
    let content = std::fs::read_to_string(path).map_err(|source| ToolError::Io {
        path: path.to_path_buf(),
        source,
    })?;

    let mut records = Vec::new();

    for (index, line) in content.lines().enumerate() {
        if line.trim().is_empty() {
            continue;
        }

        let line_no = index + 1;

        let probe: VersionProbe =
            serde_json::from_str(line).map_err(|source| ToolError::Ndjson {
                path: path.to_path_buf(),
                line: line_no,
                source,
            })?;

        if probe.schema_version != SCHEMA_VERSION {
            return Err(ToolError::UnsupportedSchemaVersion {
                path: path.to_path_buf(),
                line: line_no,
                found: probe.schema_version,
                expected: SCHEMA_VERSION,
            });
        }

        let record: Record = serde_json::from_str(line).map_err(|source| ToolError::Ndjson {
            path: path.to_path_buf(),
            line: line_no,
            source,
        })?;

        if record.timing.runs != record.timing.render_seconds.len() {
            return Err(ToolError::InconsistentRecord {
                path: path.to_path_buf(),
                line: line_no,
                details: format!(
                    "runs is {} but render_seconds holds {} entries",
                    record.timing.runs,
                    record.timing.render_seconds.len()
                ),
            });
        }

        records.push(record);
    }

    Ok(records)
}

#[derive(Debug, Deserialize)]
struct VersionProbe {
    schema_version: u32,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_support::{STATS_RECORD, TIMING_RECORD};
    use tempfile::TempDir;

    /// Writes `contents` as an NDJSON file and loads it back. The temp dir is
    /// dropped on return, which is safe: the loader reads the file eagerly.
    fn load(contents: &str) -> Result<Vec<Record>, ToolError> {
        let dir = TempDir::new().expect("a temp dir must be creatable");
        let path = dir.path().join("benchmarks.ndjson");
        std::fs::write(&path, contents).expect("the fixture must be writable");

        load_records(&path)
    }

    fn load_one(contents: &str) -> Record {
        let mut records = load(contents).expect("the fixture must parse");

        assert_eq!(records.len(), 1);
        records.remove(0)
    }

    #[test]
    fn a_timing_record_parses_into_the_model() {
        let record = load_one(TIMING_RECORD);

        assert_eq!(record.schema_version, SCHEMA_VERSION);
        assert_eq!(record.scene, "./scenes/cornell_box.json");
        assert_eq!(record.host.arch, "x86_64");
        assert_eq!(record.build.scalar, "double");
        assert_eq!(record.build.revision.as_deref(), Some("e28bfde4412b"));
        assert_eq!(record.runtime.threads, 1);
        assert_eq!(record.render.samples_per_pixel, 16);
        assert_eq!(record.timing.runs, 3);
        assert_eq!(record.timing.render_seconds_min, Some(1.699408363));
        assert_eq!(record.timing.primary_rays, 1_440_000);
        assert_eq!(record.memory.peak_rss_bytes, Some(9_961_472));
        assert_eq!(record.bvh.nodes, 15);
    }

    // The two passes differ in exactly one place the comparison depends on:
    // the timing build carries no counters, the instrumented one does.
    #[test]
    fn the_two_passes_are_told_apart_by_their_counters() {
        let timing = load_one(TIMING_RECORD);
        let stats = load_one(STATS_RECORD);

        assert!(!timing.build.stats_enabled);
        assert!(timing.traversal.is_none());

        assert!(stats.build.stats_enabled);
        let counters = stats
            .traversal
            .expect("the instrumented pass carries counters");
        assert_eq!(counters.node_tests, 104_549_521);
        assert_eq!(counters.leaf_tests, 10_912_061);
        assert_eq!(counters.ray_queries, 7_890_607);
    }

    #[test]
    fn records_are_returned_in_file_order() {
        let file = format!("{TIMING_RECORD}\n{STATS_RECORD}\n");

        let records = load(&file).expect("both lines must parse");

        assert_eq!(records.len(), 2);
        assert!(!records[0].build.stats_enabled);
        assert!(records[1].build.stats_enabled);
    }

    // A file appended to by two runs, or ending in a newline, still parses.
    #[test]
    fn blank_lines_are_skipped() {
        let file = format!("\n{TIMING_RECORD}\n\n   \n{STATS_RECORD}\n");

        let records = load(&file).expect("blank lines must not be records");

        assert_eq!(records.len(), 2);
    }

    #[test]
    fn a_newer_schema_version_is_rejected_with_its_line() {
        let newer = TIMING_RECORD.replace(r#""schema_version":2"#, r#""schema_version":3"#);
        let file = format!("{TIMING_RECORD}\n{newer}\n");

        let err = load(&file).expect_err("a version this tool does not know cannot be read");

        let ToolError::UnsupportedSchemaVersion {
            line,
            found,
            expected,
            ..
        } = err
        else {
            panic!("{err}")
        };
        assert_eq!(line, 2);
        assert_eq!(found, 3);
        assert_eq!(expected, SCHEMA_VERSION);
    }

    // The probe runs first for this case: a record whose shape has moved on
    // must be reported as a version mismatch, not as a parse failure.
    #[test]
    fn a_newer_version_is_reported_before_its_unknown_fields_are() {
        let newer = TIMING_RECORD.replace(
            r#""schema_version":2"#,
            r#""schema_version":3,"cache_misses":17"#,
        );

        let err = load(&newer).expect_err("the version is checked before the shape");

        assert!(
            matches!(err, ToolError::UnsupportedSchemaVersion { .. }),
            "{err}"
        );
    }

    // deny_unknown_fields is the second half of the version contract: a field
    // added without bumping the version has to fail rather than be ignored.
    #[test]
    fn an_unknown_field_at_the_current_version_is_an_error() {
        let extended = TIMING_RECORD.replace(
            r#""schema_version":2"#,
            r#""schema_version":2,"cache_misses":17"#,
        );

        let err = load(&extended).expect_err("an unannounced field cannot be accepted");

        assert!(matches!(err, ToolError::Ndjson { .. }), "{err}");
    }

    #[test]
    fn a_malformed_line_reports_its_own_line_number() {
        let file = format!("{TIMING_RECORD}\n{{ not json\n{STATS_RECORD}\n");

        let err = load(&file).expect_err("a truncated line cannot be parsed");

        let ToolError::Ndjson { line, .. } = err else {
            panic!("{err}")
        };
        assert_eq!(line, 2);
    }

    // The schema cannot express that runs equals the length of render_seconds.
    #[test]
    fn a_run_count_that_disagrees_with_the_timings_is_an_error() {
        let broken = TIMING_RECORD.replace(r#""runs":3"#, r#""runs":4"#);

        let err = load(&broken).expect_err("the two must describe the same runs");

        assert!(matches!(err, ToolError::InconsistentRecord { .. }), "{err}");
    }

    #[test]
    fn a_missing_file_is_an_io_error() {
        let err = load_records(Path::new("no/such/benchmarks.ndjson"))
            .expect_err("a missing file cannot be read");

        assert!(matches!(err, ToolError::Io { .. }), "{err}");
    }
}
