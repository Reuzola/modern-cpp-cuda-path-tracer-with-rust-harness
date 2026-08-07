//! Findings produced by validation, and how they are printed.

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Severity {
    Error,
    Warning,
}

impl std::fmt::Display for Severity {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Severity::Error => write!(f, "error"),
            Severity::Warning => write!(f, "warning"),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Diagnostic {
    pub severity: Severity,
    pub location: String,
    pub message: String,
}

impl Diagnostic {
    pub fn error(location: String, message: String) -> Self {
        Self {
            severity: Severity::Error,
            location,
            message,
        }
    }

    pub fn warning(location: String, message: String) -> Self {
        Self {
            severity: Severity::Warning,
            location,
            message,
        }
    }
}

#[derive(Debug, Default)]
pub struct Report {
    diagnostics: Vec<Diagnostic>,
}

impl Report {
    pub fn push(&mut self, diagnostic: Diagnostic) {
        self.diagnostics.push(diagnostic);
    }

    pub fn extend(&mut self, diagnostics: Vec<Diagnostic>) {
        self.diagnostics.extend(diagnostics);
    }

    #[must_use]
    pub fn has_errors(&self) -> bool {
        self.diagnostics
            .iter()
            .any(|d| d.severity == Severity::Error)
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.diagnostics.is_empty()
    }

    #[must_use]
    pub fn count(&self, severity: Severity) -> usize {
        self.diagnostics
            .iter()
            .filter(|d| d.severity == severity)
            .count()
    }
}

fn pluralize(count: usize, singular: &str) -> String {
    if count == 1 {
        format!("{count} {singular}")
    } else {
        format!("{count} {singular}s")
    }
}

impl std::fmt::Display for Report {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for d in &self.diagnostics {
            let loc = if d.location.is_empty() {
                "/"
            } else {
                d.location.as_str()
            };
            writeln!(f, "  {}: {loc}: {}", d.severity, d.message)?;
        }

        let err_text = pluralize(self.count(Severity::Error), "error");
        let warn_text = pluralize(self.count(Severity::Warning), "warning");

        write!(f, "{err_text}, {warn_text}")
    }
}
