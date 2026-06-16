use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    if args.len() == 2 && matches!(args[1].as_str(), "-h" | "--help") {
        print_usage(None);
        return ExitCode::SUCCESS;
    }

    if args.len() != 2 {
        print_usage(Some("not correct number of commandline arguments"));
        return ExitCode::FAILURE;
    }

    match herss::run(&args[1]) {
        Ok(()) => ExitCode::SUCCESS,
        Err(err) => {
            eprintln!("ERROR: {err}");
            ExitCode::FAILURE
        }
    }
}

fn print_usage(error: Option<&str>) {
    println!("#################################################################");
    println!("# The Hydraulic Economic River System Simulator (HERSS)");
    println!("# VERSION: {}", herss::VERSION);
    println!("# VERSION_DATE: {}", herss::VERSION_DATE);
    if let Some(error) = error {
        println!("# {error}");
    }
    println!("# USAGE:  herss globalconfigfile.txt ");
    println!("#################################################################");
}
