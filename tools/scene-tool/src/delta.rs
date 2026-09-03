//! Per-metric change between two benchmark runs, and the verdict a threshold assigns to it.
use crate::{benchmark::Record, regression::ScenePair};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Direction {
    LowerIsBetter,
    HigherIsBetter,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Verdict {
    Gain,
    Regression,
    Unchanged,
}

#[derive(Debug)]
pub struct Delta {
    pub label: &'static str,
    pub baseline: Option<f64>,
    pub current: Option<f64>,
    pub direction: Direction,
    pub verdict: Verdict,
}

impl Delta {
    #[must_use]
    pub fn relative_change(&self) -> Option<f64> {
        let (b, c) = (self.baseline?, self.current?);
        if b == 0.0 {
            return None;
        }

        Some((c - b) / b)
    }
}

fn verdict(change: Option<f64>, direction: Direction, threshold: f64) -> Verdict {
    let Some(diff) = change else {
        return Verdict::Unchanged;
    };

    if diff.abs() < threshold {
        return Verdict::Unchanged;
    }

    match (direction, diff > 0.0) {
        (Direction::LowerIsBetter, true) => Verdict::Regression,
        (Direction::LowerIsBetter, false) => Verdict::Gain,
        (Direction::HigherIsBetter, true) => Verdict::Gain,
        (Direction::HigherIsBetter, false) => Verdict::Regression,
    }
}

fn metric(
    label: &'static str,
    baseline: Option<f64>,
    current: Option<f64>,
    direction: Direction,
    threshold: f64,
) -> Delta {
    let mut delta = Delta {
        label,
        baseline,
        current,
        direction,
        verdict: Verdict::Unchanged,
    };

    delta.verdict = verdict(delta.relative_change(), direction, threshold);
    delta
}

fn timing_seconds(record: Option<&Record>) -> Option<f64> {
    record?.timing.render_seconds_min
}

fn throughput(record: Option<&Record>) -> Option<f64> {
    record?.timing.primary_rays_per_second.map(|rps| rps / 1e6)
}

fn peak_rss(record: Option<&Record>) -> Option<f64> {
    record?
        .memory
        .peak_rss_bytes
        .map(|b| b as f64 / (1024.0 * 1024.0))
}

fn build_ms(record: Option<&Record>) -> Option<f64> {
    Some(record?.bvh.build_ms)
}

fn tests_per_ray(record: Option<&Record>) -> Option<f64> {
    let traversal = record?.traversal.as_ref()?;
    if traversal.ray_queries == 0 {
        return None;
    }

    let total_tests = traversal.node_tests + traversal.leaf_tests;
    Some(total_tests as f64 / traversal.ray_queries as f64)
}

fn ray_queries(record: Option<&Record>) -> Option<f64> {
    let traversal = record?.traversal.as_ref()?;
    Some(traversal.ray_queries as f64)
}

#[derive(Debug)]
pub struct SceneDeltas {
    pub scene: String,
    pub metrics: Vec<Delta>,
}

pub fn scene_deltas(pair: &ScenePair, threshold: f64) -> SceneDeltas {
    let (tb, tc) = (pair.baseline.timing, pair.current.timing);
    let (sb, sc) = (pair.baseline.stats, pair.current.stats);

    let metrics = vec![
        metric(
            "render (s)",
            timing_seconds(tb),
            timing_seconds(tc),
            Direction::LowerIsBetter,
            threshold,
        ),
        metric(
            "throughput (Mray/s)",
            throughput(tb),
            throughput(tc),
            Direction::HigherIsBetter,
            threshold,
        ),
        metric(
            "peak RSS (MiB)",
            peak_rss(tb),
            peak_rss(tc),
            Direction::LowerIsBetter,
            threshold,
        ),
        metric(
            "BVH build (ms)",
            build_ms(tb),
            build_ms(tc),
            Direction::LowerIsBetter,
            threshold,
        ),
        metric(
            "tests/ray",
            tests_per_ray(sb),
            tests_per_ray(sc),
            Direction::LowerIsBetter,
            threshold,
        ),
        metric(
            "ray queries",
            ray_queries(sb),
            ray_queries(sc),
            Direction::LowerIsBetter,
            threshold,
        ),
    ];

    SceneDeltas {
        scene: pair.scene.clone(),
        metrics,
    }
}

#[derive(Debug)]
pub struct Comparison {
    pub scenes: Vec<SceneDeltas>,
    pub threshold: f64,
}

impl Comparison {
    #[must_use]
    pub fn has_regression(&self) -> bool {
        self.scenes
            .iter()
            .any(|s| s.metrics.iter().any(|m| m.verdict == Verdict::Regression))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::regression::{ScenePair, SceneRecords};
    use crate::test_support::{STATS_RECORD, TIMING_RECORD};

    /// The noise floor the benchmark set was characterised at.
    const THRESHOLD: f64 = 0.02;

    fn record(json: &str) -> Record {
        serde_json::from_str(json).expect("the fixture must be a valid record")
    }

    fn pair<'a>(
        baseline: (Option<&'a Record>, Option<&'a Record>),
        current: (Option<&'a Record>, Option<&'a Record>),
    ) -> ScenePair<'a> {
        ScenePair {
            scene: "cornell_box".to_string(),
            baseline: SceneRecords {
                timing: baseline.0,
                stats: baseline.1,
            },
            current: SceneRecords {
                timing: current.0,
                stats: current.1,
            },
        }
    }

    fn find<'a>(deltas: &'a SceneDeltas, label: &str) -> &'a Delta {
        deltas
            .metrics
            .iter()
            .find(|m| m.label == label)
            .unwrap_or_else(|| panic!("the set must carry '{label}'"))
    }

    fn assert_close(actual: f64, expected: f64) {
        assert!(
            (actual - expected).abs() < 1e-3,
            "expected {expected}, got {actual}"
        );
    }

    // --- verdict logic, independent of any record ---

    #[test]
    fn a_change_under_the_threshold_is_noise() {
        let quicker = metric(
            "m",
            Some(100.0),
            Some(99.0),
            Direction::LowerIsBetter,
            THRESHOLD,
        );
        let slower = metric(
            "m",
            Some(100.0),
            Some(101.0),
            Direction::LowerIsBetter,
            THRESHOLD,
        );

        assert_eq!(quicker.verdict, Verdict::Unchanged);
        assert_eq!(slower.verdict, Verdict::Unchanged);
    }

    // The threshold itself counts as a finding: the comparison is `< threshold`,
    // so a change of exactly the noise floor is reported rather than swallowed.
    #[test]
    fn a_change_exactly_at_the_threshold_counts() {
        let delta = metric(
            "m",
            Some(100.0),
            Some(102.0),
            Direction::LowerIsBetter,
            THRESHOLD,
        );

        assert_eq!(delta.verdict, Verdict::Regression);
    }

    #[test]
    fn the_direction_decides_which_way_is_a_gain() {
        let lower = metric(
            "m",
            Some(100.0),
            Some(50.0),
            Direction::LowerIsBetter,
            THRESHOLD,
        );
        let higher = metric(
            "m",
            Some(100.0),
            Some(50.0),
            Direction::HigherIsBetter,
            THRESHOLD,
        );

        assert_eq!(lower.verdict, Verdict::Gain);
        assert_eq!(higher.verdict, Verdict::Regression);
    }

    #[test]
    fn a_value_missing_on_one_side_has_no_change_and_no_verdict() {
        let delta = metric("m", Some(100.0), None, Direction::LowerIsBetter, THRESHOLD);

        assert_eq!(delta.relative_change(), None);
        assert_eq!(delta.verdict, Verdict::Unchanged);
    }

    // A scene small enough to round its build time to zero must not divide by it.
    #[test]
    fn a_zero_baseline_yields_no_change_rather_than_infinity() {
        let delta = metric(
            "m",
            Some(0.0),
            Some(0.5),
            Direction::LowerIsBetter,
            THRESHOLD,
        );

        assert_eq!(delta.relative_change(), None);
        assert_eq!(delta.verdict, Verdict::Unchanged);
    }

    // --- metric extraction from real records ---

    // The label set and its order are the table's columns; both are fixed here
    // rather than at the point the table is printed.
    #[test]
    fn the_metric_set_is_fixed_and_ordered() {
        let timing = record(TIMING_RECORD);
        let stats = record(STATS_RECORD);
        let pair = pair((Some(&timing), Some(&stats)), (Some(&timing), Some(&stats)));

        let deltas = scene_deltas(&pair, THRESHOLD);

        let labels: Vec<&str> = deltas.metrics.iter().map(|m| m.label).collect();
        assert_eq!(
            labels,
            [
                "render (s)",
                "throughput (Mray/s)",
                "peak RSS (MiB)",
                "BVH build (ms)",
                "tests/ray",
                "ray queries",
            ]
        );
        assert_eq!(deltas.scene, "cornell_box");
    }

    #[test]
    fn values_are_read_and_converted_to_their_reported_units() {
        let timing = record(TIMING_RECORD);
        let stats = record(STATS_RECORD);
        let pair = pair((Some(&timing), Some(&stats)), (Some(&timing), Some(&stats)));

        let deltas = scene_deltas(&pair, THRESHOLD);

        assert_close(
            find(&deltas, "render (s)").baseline.expect("timed"),
            1.699_408_363,
        );
        assert_close(
            find(&deltas, "throughput (Mray/s)")
                .baseline
                .expect("timed"),
            0.847_353,
        );
        assert_close(
            find(&deltas, "peak RSS (MiB)").baseline.expect("measured"),
            9.5,
        );
        // (104,549,521 + 10,912,061) / 7,890,607
        assert_close(
            find(&deltas, "tests/ray").baseline.expect("instrumented"),
            14.632_8,
        );
        assert_close(
            find(&deltas, "ray queries").baseline.expect("instrumented"),
            7_890_607.0,
        );
    }

    #[test]
    fn a_run_compared_against_itself_reports_nothing() {
        let timing = record(TIMING_RECORD);
        let stats = record(STATS_RECORD);
        let pair = pair((Some(&timing), Some(&stats)), (Some(&timing), Some(&stats)));

        let deltas = scene_deltas(&pair, THRESHOLD);

        assert!(
            deltas
                .metrics
                .iter()
                .all(|m| m.verdict == Verdict::Unchanged),
            "{deltas:?}"
        );
        assert_eq!(find(&deltas, "render (s)").relative_change(), Some(0.0));
    }

    #[test]
    fn a_faster_render_is_a_gain() {
        let baseline = record(TIMING_RECORD);
        let current = record(&TIMING_RECORD.replace(
            r#""render_seconds_min":1.699408363"#,
            r#""render_seconds_min":1.5"#,
        ));
        let pair = pair((Some(&baseline), None), (Some(&current), None));

        let deltas = scene_deltas(&pair, THRESHOLD);

        assert_eq!(find(&deltas, "render (s)").verdict, Verdict::Gain);
        assert_close(
            find(&deltas, "render (s)")
                .relative_change()
                .expect("both timed"),
            -0.1173,
        );
    }

    #[test]
    fn a_slower_render_is_a_regression() {
        let baseline = record(TIMING_RECORD);
        let current = record(&TIMING_RECORD.replace(
            r#""render_seconds_min":1.699408363"#,
            r#""render_seconds_min":2.0"#,
        ));
        let pair = pair((Some(&baseline), None), (Some(&current), None));

        let deltas = scene_deltas(&pair, THRESHOLD);

        assert_eq!(find(&deltas, "render (s)").verdict, Verdict::Regression);
    }

    // Throughput is the one metric that rises when the renderer improves; a
    // sign convention applied uniformly would call this a gain.
    #[test]
    fn falling_throughput_is_a_regression() {
        let baseline = record(TIMING_RECORD);
        let current = record(&TIMING_RECORD.replace(
            r#""primary_rays_per_second":847353.7210667474"#,
            r#""primary_rays_per_second":700000.0"#,
        ));
        let pair = pair((Some(&baseline), None), (Some(&current), None));

        let deltas = scene_deltas(&pair, THRESHOLD);

        assert_eq!(
            find(&deltas, "throughput (Mray/s)").verdict,
            Verdict::Regression
        );
    }

    #[test]
    fn fewer_tests_per_ray_is_a_gain() {
        let baseline = record(STATS_RECORD);
        let current =
            record(&STATS_RECORD.replace(r#""node_tests":104549521"#, r#""node_tests":80000000"#));
        let pair = pair((None, Some(&baseline)), (None, Some(&current)));

        let deltas = scene_deltas(&pair, THRESHOLD);

        assert_eq!(find(&deltas, "tests/ray").verdict, Verdict::Gain);
    }

    // Without the instrumented pass the counter columns are empty rather than
    // absent: the table keeps its shape across scenes.
    #[test]
    fn a_run_without_counters_still_carries_the_counter_metrics() {
        let timing = record(TIMING_RECORD);
        let pair = pair((Some(&timing), None), (Some(&timing), None));

        let deltas = scene_deltas(&pair, THRESHOLD);

        assert_eq!(deltas.metrics.len(), 6);
        assert_eq!(find(&deltas, "tests/ray").baseline, None);
        assert_eq!(find(&deltas, "ray queries").current, None);
        assert!(find(&deltas, "render (s)").baseline.is_some());
    }

    // The timing figures are read from the release pass only: the instrumented
    // build carries the counters in the hot loop and its timing does not count.
    #[test]
    fn timing_is_not_taken_from_the_instrumented_pass() {
        let stats = record(STATS_RECORD);
        let pair = pair((None, Some(&stats)), (None, Some(&stats)));

        let deltas = scene_deltas(&pair, THRESHOLD);

        assert_eq!(find(&deltas, "render (s)").baseline, None);
        assert_eq!(find(&deltas, "BVH build (ms)").baseline, None);
        assert!(find(&deltas, "tests/ray").baseline.is_some());
    }

    #[test]
    fn one_regression_anywhere_decides_the_comparison() {
        let timing = record(TIMING_RECORD);
        let slower = record(&TIMING_RECORD.replace(
            r#""render_seconds_min":1.699408363"#,
            r#""render_seconds_min":2.0"#,
        ));
        let clean = scene_deltas(
            &pair((Some(&timing), None), (Some(&timing), None)),
            THRESHOLD,
        );
        let broken = scene_deltas(
            &pair((Some(&timing), None), (Some(&slower), None)),
            THRESHOLD,
        );

        assert!(
            !Comparison {
                scenes: vec![clean],
                threshold: THRESHOLD
            }
            .has_regression()
        );

        let mixed = Comparison {
            scenes: vec![
                scene_deltas(
                    &pair((Some(&timing), None), (Some(&timing), None)),
                    THRESHOLD,
                ),
                broken,
            ],
            threshold: THRESHOLD,
        };
        assert!(mixed.has_regression());
    }
}
