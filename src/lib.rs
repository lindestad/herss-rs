mod config;
mod curve;
mod dataset;
mod error;
mod model;
mod routing;
mod util;

pub use config::{GlobalConfig, NodeType};
pub use curve::ArrayCurve;
pub use dataset::Dataset;
pub use error::{HerssError, Result};
pub use model::{Herss, Riversystem};

pub const VERSION: &str = "3.1.03";
pub const VERSION_DATE: &str = "20260611";

pub(crate) const MAX_NR_NODES: usize = 30;
pub(crate) const MAX_WORDS: usize = 200;
pub(crate) const POINTS_IN_ARRAY: usize = 1000;
pub(crate) const MAX_TRAVELTIME_HOURS: f64 = 240.0;
pub(crate) const MAX_NR_POINTS_CURVE: usize = 50;
pub(crate) const MAX_NR_GENERATORS: usize = 6;
pub(crate) const MAX_NR_CASCADED_RESERVOIRS: usize = 10;
pub(crate) const N_UNIFORM_EFF_CURVE_POINTS: usize = 11;
pub(crate) const HERSS_AGGRESSIVE_ACTIONS_COST: f64 = 1000.0;
pub(crate) const GRAVITY: f64 = 9.80665;
pub(crate) const PI: f64 = std::f64::consts::PI;
pub(crate) const MOUNT_EVEREST_MASL: f64 = 8848.0;
pub(crate) const NOT_INIT_I32: i32 = 9_999_999;
pub(crate) const NOT_INIT_USIZE: usize = 9_999_999;
pub(crate) const NOT_INIT: f64 = 9_999_999.0;
pub(crate) const VERY_LARGE_NUMBER: f64 = 999_999_999.0;

pub(crate) fn m3s_to_mm3(q: f64, dt: f64) -> f64 {
    q * dt / 1_000_000.0
}

pub(crate) fn mm3_to_m3s(q: f64, dt: f64) -> f64 {
    q * 1_000_000.0 / dt
}

pub fn run(global_file: &str) -> Result<()> {
    let mut gc = GlobalConfig::from_file(global_file)?;
    gc.set_directories_and_filenames();
    gc.diagnose()?;
    gc.check_nr_steps()?;

    let mut data = Dataset::new(&gc);
    data.read_all_data(&gc)?;

    let mut herss = Herss::new(gc);
    herss.prepare_simulation(data)?;
    herss.riversystem.diagnose_configuration()?;
    herss.simulate()?;
    herss.check_water_balance()?;
    herss.global_water_balance()?;
    herss.calc_adjustment_costs()?;
    herss.riversystem.calc_simulation_profit();
    herss.write_outputs()?;
    Ok(())
}
