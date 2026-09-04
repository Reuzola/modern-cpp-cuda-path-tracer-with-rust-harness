use clap::{Parser, Subcommand};
use scene_tool::validate::validate_scene_file;
use scene_tool::{compare::compare_images, regression::compare_benchmarks};
use std::{path::PathBuf, process::ExitCode};

fn noise_threshold(s: &str) -> Result<f64, String> {
    let val: f64 = s
        .parse()
        .map_err(|_| format!("'{s}' is not a valid number"))?;

    if !val.is_finite() || val < 0.0 {
        return Err("threshold must be a finite, non-negative number".to_string());
    }
    Ok(val)
}

#[derive(Parser)]
#[command(
    name = "scene-tool",
    version,
    about = "Scene validator and golden image harness for the path tracer"
)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Validate the specified scene file for correctness
    Validate { scene: PathBuf },

    /// Compare a rendered image against a reference image
    Compare {
        reference: PathBuf,
        actual: PathBuf,

        /// Maximum RMSE that still counts as a pass
        #[arg(long, default_value_t = 0.0)]
        threshold: f64,

        /// Write an amplified difference image here when the comparison fails
        #[arg(long)]
        diff: Option<PathBuf>,

        /// Multiplier applied to the difference before it is written
        #[arg(long, default_value_t = 10.0)]
        diff_gain: f32,
    },

    /// Compare two NDJSON benchmark runs scene by scene
    BenchCompare {
        baseline: PathBuf,
        current: PathBuf,

        /// Relative change below which a difference counts as noise
        #[arg(long, default_value_t = 0.02, value_parser = noise_threshold)]
        threshold: f64,
    },
}

fn main() -> ExitCode {
    let cli = Cli::parse();
    match cli.command {
        Commands::Validate { scene } => match validate_scene_file(&scene) {
            Ok(report) => {
                if report.is_empty() {
                    eprintln!("{} is valid", scene.display());
                } else {
                    eprintln!("{}:", scene.display());
                    eprintln!("{report}");
                }

                if report.has_errors() {
                    ExitCode::from(1)
                } else {
                    ExitCode::SUCCESS
                }
            }
            Err(e) => {
                eprintln!("scene-tool: {e}");
                ExitCode::from(2)
            }
        },

        Commands::Compare {
            reference,
            actual,
            threshold,
            diff,
            diff_gain,
        } => match compare_images(&reference, &actual, threshold, diff.as_deref(), diff_gain) {
            Ok(outcome) => {
                eprintln!(
                    "rmse {:.6}, max abs diff {:.6}",
                    outcome.metrics.rmse, outcome.metrics.max_abs_diff
                );

                if let Some(db) = outcome.psnr_db {
                    eprintln!("psnr {db:.2} dB");
                }

                if outcome.diff_written {
                    eprintln!("wrote diff image");
                }

                if outcome.passed {
                    ExitCode::SUCCESS
                } else {
                    ExitCode::from(1)
                }
            }
            Err(e) => {
                eprintln!("scene-tool: {e}");
                ExitCode::from(2)
            }
        },

        Commands::BenchCompare {
            baseline,
            current,
            threshold,
        } => match compare_benchmarks(&baseline, &current, threshold) {
            Ok(comparison) => {
                println!("{comparison}");

                if comparison.has_regression() {
                    ExitCode::from(1)
                } else {
                    ExitCode::SUCCESS
                }
            }
            Err(e) => {
                eprintln!("scene-tool: {e}");
                ExitCode::from(2)
            }
        },
    }
}
