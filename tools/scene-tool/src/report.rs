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

#[cfg(test)]
mod tests {
    use super::*;

    fn error(location: &str, message: &str) -> Diagnostic {
        Diagnostic::error(location.to_string(), message.to_string())
    }

    fn warning(location: &str, message: &str) -> Diagnostic {
        Diagnostic::warning(location.to_string(), message.to_string())
    }

    #[test]
    fn severity_prints_in_lower_case() {
        assert_eq!(Severity::Error.to_string(), "error");
        assert_eq!(Severity::Warning.to_string(), "warning");
    }

    #[test]
    fn the_constructors_set_the_severity() {
        assert_eq!(error("/a", "boom").severity, Severity::Error);
        assert_eq!(warning("/a", "hmm").severity, Severity::Warning);
    }

    #[test]
    fn a_new_report_is_empty_and_clean() {
        let report = Report::default();

        assert!(report.is_empty());
        assert!(!report.has_errors());
        assert_eq!(report.count(Severity::Error), 0);
        assert_eq!(report.count(Severity::Warning), 0);
    }

    // The distinction the CLI's exit code depends on: warnings are findings,
    // but only errors are failures.
    #[test]
    fn a_warning_only_report_is_not_empty_but_has_no_errors() {
        let mut report = Report::default();
        report.push(warning("/render/background", "the render will be black"));

        assert!(!report.is_empty());
        assert!(!report.has_errors());
    }

    #[test]
    fn one_error_among_warnings_is_enough_to_fail() {
        let mut report = Report::default();
        report.push(warning("/a", "hmm"));
        report.push(error("/b", "boom"));
        report.push(warning("/c", "hmm"));

        assert!(report.has_errors());
        assert_eq!(report.count(Severity::Error), 1);
        assert_eq!(report.count(Severity::Warning), 2);
    }

    #[test]
    fn extend_appends_every_diagnostic() {
        let mut report = Report::default();
        report.push(error("/a", "first"));
        report.extend(vec![error("/b", "second"), warning("/c", "third")]);

        assert_eq!(report.count(Severity::Error), 2);
        assert_eq!(report.count(Severity::Warning), 1);
    }

    #[test]
    fn extending_with_nothing_changes_nothing() {
        let mut report = Report::default();
        report.extend(Vec::new());

        assert!(report.is_empty());
    }

    #[test]
    fn an_empty_report_prints_only_the_summary() {
        assert_eq!(Report::default().to_string(), "0 errors, 0 warnings");
    }

    #[test]
    fn the_summary_is_singular_for_one() {
        let mut report = Report::default();
        report.push(error("/a", "boom"));

        assert!(
            report.to_string().ends_with("1 error, 0 warnings"),
            "{report}"
        );
    }

    #[test]
    fn diagnostics_print_in_insertion_order_above_the_summary() {
        let mut report = Report::default();
        report.push(error("/objects/0", "undefined material 'red'"));
        report.push(warning("/textures/earth", "image file not found"));

        assert_eq!(
            report.to_string(),
            "  error: /objects/0: undefined material 'red'\n\
             \x20 warning: /textures/earth: image file not found\n\
             1 error, 1 warning"
        );
    }

    // An empty location means the whole document; it prints as the root path
    // rather than as a blank gap between two colons.
    #[test]
    fn an_empty_location_prints_as_the_root() {
        let mut report = Report::default();
        report.push(error("", "the document is not an object"));

        assert!(
            report.to_string().starts_with("  error: /: "),
            "{report}"
        );
    }
}
