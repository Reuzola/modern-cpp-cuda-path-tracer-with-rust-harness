use clap::{Parser, Subcommand};
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
        Commands::Validate { scene } => {
            eprintln!("not implemented yet: {scene:?}");
            ExitCode::from(2)
        }
    }
}
