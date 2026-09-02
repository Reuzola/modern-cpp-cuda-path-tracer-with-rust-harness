//! Pairs two benchmark runs scene by scene and refuses the ones that cannot be compared.
use crate::{benchmark::Record, error::ToolError};
use std::{collections::BTreeMap, path::Path};

fn scene_name(record: &Record) -> Option<&str> {
    Path::new(&record.scene)
        .file_stem()
        .and_then(|stem| stem.to_str())
}

#[derive(Debug, Default, Clone, Copy)]
pub struct SceneRecords<'a> {
    pub timing: Option<&'a Record>,
    pub stats: Option<&'a Record>,
}

#[derive(Debug)]
pub struct Run<'a> {
    pub scenes: BTreeMap<String, SceneRecords<'a>>,
}

impl<'a> Run<'a> {
    pub fn from_records(records: &'a [Record], path: &Path) -> Result<Self, ToolError> {
        let mut scenes = BTreeMap::new();

        for record in records {
            let name = scene_name(record).ok_or_else(|| ToolError::MalformedRun {
                path: path.to_path_buf(),
                details: format!("scene path '{}' has no valid filename", record.scene),
            })?;

            let slot: &mut SceneRecords<'a> = scenes.entry(name.to_string()).or_default();

            let target = if record.build.stats_enabled {
                &mut slot.stats
            } else {
                &mut slot.timing
            };

            let pass_name = if record.build.stats_enabled {
                "stats"
            } else {
                "timing"
            };

            if target.is_some() {
                return Err(ToolError::MalformedRun {
                    path: path.to_path_buf(),
                    details: format!("scene '{name}' has duplicate {pass_name} pass record"),
                });
            }

            *target = Some(record);
        }

        if scenes.is_empty() {
            return Err(ToolError::MalformedRun {
                path: path.to_path_buf(),
                details: "file contains no benchmark records".to_string(),
            });
        }

        Ok(Self { scenes })
    }
}

fn check_record_pair(scene: &str, b: &Record, c: &Record, violations: &mut Vec<String>) {
    if b.render.width != c.render.width {
        violations.push(format!(
            "{scene}: width differs (baseline {}, current {})",
            b.render.width, c.render.width
        ));
    }

    if b.render.height != c.render.height {
        violations.push(format!(
            "{scene}: height differs (baseline {}, current {})",
            b.render.height, c.render.height
        ));
    }

    if b.render.samples_per_pixel != c.render.samples_per_pixel {
        violations.push(format!(
            "{scene}: samples_per_pixel differs (baseline {}, current {})",
            b.render.samples_per_pixel, c.render.samples_per_pixel
        ));
    }

    if b.render.max_depth != c.render.max_depth {
        violations.push(format!(
            "{scene}: max_depth differs (baseline {}, current {})",
            b.render.max_depth, c.render.max_depth
        ));
    }

    if b.render.seed != c.render.seed {
        violations.push(format!(
            "{scene}: seed differs (baseline {}, current {})",
            b.render.seed, c.render.seed
        ));
    }

    if b.host.arch != c.host.arch {
        violations.push(format!(
            "{scene}: arch differs (baseline {}, current {})",
            b.host.arch, c.host.arch
        ));
    }

    if b.host.cpu_model != c.host.cpu_model {
        violations.push(format!(
            "{scene}: cpu_model differs (baseline {}, current {})",
            b.host.cpu_model, c.host.cpu_model
        ));
    }

    if b.build.scalar != c.build.scalar {
        violations.push(format!(
            "{scene}: scalar differs (baseline {}, current {})",
            b.build.scalar, c.build.scalar
        ));
    }

    if b.build.build_type != c.build.build_type {
        violations.push(format!(
            "{scene}: build_type differs (baseline {}, current {})",
            b.build.build_type, c.build.build_type
        ));
    }

    if b.runtime.threads != c.runtime.threads {
        violations.push(format!(
            "{scene}: threads differs (baseline {}, current {})",
            b.runtime.threads, c.runtime.threads
        ));
    }
}

fn comparability_violations(baseline: &Run, current: &Run) -> Vec<String> {
    let mut violations = Vec::new();

    for scene in baseline.scenes.keys() {
        if !current.scenes.contains_key(scene) {
            violations.push(format!("{scene} is missing from the current run"));
        }
    }

    for scene in current.scenes.keys() {
        if !baseline.scenes.contains_key(scene) {
            violations.push(format!("{scene} is missing from the baseline run"));
        }
    }

    for (scene, base_records) in &baseline.scenes {
        let Some(curr_records) = current.scenes.get(scene) else {
            continue;
        };

        if base_records.timing.is_some() != curr_records.timing.is_some() {
            violations.push(format!(
                "{scene}: timing pass presence differs between baseline and current"
            ));
        }

        if base_records.stats.is_some() != curr_records.stats.is_some() {
            violations.push(format!(
                "{scene}: stats pass presence differs between baseline and current"
            ));
        }

        if let (Some(b), Some(c)) = (base_records.timing, curr_records.timing) {
            check_record_pair(scene, b, c, &mut violations);
        }
        if let (Some(b), Some(c)) = (base_records.stats, curr_records.stats) {
            check_record_pair(scene, b, c, &mut violations);
        }
    }

    violations
}

#[derive(Debug, Clone)]
pub struct ScenePair<'a> {
    pub scene: String,
    pub baseline: SceneRecords<'a>,
    pub current: SceneRecords<'a>,
}

pub fn pair_runs<'a>(
    baseline: &Run<'a>,
    current: &Run<'a>,
    baseline_path: &Path,
    current_path: &Path,
) -> Result<Vec<ScenePair<'a>>, ToolError> {
    let violations = comparability_violations(baseline, current);

    if !violations.is_empty() {
        let details = violations
            .into_iter()
            .map(|v| format!("  {v}"))
            .collect::<Vec<_>>()
            .join("\n");

        return Err(ToolError::Incomparable {
            baseline: baseline_path.to_path_buf(),
            current: current_path.to_path_buf(),
            details,
        });
    }

    let mut pairs = Vec::new();
    for (name, base_records) in &baseline.scenes {
        let curr_records = current.scenes[name];

        pairs.push(ScenePair {
            scene: name.clone(),
            baseline: *base_records,
            current: curr_records,
        });
    }

    Ok(pairs)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_support::{STATS_RECORD, TIMING_RECORD};

    /// The scene path both fixtures carry, as the renderer wrote it.
    const CORNELL: &str = r#""scene":"./scenes/cornell_box.json""#;

    fn record(json: &str) -> Record {
        serde_json::from_str(json).expect("the fixture must be a valid record")
    }

    /// Repoints a fixture at another scene path, which is also how a second
    /// scene is introduced into a run.
    fn with_scene(json: &str, path: &str) -> String {
        json.replace(CORNELL, &format!(r#""scene":"{path}""#))
    }

    fn run(records: &[Record]) -> Run<'_> {
        Run::from_records(records, Path::new("run.ndjson")).expect("the fixture must form a run")
    }

    fn pairs<'a>(baseline: &Run<'a>, current: &Run<'a>) -> Vec<ScenePair<'a>> {
        pair_runs(baseline, current, Path::new("baseline.ndjson"), Path::new("current.ndjson"))
            .expect("the runs must be comparable")
    }

    /// The violation list from a refused comparison.
    fn refusal<'a>(baseline: &Run<'a>, current: &Run<'a>) -> String {
        let err = pair_runs(baseline, current, Path::new("baseline.ndjson"), Path::new("current.ndjson"))
            .expect_err("the runs must not be comparable");

        let ToolError::Incomparable { details, .. } = err else { panic!("{err}") };
        details
    }

    #[test]
    fn the_two_passes_of_a_scene_land_in_one_entry() {
        let records = [record(TIMING_RECORD), record(STATS_RECORD)];

        let run = run(&records);

        assert_eq!(run.scenes.len(), 1);
        let entry = run.scenes["cornell_box"];
        assert!(!entry.timing.expect("the timing pass is present").build.stats_enabled);
        assert!(entry.stats.expect("the stats pass is present").build.stats_enabled);
    }

    // The renderer records the path as it was invoked, so the same scene reaches
    // the tool spelled differently between two runs. The key is the stem.
    #[test]
    fn the_same_scene_spelled_differently_is_one_entry() {
        let records = [
            record(TIMING_RECORD),
            record(&with_scene(STATS_RECORD, "scenes/cornell_box.json")),
        ];

        let run = run(&records);

        assert_eq!(run.scenes.len(), 1);
        assert!(run.scenes.contains_key("cornell_box"));
    }

    // Two runs concatenated into one file. Which of the duplicates is current
    // cannot be recovered, so a silent "last one wins" would pick a baseline at
    // random.
    #[test]
    fn a_repeated_pass_for_one_scene_is_not_a_single_run() {
        let records = [record(TIMING_RECORD), record(TIMING_RECORD)];

        let err = Run::from_records(&records, Path::new("run.ndjson"))
            .expect_err("a duplicated pass cannot form one run");

        let ToolError::MalformedRun { details, .. } = err else { panic!("{err}") };
        assert!(details.contains("cornell_box"), "{details}");
        assert!(details.contains("timing"), "{details}");
    }

    #[test]
    fn an_empty_run_is_rejected() {
        let err = Run::from_records(&[], Path::new("run.ndjson"))
            .expect_err("a run with no records cannot be compared");

        assert!(matches!(err, ToolError::MalformedRun { .. }), "{err}");
    }

    #[test]
    fn a_scene_path_without_a_filename_is_rejected() {
        let records = [record(&with_scene(TIMING_RECORD, "/"))];

        let err = Run::from_records(&records, Path::new("run.ndjson"))
            .expect_err("a record with no scene name cannot be keyed");

        assert!(matches!(err, ToolError::MalformedRun { .. }), "{err}");
    }

    // Insertion order is deliberately the reverse of the expected output order.
    #[test]
    fn pairs_come_back_in_scene_order() {
        let records = [
            record(&with_scene(TIMING_RECORD, "scenes/quads.json")),
            record(TIMING_RECORD),
        ];
        let baseline = run(&records);
        let current = run(&records);

        let pairs = pairs(&baseline, &current);

        assert_eq!(pairs.len(), 2);
        assert_eq!(pairs[0].scene, "cornell_box");
        assert_eq!(pairs[1].scene, "quads");
        assert!(pairs[0].baseline.timing.is_some());
        assert!(pairs[0].current.timing.is_some());
    }

    // Counters are optional: a run taken from the release build alone compares
    // fine against another one like it. The rule is that the two runs agree,
    // not that the counters exist.
    #[test]
    fn two_runs_without_counters_are_comparable() {
        let records = [record(TIMING_RECORD)];
        let baseline = run(&records);
        let current = run(&records);

        assert_eq!(pairs(&baseline, &current).len(), 1);
    }

    // Counters present on one side only: the instrumented pass was forgotten,
    // and reporting empty counter columns would read as "unchanged".
    #[test]
    fn counters_on_one_side_only_are_refused() {
        let both = [record(TIMING_RECORD), record(STATS_RECORD)];
        let timing_only = [record(TIMING_RECORD)];
        let baseline = run(&both);
        let current = run(&timing_only);

        let details = refusal(&baseline, &current);

        assert!(details.contains("stats pass presence differs"), "{details}");
    }

    #[test]
    fn a_scene_present_in_one_run_only_is_refused() {
        let two = [
            record(TIMING_RECORD),
            record(&with_scene(TIMING_RECORD, "scenes/quads.json")),
        ];
        let one = [record(TIMING_RECORD)];
        let baseline = run(&two);
        let current = run(&one);

        let details = refusal(&baseline, &current);

        assert!(details.contains("quads is missing from the current run"), "{details}");
    }

    // A changed manifest row is a changed workload, and the older number has to
    // be remeasured rather than reused.
    #[test]
    fn a_different_sample_count_is_refused_with_both_values() {
        let baseline_records = [record(TIMING_RECORD)];
        let current_records = [record(&TIMING_RECORD.replace(
            r#""samples_per_pixel":16"#,
            r#""samples_per_pixel":25"#,
        ))];
        let baseline = run(&baseline_records);
        let current = run(&current_records);

        let details = refusal(&baseline, &current);

        assert!(details.contains("samples_per_pixel differs"), "{details}");
        assert!(details.contains("baseline 16"), "{details}");
        assert!(details.contains("current 25"), "{details}");
    }

    // A figure taken on more threads is not a faster renderer.
    #[test]
    fn a_different_thread_count_is_refused() {
        let baseline_records = [record(TIMING_RECORD)];
        let current_records = [record(&TIMING_RECORD.replace(r#""threads":1"#, r#""threads":8"#))];
        let baseline = run(&baseline_records);
        let current = run(&current_records);

        let details = refusal(&baseline, &current);

        assert!(details.contains("threads differs"), "{details}");
    }

    // fmadd is baseline on AArch64, so the counters are not bit-identical
    // across architectures and the difference is not an optimisation.
    #[test]
    fn a_different_architecture_is_refused() {
        let baseline_records = [record(TIMING_RECORD)];
        let current_records =
            [record(&TIMING_RECORD.replace(r#""arch":"x86_64""#, r#""arch":"aarch64""#))];
        let baseline = run(&baseline_records);
        let current = run(&current_records);

        let details = refusal(&baseline, &current);

        assert!(details.contains("arch differs"), "{details}");
    }

    // The one field that is expected to differ: telling two revisions apart is
    // the point of the comparison.
    #[test]
    fn a_different_revision_is_not_a_violation() {
        let baseline_records = [record(TIMING_RECORD)];
        let current_records = [record(
            &TIMING_RECORD.replace(r#""revision":"e28bfde4412b""#, r#""revision":"0123456789ab""#),
        )];
        let baseline = run(&baseline_records);
        let current = run(&current_records);

        assert_eq!(pairs(&baseline, &current).len(), 1);
    }

    // Every violation is collected before the refusal, so one fix does not
    // reveal the next only on the following run.
    #[test]
    fn every_violation_is_reported_at_once() {
        let baseline_records = [record(TIMING_RECORD)];
        let current_records = [record(
            &TIMING_RECORD
                .replace(r#""samples_per_pixel":16"#, r#""samples_per_pixel":25"#)
                .replace(r#""threads":1"#, r#""threads":8"#),
        )];
        let baseline = run(&baseline_records);
        let current = run(&current_records);

        let details = refusal(&baseline, &current);

        assert!(details.contains("samples_per_pixel differs"), "{details}");
        assert!(details.contains("threads differs"), "{details}");
        assert_eq!(details.lines().count(), 2, "{details}");
        assert!(details.lines().all(|l| l.starts_with("  ")), "{details}");
    }
}
