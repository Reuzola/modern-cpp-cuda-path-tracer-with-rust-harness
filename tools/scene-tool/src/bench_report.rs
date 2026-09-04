//! How a benchmark comparison is printed.
use crate::{
    delta::{Comparison, Delta, Verdict},
    report::pluralize,
};
use std::fmt::{self, Display, Formatter};

fn value(v: Option<f64>) -> String {
    let Some(v) = v else {
        return "-".to_string();
    };

    if v >= 100_000.0 {
        let int_str = format!("{:.0}", v);
        let mut result = String::new();
        let chars: Vec<char> = int_str.chars().collect();
        let len = chars.len();

        for (i, &ch) in chars.iter().enumerate() {
            if i > 0 && (len - i).is_multiple_of(3) {
                result.push(',');
            }
            result.push(ch);
        }
        result
    } else {
        format!("{:.3}", v)
    }
}

fn change(delta: &Delta) -> String {
    let Some(c) = delta.relative_change() else {
        return "-".to_string();
    };

    let pct = c * 100.0;
    if pct.abs() < 0.05 {
        "0.0%".to_string()
    } else {
        format!("{:+.1}%", pct)
    }
}

impl Display for Comparison {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        for (index, scene) in self.scenes.iter().enumerate() {
            if index > 0 {
                writeln!(f)?;
            }
            writeln!(f, "{}", scene.scene)?;

            for m in &scene.metrics {
                let b = value(m.baseline);
                let c = value(m.current);
                let pct = change(m);

                write!(f, "  {:<20} {:>10} {:>10} {:>8}", m.label, b, c, pct)?;
                let marker = m.verdict.marker();
                if !marker.is_empty() {
                    write!(f, "  {marker}")?;
                }
                writeln!(f)?;
            }
        }

        if !self.scenes.is_empty() {
            writeln!(f)?;
        }

        let scenes_str = pluralize(self.scenes.len(), "scene");
        let reg_str = pluralize(self.count(Verdict::Regression), "regression");
        let gain_str = pluralize(self.count(Verdict::Gain), "gain");

        write!(
            f,
            "{scenes_str}, {reg_str}, {gain_str}; threshold {:.1}%",
            self.threshold * 100.0
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::benchmark::Record;
    use crate::delta::scene_deltas;
    use crate::regression::{ScenePair, SceneRecords};
    use crate::test_support::{STATS_RECORD, TIMING_RECORD};

    const THRESHOLD: f64 = 0.02;

    fn record(json: &str) -> Record {
        serde_json::from_str(json).expect("the fixture must be a valid record")
    }

    fn pair<'a>(
        scene: &str,
        baseline: (Option<&'a Record>, Option<&'a Record>),
        current: (Option<&'a Record>, Option<&'a Record>),
    ) -> ScenePair<'a> {
        ScenePair {
            scene: scene.to_string(),
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

    fn comparison(pairs: &[ScenePair]) -> Comparison {
        Comparison {
            scenes: pairs.iter().map(|p| scene_deltas(p, THRESHOLD)).collect(),
            threshold: THRESHOLD,
        }
    }

    /// The rendered line whose metric label matches, without its indent.
    fn metric_line<'a>(rendered: &'a str, label: &str) -> &'a str {
        rendered
            .lines()
            .find(|l| l.trim_start().starts_with(label))
            .unwrap_or_else(|| panic!("'{label}' must appear in:\n{rendered}"))
    }

    /// Every indented line, which is every metric line and nothing else.
    fn metric_lines(rendered: &str) -> Vec<&str> {
        rendered.lines().filter(|l| l.starts_with("  ")).collect()
    }

    #[test]
    fn an_unchanged_comparison_marks_nothing() {
        let timing = record(TIMING_RECORD);
        let stats = record(STATS_RECORD);
        let pairs = [pair(
            "cornell_box",
            (Some(&timing), Some(&stats)),
            (Some(&timing), Some(&stats)),
        )];

        let rendered = comparison(&pairs).to_string();

        // The summary counts verdicts by name, so only the metric lines can be
        // scanned for markers.
        for line in metric_lines(&rendered) {
            assert!(!line.contains("gain"), "{line}");
            assert!(!line.contains("REGRESSION"), "{line}");
        }
    }

    #[test]
    fn a_faster_render_is_marked_as_a_gain() {
        let baseline = record(TIMING_RECORD);
        let current = record(&TIMING_RECORD.replace(
            r#""render_seconds_min":1.699408363"#,
            r#""render_seconds_min":1.5"#,
        ));
        let pairs = [pair(
            "cornell_box",
            (Some(&baseline), None),
            (Some(&current), None),
        )];

        let rendered = comparison(&pairs).to_string();
        let line = metric_line(&rendered, "render (s)");

        assert!(line.contains("-11.7%"), "{line}");
        assert!(line.ends_with("gain"), "{line}");
    }

    #[test]
    fn a_slower_render_is_marked_as_a_regression() {
        let baseline = record(TIMING_RECORD);
        let current = record(&TIMING_RECORD.replace(
            r#""render_seconds_min":1.699408363"#,
            r#""render_seconds_min":2.0"#,
        ));
        let pairs = [pair(
            "cornell_box",
            (Some(&baseline), None),
            (Some(&current), None),
        )];

        let rendered = comparison(&pairs).to_string();
        let line = metric_line(&rendered, "render (s)");

        assert!(line.contains("+17.7%"), "{line}");
        assert!(line.ends_with("REGRESSION"), "{line}");
    }

    // A run taken without the instrumented pass keeps the counter rows, with
    // every column reading as unmeasured rather than as zero.
    #[test]
    fn an_unmeasured_metric_prints_placeholders_in_every_column() {
        let timing = record(TIMING_RECORD);
        let pairs = [pair(
            "cornell_box",
            (Some(&timing), None),
            (Some(&timing), None),
        )];

        let rendered = comparison(&pairs).to_string();
        let line = metric_line(&rendered, "tests/ray");

        let fields: Vec<&str> = line.split_whitespace().collect();
        assert_eq!(fields, ["tests/ray", "-", "-", "-"], "{line}");
    }

    #[test]
    fn large_counts_are_printed_with_thousands_separators() {
        let stats = record(STATS_RECORD);
        let pairs = [pair(
            "cornell_box",
            (None, Some(&stats)),
            (None, Some(&stats)),
        )];

        let rendered = comparison(&pairs).to_string();
        let line = metric_line(&rendered, "ray queries");

        assert!(line.contains("7,890,607"), "{line}");
    }

    #[test]
    fn the_scene_name_heads_its_block_and_the_metrics_sit_under_it() {
        let timing = record(TIMING_RECORD);
        let pairs = [pair(
            "cornell_box",
            (Some(&timing), None),
            (Some(&timing), None),
        )];

        let rendered = comparison(&pairs).to_string();
        let mut lines = rendered.lines();

        assert_eq!(lines.next(), Some("cornell_box"));
        assert_eq!(metric_lines(&rendered).len(), 6);
    }

    // Fixed-width columns: with no markers every metric line is exactly as long
    // as every other, which is what keeps the numbers under one another.
    #[test]
    fn the_columns_line_up_across_metrics() {
        let timing = record(TIMING_RECORD);
        let stats = record(STATS_RECORD);
        let pairs = [pair(
            "cornell_box",
            (Some(&timing), Some(&stats)),
            (Some(&timing), Some(&stats)),
        )];

        let rendered = comparison(&pairs).to_string();
        let lines = metric_lines(&rendered);
        let width = lines[0].chars().count();

        assert!(
            lines.iter().all(|l| l.chars().count() == width),
            "{rendered}"
        );
    }

    #[test]
    fn a_blank_line_separates_two_scenes() {
        let timing = record(TIMING_RECORD);
        let pairs = [
            pair("cornell_box", (Some(&timing), None), (Some(&timing), None)),
            pair("quads", (Some(&timing), None), (Some(&timing), None)),
        ];

        let rendered = comparison(&pairs).to_string();
        let lines: Vec<&str> = rendered.lines().collect();

        let second = lines
            .iter()
            .position(|l| *l == "quads")
            .expect("the second block must appear");
        assert_eq!(lines[second - 1], "");
        assert_eq!(lines[0], "cornell_box");
    }

    // The summary is what a reader takes away, and it cannot be read without
    // the threshold that produced the verdicts.
    #[test]
    fn the_summary_counts_the_verdicts_and_names_the_threshold() {
        let timing = record(TIMING_RECORD);
        let slower = record(&TIMING_RECORD.replace(
            r#""render_seconds_min":1.699408363"#,
            r#""render_seconds_min":2.0"#,
        ));
        let faster = record(&TIMING_RECORD.replace(
            r#""render_seconds_min":1.699408363"#,
            r#""render_seconds_min":1.5"#,
        ));
        let pairs = [
            pair("cornell_box", (Some(&timing), None), (Some(&slower), None)),
            pair("quads", (Some(&timing), None), (Some(&faster), None)),
        ];

        let rendered = comparison(&pairs).to_string();
        let summary = rendered
            .lines()
            .last()
            .expect("the report must end with its summary");

        assert!(summary.contains("2 scenes"), "{summary}");
        assert!(summary.contains("1 regression,"), "{summary}");
        assert!(summary.contains("1 gain"), "{summary}");
        assert!(summary.contains("threshold 2.0%"), "{summary}");
    }

    #[test]
    fn no_line_ends_in_whitespace() {
        let timing = record(TIMING_RECORD);
        let stats = record(STATS_RECORD);
        let pairs = [pair(
            "cornell_box",
            (Some(&timing), Some(&stats)),
            (Some(&timing), Some(&stats)),
        )];

        let rendered = comparison(&pairs).to_string();

        assert!(rendered.lines().all(|l| l == l.trim_end()), "{rendered:?}");
        assert!(!rendered.ends_with('\n'), "{rendered:?}");
    }
}
