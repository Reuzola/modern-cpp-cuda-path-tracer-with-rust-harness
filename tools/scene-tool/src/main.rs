use clap::{Parser, Subcommand};
use scene_tool::validate::validate_scene_file;
use std::{path::PathBuf, process::ExitCode};

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
    }
}
