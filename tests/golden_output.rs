use std::fs;
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

#[test]
fn golden_output_cases_match_references() {
    for case in [
        "data/mini_utahps_daily",
        "data/mini_utahps_hourly",
        "data/mini_utahps_new_inputformat",
        "data/res_casc_A",
        "data/res_casc_B",
    ] {
        run_case(case);
    }
}

fn run_case(case: &str) {
    let repo = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let case_dir = repo.join(case);
    let reference_dir = case_dir.join("output");
    assert!(
        reference_dir.exists(),
        "missing reference output {reference_dir:?}"
    );

    let run_dir = unique_run_dir(case);
    fs::create_dir_all(run_dir.join("output")).unwrap();
    for entry in fs::read_dir(&case_dir).unwrap() {
        let entry = entry.unwrap();
        if entry.file_type().unwrap().is_file() {
            fs::copy(entry.path(), run_dir.join(entry.file_name())).unwrap();
        }
    }
    rewrite_global_file(
        &find_global_file(&case_dir),
        &run_dir.join("global.txt"),
        &run_dir,
    );

    herss::run(run_dir.join("global.txt").to_str().unwrap()).unwrap();

    let expected_files = regular_filenames(&reference_dir);
    let actual_files = regular_filenames(&run_dir.join("output"));
    assert_eq!(actual_files, expected_files, "file set mismatch for {case}");

    for filename in expected_files {
        assert_files_match(
            &reference_dir.join(&filename),
            &run_dir.join("output").join(&filename),
        );
    }

    let _ = fs::remove_dir_all(run_dir);
}

fn find_global_file(case_dir: &Path) -> PathBuf {
    let mut candidates = fs::read_dir(case_dir)
        .unwrap()
        .map(|entry| entry.unwrap().path())
        .filter(|path| {
            path.file_name()
                .unwrap()
                .to_string_lossy()
                .starts_with("global")
                && path.extension().is_some_and(|ext| ext == "txt")
        })
        .collect::<Vec<_>>();
    candidates.sort();
    candidates
        .into_iter()
        .next()
        .unwrap_or_else(|| panic!("missing global file in {case_dir:?}"))
}

fn rewrite_global_file(source: &Path, destination: &Path, run_dir: &Path) {
    let input = fs::read_to_string(source).unwrap();
    let mut output = String::new();
    for line in input.lines() {
        let trimmed = line.trim_start();
        if trimmed.starts_with("INPUTDIR ") {
            output.push_str(&format!("INPUTDIR {}/\n", run_dir.display()));
        } else if trimmed.starts_with("OUTPUTDIR ") {
            output.push_str(&format!("OUTPUTDIR {}/output/\n", run_dir.display()));
        } else {
            output.push_str(line);
            output.push('\n');
        }
    }
    fs::write(destination, output).unwrap();
}

fn regular_filenames(directory: &Path) -> Vec<String> {
    let mut files = fs::read_dir(directory)
        .unwrap()
        .map(|entry| entry.unwrap())
        .filter(|entry| entry.file_type().unwrap().is_file())
        .map(|entry| entry.file_name().to_string_lossy().into_owned())
        .collect::<Vec<_>>();
    files.sort();
    files
}

fn assert_files_match(expected: &Path, actual: &Path) {
    let expected_text = normalize(&fs::read_to_string(expected).unwrap());
    let actual_text = normalize(&fs::read_to_string(actual).unwrap());
    let expected_lines = expected_text.lines().collect::<Vec<_>>();
    let actual_lines = actual_text.lines().collect::<Vec<_>>();
    assert_eq!(
        actual_lines.len(),
        expected_lines.len(),
        "line count mismatch in {}",
        actual.display()
    );

    for (line_idx, (expected_line, actual_line)) in
        expected_lines.iter().zip(actual_lines.iter()).enumerate()
    {
        let expected_tokens = expected_line.split_whitespace().collect::<Vec<_>>();
        let actual_tokens = actual_line.split_whitespace().collect::<Vec<_>>();
        assert_eq!(
            actual_tokens.len(),
            expected_tokens.len(),
            "token count mismatch in {} on line {}",
            actual.display(),
            line_idx + 1
        );
        for (token_idx, (expected_token, actual_token)) in
            expected_tokens.iter().zip(actual_tokens.iter()).enumerate()
        {
            match (expected_token.parse::<f64>(), actual_token.parse::<f64>()) {
                (Ok(expected_number), Ok(actual_number)) => {
                    let tolerance = 0.005_f64.max(expected_number.abs() * 1e-9);
                    assert!(
                        (actual_number - expected_number).abs() <= tolerance,
                        "numeric mismatch in {} on line {}, token {}: expected {}, got {}",
                        actual.display(),
                        line_idx + 1,
                        token_idx + 1,
                        expected_token,
                        actual_token
                    );
                }
                _ => assert_eq!(
                    actual_token,
                    expected_token,
                    "text mismatch in {} on line {}, token {}",
                    actual.display(),
                    line_idx + 1,
                    token_idx + 1
                ),
            }
        }
    }
}

fn normalize(input: &str) -> String {
    input.replace('\r', "")
}

fn unique_run_dir(case: &str) -> PathBuf {
    let safe_case = case.replace('/', "_");
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_nanos();
    std::env::temp_dir().join(format!(
        "herss_golden_{}_{}_{}",
        safe_case,
        std::process::id(),
        nanos
    ))
}
