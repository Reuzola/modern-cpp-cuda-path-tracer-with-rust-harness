//! Library for processing, validating, and managing scene files.
pub mod compare;
pub mod diff;
pub mod error;
pub mod image;
pub mod metrics;
pub mod report;
pub mod scene;
pub mod schema;
pub mod semantics;
pub mod validate;

#[cfg(test)]
mod test_support;
