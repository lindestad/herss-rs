use crate::config::{
    ChannelConfig, GeneratorConfig, GlobalConfig, NodeConfig, NodeConfigKind, NodeType,
    PowerstationConfig, ReservoirConfig,
};
use crate::curve::ArrayCurve;
use crate::dataset::Dataset;
use crate::error::{HerssError, Result};
use crate::routing::CascadedReservoirs;
use crate::util::{active_lines, parse_f64_or_zero, tokens};
use crate::{
    GRAVITY, MAX_NR_GENERATORS, MOUNT_EVEREST_MASL, NOT_INIT, NOT_INIT_I32, PI, VERY_LARGE_NUMBER,
    m3s_to_mm3, mm3_to_m3s,
};
use std::fmt::Write as _;

#[derive(Debug, Clone)]
pub(crate) struct QminPeriod {
    min_discharge: f64,
    start_day: i32,
    start_month: i32,
    end_day: i32,
    end_month: i32,
    penalty_cost: f64,
}

#[derive(Debug, Clone, Default)]
pub(crate) struct Qmin {
    timeperiods: Vec<QminPeriod>,
}

impl Qmin {
    fn calc_requirement(&self, month: i32, day: i32) -> (f64, f64) {
        let date = month * 100 + day;
        for period in &self.timeperiods {
            let start = period.start_month * 100 + period.start_day;
            let end = period.end_month * 100 + period.end_day;
            if date >= start && date <= end {
                return (period.min_discharge, period.penalty_cost);
            }
        }
        (0.0, 0.0)
    }
}

#[derive(Debug, Clone)]
pub(crate) struct Scenario {
    stps: usize,
    dt: f64,
    restprice: f64,
    price: Vec<f64>,
    action: Vec<Vec<f64>>,
    inflow: Vec<f64>,
    tot_outflow: Vec<f64>,
    tot_inflow: Vec<f64>,
    up_inflow: Vec<f64>,
    tunnelflow_m3s: Vec<f64>,
    hatchflow_m3s: Vec<f64>,
    overflow_m3s: Vec<f64>,
    auto_qmin_m3s: Vec<f64>,
    channel_storage_mm3: Vec<f64>,
    res_mm3: Vec<f64>,
    res_active_mm3: Vec<f64>,
    res_masl: Vec<f64>,
    res_fr: Vec<f64>,
    profit: Vec<f64>,
    overflow_mm3: Vec<f64>,
    income: Vec<f64>,
    cost: Vec<f64>,
    cost_qmin: Vec<f64>,
    start_stop_cost: Vec<f64>,
    adjust_cost: Vec<f64>,
    hbrutto: Vec<f64>,
    hnetto: Vec<f64>,
    power: Vec<f64>,
    estimated_eekv: Vec<f64>,
    year: Vec<i32>,
    month: Vec<i32>,
    day: Vec<i32>,
    hour: Vec<i32>,
}

impl Scenario {
    fn new(stps: usize, max_action_cols: usize) -> Self {
        Self {
            stps,
            dt: 0.0,
            restprice: NOT_INIT,
            price: vec![NOT_INIT; stps],
            action: vec![vec![NOT_INIT; max_action_cols]; stps],
            inflow: vec![0.0; stps],
            tot_outflow: vec![NOT_INIT; stps],
            tot_inflow: vec![NOT_INIT; stps],
            up_inflow: vec![0.0; stps],
            tunnelflow_m3s: vec![NOT_INIT; stps],
            hatchflow_m3s: vec![NOT_INIT; stps],
            overflow_m3s: vec![NOT_INIT; stps],
            auto_qmin_m3s: vec![NOT_INIT; stps],
            channel_storage_mm3: vec![NOT_INIT; stps],
            res_mm3: vec![NOT_INIT; stps],
            res_active_mm3: vec![NOT_INIT; stps],
            res_masl: vec![NOT_INIT; stps],
            res_fr: vec![NOT_INIT; stps],
            profit: vec![NOT_INIT; stps],
            overflow_mm3: vec![NOT_INIT; stps],
            income: vec![NOT_INIT; stps],
            cost: vec![NOT_INIT; stps],
            cost_qmin: vec![NOT_INIT; stps],
            start_stop_cost: vec![NOT_INIT; stps],
            adjust_cost: vec![0.0; stps],
            hbrutto: vec![NOT_INIT; stps],
            hnetto: vec![NOT_INIT; stps],
            power: vec![NOT_INIT; stps],
            estimated_eekv: vec![NOT_INIT; stps],
            year: vec![NOT_INIT_I32; stps],
            month: vec![NOT_INIT_I32; stps],
            day: vec![NOT_INIT_I32; stps],
            hour: vec![NOT_INIT_I32; stps],
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct NodeCommon {
    nodetype: NodeType,
    idnr: usize,
    nodename: String,
    qmin: Qmin,
    qmin_in_use: bool,
    up_res_mm3: f64,
    remaining_mm3: f64,
    upstream_remaining_mm3: f64,
    remaining_active_mm3: f64,
    upstream_remaining_active_mm3: f64,
    reservoir_idnr: usize,
    pstation_idnr: i32,
    max_adjustment_pr_day: i32,
    max_adjustment_cost: f64,
    local_energy_equivalent: f64,
    powstat_min_discharge: f64,
    auto_qmin: f64,
    start_of_stp_masl: f64,
    end_of_stp_masl: f64,
    downstream_idnr: Option<usize>,
    downstream_idnr_tunnel: Option<usize>,
    downstream_idnr_hatch: Option<usize>,
    downstream_idnr_overflow: Option<usize>,
    downstream_idnr_auto_qmin: Option<usize>,
}

impl NodeCommon {
    fn new(nodetype: NodeType, idnr: usize, nodename: String) -> Self {
        Self {
            nodetype,
            idnr,
            nodename,
            qmin: Qmin::default(),
            qmin_in_use: false,
            up_res_mm3: NOT_INIT,
            remaining_mm3: NOT_INIT,
            upstream_remaining_mm3: 0.0,
            remaining_active_mm3: NOT_INIT,
            upstream_remaining_active_mm3: 0.0,
            reservoir_idnr: crate::NOT_INIT_USIZE,
            pstation_idnr: crate::NOT_INIT_I32,
            max_adjustment_pr_day: crate::NOT_INIT_I32,
            max_adjustment_cost: crate::NOT_INIT,
            local_energy_equivalent: -NOT_INIT,
            powstat_min_discharge: NOT_INIT,
            auto_qmin: NOT_INIT,
            start_of_stp_masl: NOT_INIT,
            end_of_stp_masl: NOT_INIT,
            downstream_idnr: None,
            downstream_idnr_tunnel: None,
            downstream_idnr_hatch: None,
            downstream_idnr_overflow: None,
            downstream_idnr_auto_qmin: None,
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct Reservoir {
    reservoir_init_fr: f64,
    reservoir_init_masl: f64,
    reservoir_init_mm3: f64,
    reservoir_init_active_mm3: f64,
    active_max_volume_mm3: f64,
    hrw: f64,
    lrw: f64,
    filling_at_hrw_mm3: f64,
    filling_at_lrw_mm3: f64,
    filling_at_hatchlevel: f64,
    cost_lrw: f64,
    res_penalty: f64,
    #[allow(dead_code)]
    floodlevel_penalty: f64,
    fast_overflow: bool,
    res_mm3: f64,
    res_masl: f64,
    res_fr: f64,
    use_reservoir_geometry: bool,
    width_m: f64,
    length_m: f64,
    theta: f64,
    bottom_masl: f64,
    slope_term: f64,
    geo_denom: f64,
    use_reservoir_curve: bool,
    reservoir_curve: Vec<(f64, f64)>,
    overflow_curve: Vec<(f64, f64)>,
    min_q_hatch: f64,
    max_q_hatch: f64,
    hatch_masl: f64,
    ac_res_masl_to_mm3: Option<ArrayCurve>,
    ac_res_mm3_to_masl: Option<ArrayCurve>,
    ac_overflow_masl_to_m3s: ArrayCurve,
}

impl Reservoir {
    fn from_config(config: &ReservoirConfig) -> Result<Self> {
        let hatch = config.outlet_hatch.as_ref();
        let mut reservoir = Self {
            reservoir_init_fr: NOT_INIT,
            reservoir_init_masl: NOT_INIT,
            reservoir_init_mm3: NOT_INIT,
            reservoir_init_active_mm3: NOT_INIT,
            active_max_volume_mm3: NOT_INIT,
            hrw: config.hrw,
            lrw: config.lrw,
            filling_at_hrw_mm3: NOT_INIT,
            filling_at_lrw_mm3: NOT_INIT,
            filling_at_hatchlevel: NOT_INIT,
            cost_lrw: 0.0,
            res_penalty: config.res_penalty,
            floodlevel_penalty: config.floodlevel_penalty,
            fast_overflow: config.fast_overflow,
            res_mm3: NOT_INIT,
            res_masl: NOT_INIT,
            res_fr: NOT_INIT,
            use_reservoir_geometry: config.use_reservoir_geometry,
            width_m: config.width_m,
            length_m: config.length_m,
            theta: config.theta,
            bottom_masl: config.bottom_masl,
            slope_term: -NOT_INIT,
            geo_denom: -NOT_INIT,
            use_reservoir_curve: config.use_reservoir_curve,
            reservoir_curve: config.reservoir_curve.clone(),
            overflow_curve: config.overflow_curve.clone(),
            min_q_hatch: hatch.map(|h| h.min_q).unwrap_or(NOT_INIT),
            max_q_hatch: hatch.map(|h| h.max_q).unwrap_or(NOT_INIT),
            hatch_masl: hatch.map(|h| h.hatch_masl).unwrap_or(NOT_INIT),
            ac_res_masl_to_mm3: None,
            ac_res_mm3_to_masl: None,
            ac_overflow_masl_to_m3s: ArrayCurve::from_points(&config.overflow_curve)?,
        };
        reservoir.init_array_curves()?;
        Ok(reservoir)
    }

    fn init_array_curves(&mut self) -> Result<()> {
        if self.use_reservoir_curve {
            self.ac_res_masl_to_mm3 = Some(ArrayCurve::from_points(&self.reservoir_curve)?);
            let reverse: Vec<(f64, f64)> = self
                .reservoir_curve
                .iter()
                .map(|(masl, mm3)| (*mm3, *masl))
                .collect();
            self.ac_res_mm3_to_masl = Some(ArrayCurve::from_points(&reverse)?);
        }
        Ok(())
    }

    fn init_reservoir(&mut self, scenario: &mut Scenario) -> Result<()> {
        for value in &mut scenario.up_inflow {
            *value = 0.0;
        }

        if self.use_reservoir_curve {
            let masl_to_mm3 = self
                .ac_res_masl_to_mm3
                .as_ref()
                .ok_or_else(|| HerssError::new("Reservoir curve not initialized"))?;
            let mm3_to_masl = self
                .ac_res_mm3_to_masl
                .as_ref()
                .ok_or_else(|| HerssError::new("Reservoir curve not initialized"))?;

            self.filling_at_lrw_mm3 = masl_to_mm3.x2y(self.lrw);
            self.filling_at_hrw_mm3 = masl_to_mm3.x2y(self.hrw);
            self.active_max_volume_mm3 = self.filling_at_hrw_mm3 - self.filling_at_lrw_mm3;
            self.res_mm3 =
                self.filling_at_lrw_mm3 + self.reservoir_init_fr * self.active_max_volume_mm3;
            self.reservoir_init_mm3 = self.res_mm3;
            self.reservoir_init_active_mm3 = self.res_mm3 - self.filling_at_lrw_mm3;
            self.res_masl = mm3_to_masl.x2y(self.res_mm3);
            self.reservoir_init_masl = self.res_masl;
            if self.hatch_masl > -VERY_LARGE_NUMBER {
                self.filling_at_hatchlevel = masl_to_mm3.x2y(self.hatch_masl);
            }
        }

        if self.use_reservoir_geometry {
            if self.width_m <= 0.0 || self.length_m <= 0.0 || self.theta < 0.0 || self.theta >= 90.0
            {
                return Err(HerssError::new(
                    "Reservoir geometry is not initialized correctly",
                ));
            }
            self.slope_term = ((90.0 - self.theta) * PI / 180.0).tan();
            self.geo_denom = self.length_m * (self.slope_term + self.width_m);
            self.filling_at_lrw_mm3 = self.calc_res_volume(self.lrw)?;
            self.filling_at_hrw_mm3 = self.calc_res_volume(self.hrw)?;
            self.active_max_volume_mm3 = self.filling_at_hrw_mm3 - self.filling_at_lrw_mm3;
            self.res_mm3 =
                self.filling_at_lrw_mm3 + self.reservoir_init_fr * self.active_max_volume_mm3;
            self.reservoir_init_mm3 = self.res_mm3;
            self.reservoir_init_active_mm3 = self.res_mm3 - self.filling_at_lrw_mm3;
            self.res_masl = self.calc_res_masl(self.res_mm3)?;
            self.reservoir_init_masl = self.res_masl;
            if self.hatch_masl > -VERY_LARGE_NUMBER {
                self.filling_at_hatchlevel = self.calc_res_volume(self.hatch_masl)?;
            }
        }
        Ok(())
    }

    fn validate_settings(&self, name: &str) -> Result<()> {
        if self.reservoir_init_fr < -0.000001 || self.reservoir_init_fr > 1.5 {
            return Err(HerssError::new(format!(
                "reservoir_init_fr is out of bounds for {name}"
            )));
        }
        if self.hrw < 0.0
            || self.hrw > MOUNT_EVEREST_MASL
            || self.lrw < 0.0
            || self.lrw > MOUNT_EVEREST_MASL
        {
            return Err(HerssError::new(format!(
                "HRW/LRW is out of bounds for {name}"
            )));
        }
        if self.lrw >= self.hrw {
            return Err(HerssError::new(format!(
                "LRW must be lower than HRW for {name}"
            )));
        }
        if self.res_penalty < 0.0 || self.res_penalty > VERY_LARGE_NUMBER {
            return Err(HerssError::new(format!(
                "RES_PENALTY is out of bounds for {name}"
            )));
        }
        Ok(())
    }

    fn calc_res_volume(&self, masl: f64) -> Result<f64> {
        let h = masl - self.bottom_masl;
        if h < 0.0 {
            return Err(HerssError::new("masl is below bottom_masl"));
        }
        let area = (h * self.slope_term) + self.width_m * h;
        Ok(area * self.length_m / 1_000_000.0)
    }

    fn calc_res_masl(&self, mm3: f64) -> Result<f64> {
        if mm3 < 0.0 {
            return Err(HerssError::new("Mm3 is negative in calcResMasl"));
        }
        Ok(self.bottom_masl + mm3 * 1_000_000.0 / self.geo_denom)
    }

    fn update_masl(&mut self) -> Result<()> {
        if self.use_reservoir_geometry {
            self.res_masl = self.calc_res_masl(self.res_mm3)?;
        } else if self.use_reservoir_curve {
            self.validate_level_mm3(self.res_mm3)?;
            self.res_masl = self
                .ac_res_mm3_to_masl
                .as_ref()
                .ok_or_else(|| HerssError::new("Reservoir curve not initialized"))?
                .x2y(self.res_mm3);
        }
        Ok(())
    }

    fn validate_level_mm3(&self, level_mm3: f64) -> Result<()> {
        if self.use_reservoir_curve {
            let max = self
                .reservoir_curve
                .last()
                .map(|point| point.1)
                .unwrap_or(f64::INFINITY);
            if level_mm3 > max {
                return Err(HerssError::new(
                    "Numerical instability, there is too much water in your system",
                ));
            }
        }
        Ok(())
    }

    fn calc_overflow(&self, dt: f64) -> f64 {
        let masl_start_overflow = self
            .overflow_curve
            .first()
            .map(|point| point.0)
            .unwrap_or(self.hrw);
        if self.fast_overflow {
            if self.res_masl > masl_start_overflow {
                return (self.res_mm3 - self.filling_at_hrw_mm3).max(0.0);
            }
            return 0.0;
        }

        if self.res_masl > masl_start_overflow {
            let overflow_m3s = self.ac_overflow_masl_to_m3s.x2y(self.res_masl);
            let overflow_mm3 = m3s_to_mm3(overflow_m3s, dt);
            return overflow_mm3
                .min(self.res_mm3 - self.filling_at_hrw_mm3)
                .max(0.0);
        }
        0.0
    }
}

#[derive(Debug, Clone)]
pub(crate) struct Generator {
    eff_curve: ArrayCurve,
    action: Vec<f64>,
    max_discharge: f64,
}

impl Generator {
    fn from_config(config: &GeneratorConfig, stps: usize) -> Result<Self> {
        Ok(Self {
            eff_curve: ArrayCurve::from_points(&config.turbine_curve)?,
            action: vec![0.0; stps],
            max_discharge: config.max_discharge,
        })
    }
}

#[derive(Debug, Clone)]
pub(crate) struct Powerstation {
    init_power: f64,
    static_gen_efficiency: f64,
    headlosscoef: f64,
    powstat_masl: f64,
    powstat_startstop: f64,
    shared_penstock: bool,
    generators: Vec<Generator>,
    aggressive_actions_cost: f64,
}

impl Powerstation {
    fn from_config(config: &PowerstationConfig, stps: usize) -> Result<Self> {
        Ok(Self {
            init_power: -NOT_INIT,
            static_gen_efficiency: config.static_gen_efficiency,
            headlosscoef: config.headlosscoef,
            powstat_masl: config.powstat_masl,
            powstat_startstop: config.powstat_startstop,
            shared_penstock: config.shared_penstock,
            generators: config
                .generators
                .iter()
                .map(|generator| Generator::from_config(generator, stps))
                .collect::<Result<Vec<_>>>()?,
            aggressive_actions_cost: -NOT_INIT,
        })
    }

    fn validate_settings(&self, common: &NodeCommon) -> Result<()> {
        if self.generators.is_empty() || self.generators.len() > MAX_NR_GENERATORS {
            return Err(HerssError::new(format!(
                "Number of generators in powerstation {} is out of bounds",
                common.nodename
            )));
        }
        if self.headlosscoef < 0.0 || self.headlosscoef > VERY_LARGE_NUMBER {
            return Err(HerssError::new(format!(
                "Head loss coefficient in powerstation {} is out of bounds",
                common.nodename
            )));
        }
        if self.powstat_masl < -1000.0 || self.powstat_masl > MOUNT_EVEREST_MASL {
            return Err(HerssError::new(format!(
                "Powerstation elevation in {} is out of bounds",
                common.nodename
            )));
        }
        if common.local_energy_equivalent < 0.0
            || common.local_energy_equivalent > VERY_LARGE_NUMBER
        {
            return Err(HerssError::new(format!(
                "Local energy equivalent in {} is out of bounds",
                common.nodename
            )));
        }
        Ok(())
    }
}

#[derive(Debug, Clone)]
pub(crate) struct Channel {
    k_traveltime_hours: f64,
    num_cascaded_reservoirs: usize,
    #[allow(dead_code)]
    decay: f64,
    initial_storage_linres_mm3: Vec<f64>,
    casc_reservoirs: Option<CascadedReservoirs>,
}

impl Channel {
    fn from_config(config: &ChannelConfig) -> Self {
        Self {
            k_traveltime_hours: config.k_traveltime_hours,
            num_cascaded_reservoirs: config.num_cascaded_reservoirs,
            decay: config.decay,
            initial_storage_linres_mm3: Vec::new(),
            casc_reservoirs: None,
        }
    }

    fn validate_settings(&mut self) -> Result<()> {
        if self.initial_storage_linres_mm3.is_empty() {
            self.initial_storage_linres_mm3 = vec![0.0; self.num_cascaded_reservoirs];
        }
        let mut cascade =
            CascadedReservoirs::new(self.k_traveltime_hours, self.num_cascaded_reservoirs)?;
        cascade.set_initial_storage(&self.initial_storage_linres_mm3)?;
        self.casc_reservoirs = Some(cascade);
        Ok(())
    }

    fn set_start_state(&mut self, scenario: &mut Scenario) -> Result<()> {
        if self.casc_reservoirs.is_none() {
            self.validate_settings()?;
        }
        if self.initial_storage_linres_mm3.is_empty() {
            self.initial_storage_linres_mm3 = vec![0.0; self.num_cascaded_reservoirs];
        }
        self.casc_reservoirs
            .as_mut()
            .ok_or_else(|| HerssError::new("Channel routing not initialized"))?
            .set_initial_storage(&self.initial_storage_linres_mm3)?;
        for value in &mut scenario.up_inflow {
            *value = 0.0;
        }
        Ok(())
    }
}

#[derive(Debug, Clone)]
pub(crate) enum NodeData {
    Reservoir(Reservoir),
    Powerstation(Powerstation),
    Channel(Channel),
}

#[derive(Debug, Clone)]
pub(crate) struct Node {
    common: NodeCommon,
    scenario: Scenario,
    data: NodeData,
}

#[derive(Debug, Clone)]
pub struct Riversystem {
    pub(crate) nodes: Vec<Node>,
    pub start_water_mm3: f64,
    pub end_water_mm3: f64,
    pub inflow_volume_mm3: f64,
    pub outgoing_mm3: f64,
    pub waterbalance: f64,
    pub sum_prod_mwh: f64,
    pub sum_total_mwh: f64,
    pub adjust_cost: f64,
    pub tot_remaining_mm3: f64,
    pub tot_remaining_mwh: f64,
    pub tot_remaining_euro: f64,
    pub tot_active_remaining_mm3: f64,
    pub tot_income_euro: f64,
    pub tot_cost_euro: f64,
    pub tot_profit_euro: f64,
    pub valuefunction_euro: f64,
    pub sum_production: f64,
    pub avg_price: f64,
    pub sum_startstopcost: f64,
    pub sum_max_adjustment_cost: f64,
    pub sum_lrw_cost: f64,
    pub sum_qmin_cost: f64,
    systemname: String,
    outputdir: String,
}

pub struct Herss {
    pub gc: GlobalConfig,
    pub data: Option<Dataset>,
    pub riversystem: Riversystem,
}

impl Herss {
    pub fn new(gc: GlobalConfig) -> Self {
        let riversystem =
            Riversystem::new(&gc).expect("GlobalConfig must be diagnosed before Herss::new");
        Self {
            gc,
            data: None,
            riversystem,
        }
    }

    pub fn prepare_simulation(&mut self, data: Dataset) -> Result<()> {
        let max_action_cols = self.gc.nr_nodes.max(data.action_colnames.len());
        for node in &mut self.riversystem.nodes {
            node.scenario = Scenario::new(self.gc.stps, max_action_cols);
            for t in 0..self.gc.stps {
                node.scenario.inflow[t] = data.inflow[t][node.common.idnr];
                if node.common.idnr < data.action[t].len() {
                    node.scenario.action[t][node.common.idnr] = data.action[t][node.common.idnr];
                }
                node.scenario.price[t] = data.price[t];
                node.scenario.year[t] = data.year[t];
                node.scenario.month[t] = data.month[t];
                node.scenario.day[t] = data.day[t];
                node.scenario.hour[t] = data.hour[t];
            }
            node.scenario.restprice = data.restprice;
        }

        self.read_state_file()?;
        for node in &mut self.riversystem.nodes {
            match &mut node.data {
                NodeData::Reservoir(reservoir) => {
                    reservoir.init_reservoir(&mut node.scenario)?;
                }
                NodeData::Channel(channel) => {
                    channel.validate_settings()?;
                }
                NodeData::Powerstation(_) => {}
            }
        }
        self.map_actions(&data)?;
        self.data = Some(data);
        Ok(())
    }

    pub fn simulate(&mut self) -> Result<()> {
        for node in &mut self.riversystem.nodes {
            match &mut node.data {
                NodeData::Reservoir(reservoir) => reservoir.init_reservoir(&mut node.scenario)?,
                NodeData::Channel(channel) => channel.set_start_state(&mut node.scenario)?,
                NodeData::Powerstation(_) => {}
            }
        }

        for node in &mut self.riversystem.nodes {
            node.common.remaining_mm3 = 0.0;
            node.common.upstream_remaining_mm3 = 0.0;
            node.common.remaining_active_mm3 = 0.0;
            node.common.upstream_remaining_active_mm3 = 0.0;
        }

        for t in 0..self.gc.stps {
            let dt = f64::from(self.get_delta_t(t)?);
            for idx in 0..self.riversystem.nodes.len() {
                self.riversystem.nodes[idx].scenario.dt = dt;
                self.riversystem.simulate_node(idx, t, dt)?;
            }
        }

        for idx in 0..self.riversystem.nodes.len() {
            if let Some(downstream) = self.riversystem.nodes[idx].common.downstream_idnr {
                let remaining = self.riversystem.nodes[idx].common.remaining_mm3
                    + self.riversystem.nodes[idx].common.upstream_remaining_mm3;
                let active = self.riversystem.nodes[idx].common.remaining_active_mm3
                    + self.riversystem.nodes[idx]
                        .common
                        .upstream_remaining_active_mm3;
                self.riversystem.nodes[downstream]
                    .common
                    .upstream_remaining_mm3 += remaining;
                self.riversystem.nodes[downstream]
                    .common
                    .upstream_remaining_active_mm3 += active;
            }
        }

        Ok(())
    }

    pub fn check_water_balance(&self) -> Result<()> {
        for node in &self.riversystem.nodes {
            match &node.data {
                NodeData::Reservoir(reservoir) => {
                    let start_res_mm3 = reservoir.filling_at_lrw_mm3
                        + reservoir.reservoir_init_fr
                            * (reservoir.filling_at_hrw_mm3 - reservoir.filling_at_lrw_mm3);
                    let mut sum_inflow = 0.0;
                    let mut sum_outflow = 0.0;
                    for t in 0..self.gc.stps {
                        let dt = f64::from(self.get_delta_t(t)?);
                        sum_inflow +=
                            m3s_to_mm3(node.scenario.inflow[t] + node.scenario.up_inflow[t], dt);
                        sum_outflow += m3s_to_mm3(node.scenario.tot_outflow[t], dt);
                    }
                    let waterbalance = start_res_mm3 + sum_inflow - reservoir.res_mm3 - sum_outflow;
                    if waterbalance.abs() > 0.0001 {
                        return Err(HerssError::new(format!(
                            "WATERBALANCE RESERVOIR ERROR for {}: {waterbalance}",
                            node.common.nodename
                        )));
                    }
                }
                NodeData::Powerstation(_) => {
                    let mut sum_inflow = 0.0;
                    let mut sum_outflow = 0.0;
                    for t in 0..self.gc.stps {
                        let dt = f64::from(self.get_delta_t(t)?);
                        sum_inflow +=
                            m3s_to_mm3(node.scenario.inflow[t] + node.scenario.up_inflow[t], dt);
                        sum_outflow += m3s_to_mm3(node.scenario.tot_outflow[t], dt);
                    }
                    if (sum_inflow - sum_outflow).abs() > 0.0001 {
                        return Err(HerssError::new(format!(
                            "WATERBALANCE POWERSTATION ERROR for {}",
                            node.common.nodename
                        )));
                    }
                }
                NodeData::Channel(channel) => {
                    let start_channel: f64 = channel.initial_storage_linres_mm3.iter().sum();
                    let end_channel = channel
                        .casc_reservoirs
                        .as_ref()
                        .map(|cascade| cascade.total_storage_m3() / 1_000_000.0)
                        .unwrap_or(0.0);
                    let mut sum_inflow = 0.0;
                    let mut sum_outflow = 0.0;
                    for t in 0..self.gc.stps {
                        let dt = f64::from(self.get_delta_t(t)?);
                        sum_inflow +=
                            m3s_to_mm3(node.scenario.inflow[t] + node.scenario.up_inflow[t], dt);
                        sum_outflow += m3s_to_mm3(node.scenario.tot_outflow[t], dt);
                    }
                    let waterbalance = start_channel + sum_inflow - end_channel - sum_outflow;
                    if waterbalance.abs() > 0.0001 {
                        return Err(HerssError::new(format!(
                            "WATERBALANCE CHANNEL ERROR for {}: {waterbalance}",
                            node.common.nodename
                        )));
                    }
                }
            }
        }
        Ok(())
    }

    pub fn global_water_balance(&mut self) -> Result<()> {
        self.riversystem.start_water_mm3 = self
            .riversystem
            .nodes
            .iter()
            .map(Node::start_water_mm3)
            .sum::<Result<f64>>()?;
        self.riversystem.end_water_mm3 = self
            .riversystem
            .nodes
            .iter()
            .map(Node::end_water_mm3)
            .sum::<Result<f64>>()?;
        self.riversystem.inflow_volume_mm3 = 0.0;
        for t in 0..self.gc.stps {
            let dt = f64::from(self.get_delta_t(t)?);
            for node in &self.riversystem.nodes {
                self.riversystem.inflow_volume_mm3 += m3s_to_mm3(node.scenario.inflow[t], dt);
            }
        }

        self.riversystem.outgoing_mm3 = 0.0;
        let outlet_idx = self.riversystem.nodes.len() - 1;
        for t in 0..self.gc.stps {
            let dt = f64::from(self.get_delta_t(t)?);
            self.riversystem.outgoing_mm3 += m3s_to_mm3(
                self.riversystem.nodes[outlet_idx].scenario.tot_outflow[t],
                dt,
            );
        }
        self.riversystem.waterbalance = self.riversystem.start_water_mm3
            + self.riversystem.inflow_volume_mm3
            - self.riversystem.end_water_mm3
            - self.riversystem.outgoing_mm3;
        if self.riversystem.waterbalance.abs() > 0.0001 {
            return Err(HerssError::new(format!(
                "GLOBAL WATERBALANCE ERROR: {}",
                self.riversystem.waterbalance
            )));
        }
        Ok(())
    }

    pub fn calc_adjustment_costs(&mut self) -> Result<()> {
        for node in &mut self.riversystem.nodes {
            if node.common.nodetype == NodeType::Pstation && node.common.max_adjustment_pr_day > 0 {
                if let NodeData::Powerstation(powerstation) = &mut node.data {
                    self.riversystem.adjust_cost = powerstation.calc_adjustment_costs(
                        &mut node.scenario,
                        node.common.max_adjustment_pr_day,
                        node.common.max_adjustment_cost,
                    );
                }
            }
        }
        Ok(())
    }

    pub fn write_outputs(&mut self) -> Result<()> {
        std::fs::create_dir_all(&self.gc.outputdir)?;
        self.riversystem
            .write_river_system_data(self.data.as_ref().unwrap().restprice)?;
        self.riversystem.write_reservoir_data()?;
        self.write_state_file()?;
        if self.gc.write_nodefiles {
            self.riversystem.write_node_output()?;
        }
        Ok(())
    }

    fn get_delta_t(&self, timestep: usize) -> Result<i32> {
        self.data
            .as_ref()
            .ok_or_else(|| HerssError::new("Dataset not available"))?
            .get_delta_t(timestep)
    }

    fn read_state_file(&mut self) -> Result<()> {
        let lines = active_lines(&self.gc.start_statefile)?;
        for idx in 0..self.riversystem.nodes.len() {
            let node_id = self.riversystem.nodes[idx].common.idnr;
            let nodename = self.riversystem.nodes[idx].common.nodename.clone();
            match &mut self.riversystem.nodes[idx].data {
                NodeData::Reservoir(reservoir) => {
                    let line = find_state_line(&lines, "RESERVOIR", node_id, &nodename)?;
                    let cols = tokens(line);
                    if cols.len() != 5 {
                        return Err(HerssError::new(format!(
                            "Expected 5 columns for NODE RESERVOIR in {}",
                            self.gc.start_statefile
                        )));
                    }
                    reservoir.reservoir_init_fr = cols[4].parse()?;
                }
                NodeData::Powerstation(powerstation) => {
                    let line = find_state_line(&lines, "PSTATION", node_id, &nodename)?;
                    let cols = tokens(line);
                    if cols.len() != 5 {
                        return Err(HerssError::new(format!(
                            "Expected 5 columns for NODE PSTATION in {}",
                            self.gc.start_statefile
                        )));
                    }
                    powerstation.init_power = cols[4].parse()?;
                }
                NodeData::Channel(channel) => {
                    let line = find_state_line(&lines, "CHANNEL", node_id, &nodename)?;
                    let cols = tokens(line);
                    channel.initial_storage_linres_mm3.clear();
                    for segment in 0..channel.num_cascaded_reservoirs {
                        channel
                            .initial_storage_linres_mm3
                            .push(parse_f64_or_zero(cols.get(4 + segment).copied()));
                    }
                }
            }
        }
        Ok(())
    }

    fn map_actions(&mut self, data: &Dataset) -> Result<()> {
        for node in &mut self.riversystem.nodes {
            match &mut node.data {
                NodeData::Reservoir(reservoir) => {
                    if node.common.downstream_idnr_hatch.is_some() {
                        let colname = node.common.idnr.to_string();
                        let col_idx = data
                            .action_colnames
                            .iter()
                            .position(|name| name == &colname)
                            .ok_or_else(|| {
                                HerssError::new(format!(
                                    "Could not find action column for {}",
                                    node.common.idnr
                                ))
                            })?;
                        for t in 0..data.stps {
                            node.scenario.action[t][node.common.idnr] = data.action[t][col_idx];
                        }
                    }
                    let _ = reservoir;
                }
                NodeData::Powerstation(powerstation) => {
                    let ps_id = node.common.idnr;
                    for (g, generator) in powerstation.generators.iter_mut().enumerate() {
                        let colname = format!("{ps_id}_{g}");
                        let col_idx = data
                            .action_colnames
                            .iter()
                            .position(|name| name == &colname)
                            .ok_or_else(|| {
                                HerssError::new(format!(
                                    "Could not find action column for generator {colname}"
                                ))
                            })?;
                        generator.action.clear();
                        for t in 0..data.stps {
                            generator.action.push(data.action[t][col_idx]);
                        }
                    }
                }
                NodeData::Channel(_) => {}
            }
        }
        Ok(())
    }

    fn write_state_file(&self) -> Result<()> {
        let mut out = String::new();
        for node in &self.riversystem.nodes {
            out.push_str(&node.state_line()?);
        }
        std::fs::write(&self.gc.out_statefile, out)?;
        Ok(())
    }
}

fn find_state_line<'a>(
    lines: &'a [String],
    node_type: &str,
    node_id: usize,
    name: &str,
) -> Result<&'a str> {
    for line in lines {
        let cols = tokens(line);
        if cols.len() >= 4
            && cols[0] == "NODE"
            && cols[1] == node_type
            && cols[2].parse::<usize>().ok() == Some(node_id)
            && cols[3] == name
        {
            return Ok(line);
        }
    }
    Err(HerssError::new(format!(
        "Could not find NODE {node_type} {node_id} {name} in statefile"
    )))
}

impl Riversystem {
    pub(crate) fn new(gc: &GlobalConfig) -> Result<Self> {
        let topology = gc
            .topology
            .as_ref()
            .ok_or_else(|| HerssError::new("GlobalConfig has not been diagnosed"))?;
        let mut reservoir_idx = 0usize;
        let mut pstation_idx = 0i32;
        let mut nodes = Vec::with_capacity(topology.nodes.len());
        let max_action_cols = gc.nr_nodes.max(gc.n_action_nodes);

        for node_config in &topology.nodes {
            nodes.push(Node::from_config(
                node_config,
                gc.stps,
                max_action_cols,
                &mut reservoir_idx,
                &mut pstation_idx,
            )?);
        }

        Ok(Self {
            nodes,
            start_water_mm3: 0.0,
            end_water_mm3: 0.0,
            inflow_volume_mm3: 0.0,
            outgoing_mm3: 0.0,
            waterbalance: 0.0,
            sum_prod_mwh: 0.0,
            sum_total_mwh: 0.0,
            adjust_cost: 0.0,
            tot_remaining_mm3: 0.0,
            tot_remaining_mwh: 0.0,
            tot_remaining_euro: 0.0,
            tot_active_remaining_mm3: 0.0,
            tot_income_euro: 0.0,
            tot_cost_euro: 0.0,
            tot_profit_euro: 0.0,
            valuefunction_euro: 0.0,
            sum_production: 0.0,
            avg_price: 0.0,
            sum_startstopcost: 0.0,
            sum_max_adjustment_cost: 0.0,
            sum_lrw_cost: 0.0,
            sum_qmin_cost: 0.0,
            systemname: gc.systemname.clone(),
            outputdir: gc.outputdir.clone(),
        })
    }

    pub fn diagnose_configuration(&mut self) -> Result<()> {
        for node in &mut self.nodes {
            match &mut node.data {
                NodeData::Reservoir(reservoir) => {
                    reservoir.validate_settings(&node.common.nodename)?
                }
                NodeData::Powerstation(powerstation) => {
                    powerstation.validate_settings(&node.common)?
                }
                NodeData::Channel(channel) => channel.validate_settings()?,
            }
        }
        Ok(())
    }

    pub fn calc_simulation_profit(&self) -> f64 {
        let mut sim_profit = 0.0;
        for node in &self.nodes {
            for t in 0..node.scenario.stps {
                sim_profit += node.scenario.income[t] - node.scenario.cost[t];
            }
        }
        sim_profit
    }

    fn simulate_node(&mut self, idx: usize, t: usize, dt: f64) -> Result<()> {
        match self.nodes[idx].common.nodetype {
            NodeType::Reservoir => self.simulate_reservoir(idx, t, dt),
            NodeType::Pstation => self.simulate_powerstation(idx, t, dt),
            NodeType::Channel => self.simulate_channel(idx, t, dt),
        }
    }

    fn simulate_reservoir(&mut self, idx: usize, t: usize, dt: f64) -> Result<()> {
        let mut tunnel_transfer = None;
        let mut hatch_transfer = None;
        let mut overflow_transfer = None;
        let mut tunnel_end_update = None;

        {
            let node = &mut self.nodes[idx];
            let NodeData::Reservoir(reservoir) = &mut node.data else {
                unreachable!();
            };
            reservoir.cost_lrw = 0.0;
            let total_inflow_mm3 =
                m3s_to_mm3(node.scenario.inflow[t] + node.scenario.up_inflow[t], dt);
            reservoir.res_mm3 += m3s_to_mm3(node.scenario.inflow[t], dt);
            reservoir.res_mm3 += m3s_to_mm3(node.scenario.up_inflow[t], dt);
            reservoir.validate_level_mm3(reservoir.res_mm3)?;
            node.scenario.tot_inflow[t] = mm3_to_m3s(total_inflow_mm3, dt);
        }

        if let Some(downstream) = self.nodes[idx].common.downstream_idnr_tunnel {
            let (start_masl, up_res_mm3, hrw) = {
                let NodeData::Reservoir(reservoir) = &self.nodes[idx].data else {
                    unreachable!();
                };
                (reservoir.res_masl, reservoir.res_mm3, reservoir.hrw)
            };
            self.nodes[downstream].common.start_of_stp_masl = start_masl.min(hrw);
            self.nodes[downstream].common.up_res_mm3 = up_res_mm3;
            self.nodes[downstream].scenario.dt = dt;
            let flow_m3s = self.get_tunnel_flow(downstream, t, dt)?;
            self.nodes[downstream].scenario.up_inflow[t] = flow_m3s;
            tunnel_transfer = Some(flow_m3s);

            let node = &mut self.nodes[idx];
            let NodeData::Reservoir(reservoir) = &mut node.data else {
                unreachable!();
            };
            reservoir.res_mm3 -= m3s_to_mm3(flow_m3s, dt);
            reservoir.update_masl()?;
        }

        {
            let node = &mut self.nodes[idx];
            let NodeData::Reservoir(reservoir) = &mut node.data else {
                unreachable!();
            };
            if let Some(downstream) = node.common.downstream_idnr_hatch {
                let action = node.scenario.action[t][node.common.idnr];
                if !(-0.000001..=1.000001).contains(&action) {
                    return Err(HerssError::new(format!(
                        "Action for hatch in reservoir {} is out of bounds: {action}",
                        node.common.nodename
                    )));
                }

                let mut hatchflow_mm3 = 0.0;
                if reservoir.res_masl > reservoir.hatch_masl {
                    let hatchflow_m3s = reservoir.min_q_hatch
                        + action * (reservoir.max_q_hatch - reservoir.min_q_hatch);
                    hatchflow_mm3 = m3s_to_mm3(hatchflow_m3s, dt);
                    let current_filling = if reservoir.use_reservoir_geometry {
                        reservoir.calc_res_volume(reservoir.res_masl)?
                    } else {
                        reservoir
                            .ac_res_masl_to_mm3
                            .as_ref()
                            .ok_or_else(|| HerssError::new("Reservoir curve not initialized"))?
                            .x2y(reservoir.res_masl)
                    };
                    hatchflow_mm3 =
                        hatchflow_mm3.min(current_filling - reservoir.filling_at_hatchlevel);
                    if hatchflow_mm3 < 0.0 {
                        hatchflow_mm3 = 0.0;
                    }
                }
                reservoir.res_mm3 -= hatchflow_mm3;
                reservoir.update_masl()?;
                hatch_transfer = Some((downstream, mm3_to_m3s(hatchflow_mm3, dt)));
            }
        }

        if let Some((downstream, flow_m3s)) = hatch_transfer {
            self.nodes[downstream].scenario.up_inflow[t] += flow_m3s;
        }

        {
            let node = &mut self.nodes[idx];
            let NodeData::Reservoir(reservoir) = &mut node.data else {
                unreachable!();
            };

            reservoir.update_masl()?;
            let overflow_mm3 = reservoir.calc_overflow(dt);
            if let Some(downstream) = node.common.downstream_idnr_overflow {
                overflow_transfer = Some((downstream, mm3_to_m3s(overflow_mm3, dt)));
            }
            reservoir.res_mm3 -= overflow_mm3;
            reservoir.update_masl()?;

            let cost_lrw = if reservoir.res_masl < reservoir.lrw {
                (reservoir.res_penalty * dt / 3600.0) * (reservoir.lrw - reservoir.res_masl)
            } else {
                0.0
            };
            reservoir.cost_lrw = cost_lrw;

            if let Some(downstream) = node.common.downstream_idnr_tunnel {
                let end_masl = reservoir.res_masl.min(reservoir.hrw);
                tunnel_end_update = Some((downstream, end_masl));
            }

            let fract_filling = (reservoir.res_mm3 - reservoir.filling_at_lrw_mm3)
                / reservoir.active_max_volume_mm3;
            reservoir.res_fr = fract_filling;
            node.common.remaining_mm3 = reservoir.res_mm3;
            node.common.remaining_active_mm3 = (fract_filling * reservoir.active_max_volume_mm3)
                .clamp(0.0, reservoir.active_max_volume_mm3);

            node.scenario.res_mm3[t] = reservoir.res_mm3;
            node.scenario.res_active_mm3[t] = node.common.remaining_active_mm3;
            node.scenario.res_masl[t] = reservoir.res_masl;
            node.scenario.res_fr[t] = fract_filling;
            node.scenario.overflow_mm3[t] = overflow_mm3;
            node.scenario.cost[t] = cost_lrw;
            node.scenario.power[t] = 0.0;
            node.scenario.tunnelflow_m3s[t] = tunnel_transfer.unwrap_or(0.0);
            node.scenario.hatchflow_m3s[t] = hatch_transfer.map(|(_, flow)| flow).unwrap_or(0.0);
            node.scenario.overflow_m3s[t] = overflow_transfer.map(|(_, flow)| flow).unwrap_or(0.0);
            node.scenario.auto_qmin_m3s[t] = 0.0;
            node.scenario.tot_outflow[t] = node.scenario.tunnelflow_m3s[t]
                + node.scenario.hatchflow_m3s[t]
                + node.scenario.overflow_m3s[t]
                + node.scenario.auto_qmin_m3s[t];
            node.scenario.income[t] = 0.0;
            node.scenario.profit[t] = node.scenario.income[t] - node.scenario.cost[t];
        }

        if let Some((downstream, flow_m3s)) = overflow_transfer {
            self.nodes[downstream].scenario.up_inflow[t] += flow_m3s;
        }
        if let Some((downstream, end_masl)) = tunnel_end_update {
            self.nodes[downstream].common.end_of_stp_masl = end_masl;
        }

        Ok(())
    }

    fn get_tunnel_flow(&mut self, idx: usize, t: usize, dt: f64) -> Result<f64> {
        let node = &mut self.nodes[idx];
        let NodeData::Powerstation(powerstation) = &mut node.data else {
            return Err(HerssError::new(
                "Tunnel outlet must point to a powerstation",
            ));
        };
        powerstation.aggressive_actions_cost = 0.0;
        node.scenario.auto_qmin_m3s[t] = 0.0;

        let mut total_action = 0.0;
        for (g, generator) in powerstation.generators.iter().enumerate() {
            if generator.action[t] < -0.000001 {
                return Err(HerssError::new(format!(
                    "action is negative for generator {g} in {}",
                    node.common.nodename
                )));
            }
            total_action += generator.action[t];
        }

        let mut flow = 0.0;
        if total_action >= 0.0000001 {
            for generator in &powerstation.generators {
                flow += generator.action[t] * generator.max_discharge;
            }
        }

        if node.common.auto_qmin > 0.0 && flow < node.common.auto_qmin {
            flow = node.common.auto_qmin;
            node.scenario.auto_qmin_m3s[t] = flow;
        }

        let q_mm3 = m3s_to_mm3(flow, dt);
        if q_mm3 > node.common.up_res_mm3 {
            powerstation.aggressive_actions_cost = (q_mm3 - node.common.up_res_mm3) * 900_000_000.0;
            flow = 0.0;
        }
        Ok(flow)
    }

    fn simulate_powerstation(&mut self, idx: usize, t: usize, dt: f64) -> Result<()> {
        let downstream = self.nodes[idx].common.downstream_idnr;
        let total_q = {
            let node = &mut self.nodes[idx];
            let NodeData::Powerstation(powerstation) = &mut node.data else {
                unreachable!();
            };

            let mut q_gen = vec![0.0; powerstation.generators.len()];
            let mut total_q = 0.0;
            for (g, generator) in powerstation.generators.iter().enumerate() {
                let action = generator.action[t];
                if !(-0.000001..=1.000001).contains(&action) {
                    return Err(HerssError::new(format!(
                        "Action for generator {g} in powerstation {} is out of bounds: {action}",
                        node.common.nodename
                    )));
                }
                q_gen[g] = action * generator.max_discharge;
                total_q += q_gen[g];
            }

            if total_q > 0.001 && total_q < node.common.powstat_min_discharge {
                // C++ warns but keeps the soft constraint.
            }

            let hbrutto = ((node.common.start_of_stp_masl + node.common.end_of_stp_masl) / 2.0)
                - powerstation.powstat_masl;
            let mut last_hnetto = 0.0;
            let mut total_power = 0.0;
            let mut income = 0.0;

            if powerstation.shared_penstock {
                let headloss = powerstation.headlosscoef * total_q * total_q;
                let hnetto = hbrutto - headloss;
                last_hnetto = hnetto;
                for (g, generator) in powerstation.generators.iter().enumerate() {
                    let q = q_gen[g];
                    let turbine_efficiency = generator_efficiency(generator, q);
                    if turbine_efficiency < 0.0 {
                        return Err(HerssError::new(format!(
                            "Turbine efficiency failed in {}",
                            node.common.nodename
                        )));
                    }
                    let mut p = turbine_efficiency * 1000.0 * GRAVITY * hnetto * q / 1_000_000.0;
                    p *= powerstation.static_gen_efficiency;
                    let power = p * dt / 3600.0;
                    total_power += power;
                    income += power * node.scenario.price[t];
                }
            } else {
                for (g, generator) in powerstation.generators.iter().enumerate() {
                    let q = q_gen[g];
                    let headloss = powerstation.headlosscoef * q * q;
                    let hnetto = hbrutto - headloss;
                    last_hnetto = hnetto;
                    let turbine_efficiency = generator_efficiency(generator, q);
                    if turbine_efficiency < 0.0 {
                        return Err(HerssError::new(format!(
                            "Turbine efficiency failed in {}",
                            node.common.nodename
                        )));
                    }
                    let mut p = turbine_efficiency * 1000.0 * GRAVITY * hnetto * q / 1_000_000.0;
                    p *= powerstation.static_gen_efficiency;
                    let power = p * dt / 3600.0;
                    total_power += power;
                    income += power * node.scenario.price[t];
                }
            }

            let mut startstop_cost = 0.0;
            for generator in &powerstation.generators {
                let previous_action = if t > 0 { generator.action[t - 1] } else { 0.0 };
                let current_action = generator.action[t];
                if (previous_action > 0.01) != (current_action > 0.01) {
                    startstop_cost += powerstation.powstat_startstop / 2.0;
                }
            }

            let q_mm3 = m3s_to_mm3(total_q, dt);
            let est_eekv = if total_power > 0.0 {
                (total_power / 1000.0) / q_mm3
            } else {
                0.0
            };

            node.scenario.income[t] = income;
            node.scenario.cost[t] = startstop_cost + powerstation.aggressive_actions_cost;
            node.scenario.profit[t] =
                income - startstop_cost - powerstation.aggressive_actions_cost;
            node.scenario.hnetto[t] = last_hnetto;
            node.scenario.hbrutto[t] = hbrutto;
            node.scenario.power[t] = total_power;
            node.scenario.estimated_eekv[t] = est_eekv;
            node.scenario.start_stop_cost[t] = startstop_cost;
            node.scenario.adjust_cost[t] = powerstation.aggressive_actions_cost;
            node.scenario.tot_outflow[t] = total_q;
            node.scenario.inflow[t] = 0.0;
            node.common.remaining_mm3 = 0.0;
            node.common.remaining_active_mm3 = 0.0;

            total_q
        };

        if let Some(downstream) = downstream {
            self.nodes[downstream].scenario.up_inflow[t] += total_q;
        }
        Ok(())
    }

    fn simulate_channel(&mut self, idx: usize, t: usize, dt: f64) -> Result<()> {
        let downstream = self.nodes[idx].common.downstream_idnr;
        let q_out = {
            let node = &mut self.nodes[idx];
            let NodeData::Channel(channel) = &mut node.data else {
                unreachable!();
            };
            let cascade = channel
                .casc_reservoirs
                .as_mut()
                .ok_or_else(|| HerssError::new("Channel routing not initialized"))?;
            let initial_storage_m3 = cascade.total_storage_m3();
            let q_in = node.scenario.up_inflow[t];
            let q_out = cascade.route(q_in, dt);
            let final_storage_m3 = cascade.total_storage_m3();
            let water_balance_m3 = initial_storage_m3 + q_in * dt - q_out * dt - final_storage_m3;
            if water_balance_m3.abs() > 0.0001 {
                return Err(HerssError::new(format!(
                    "Channel water balance failed for {}",
                    node.common.nodename
                )));
            }

            node.scenario.tot_outflow[t] = q_out;
            node.scenario.channel_storage_mm3[t] = final_storage_m3 / 1_000_000.0;
            node.scenario.income[t] = 0.0;
            node.scenario.cost_qmin[t] = 0.0;
            if node.common.qmin_in_use {
                let (requirement, qcost) = node
                    .common
                    .qmin
                    .calc_requirement(node.scenario.month[t], node.scenario.day[t]);
                if q_out < requirement {
                    node.scenario.cost_qmin[t] = qcost * dt / 3600.0;
                }
            }
            node.scenario.cost[t] = node.scenario.cost_qmin[t];
            node.common.remaining_mm3 = node.scenario.channel_storage_mm3[t].max(0.0);
            node.common.remaining_active_mm3 = 0.0;
            node.scenario.power[t] = 0.0;
            node.scenario.profit[t] = node.scenario.income[t] - node.scenario.cost[t];
            node.scenario.inflow[t] = 0.0;
            q_out
        };

        if let Some(downstream) = downstream {
            self.nodes[downstream].scenario.up_inflow[t] += q_out;
        }
        Ok(())
    }

    fn calc_economic_totals(&mut self, restprice: f64) {
        self.tot_remaining_mm3 = 0.0;
        self.tot_remaining_mwh = 0.0;
        self.tot_remaining_euro = 0.0;
        self.tot_income_euro = 0.0;
        self.tot_cost_euro = 0.0;
        self.tot_profit_euro = 0.0;
        self.valuefunction_euro = 0.0;
        self.sum_production = 0.0;

        if let Some(last) = self.nodes.last() {
            self.tot_remaining_mm3 = last.common.upstream_remaining_mm3 + last.common.remaining_mm3;
        }

        for node in &self.nodes {
            if node.common.nodetype == NodeType::Pstation {
                self.tot_remaining_mwh += node.common.local_energy_equivalent
                    * node.common.upstream_remaining_active_mm3
                    * 1000.0;
                for t in 0..node.scenario.stps {
                    self.sum_production += node.scenario.power[t];
                }
            }
        }

        for node in &self.nodes {
            for t in 0..node.scenario.stps {
                self.tot_income_euro += node.scenario.income[t];
                self.tot_cost_euro += node.scenario.cost[t];
            }
        }

        self.tot_remaining_euro = self.tot_remaining_mwh * restprice;
        self.tot_profit_euro = self.tot_income_euro - self.tot_cost_euro;
        self.valuefunction_euro = self.tot_profit_euro + self.tot_remaining_euro;

        self.sum_qmin_cost = self
            .nodes
            .iter()
            .filter(|node| node.common.nodetype == NodeType::Channel)
            .flat_map(|node| node.scenario.cost.iter())
            .sum();
        self.sum_lrw_cost = self
            .nodes
            .iter()
            .filter(|node| node.common.nodetype == NodeType::Reservoir)
            .flat_map(|node| node.scenario.cost.iter())
            .sum();
        self.sum_startstopcost = 0.0;
        self.sum_max_adjustment_cost = 0.0;
        for node in &self.nodes {
            if node.common.nodetype == NodeType::Pstation {
                for t in 0..node.scenario.stps {
                    self.sum_startstopcost += node.scenario.cost[t] - node.scenario.adjust_cost[t];
                    self.sum_max_adjustment_cost += node.scenario.adjust_cost[t];
                }
            }
        }

        self.avg_price = if let Some(first) = self.nodes.first() {
            first.scenario.price.iter().sum::<f64>() / first.scenario.stps as f64
        } else {
            0.0
        };
    }

    fn write_river_system_data(&mut self, restprice: f64) -> Result<()> {
        self.calc_economic_totals(restprice);
        let filename = format!(
            "{}riversystem_{}_output.txt",
            self.outputdir, self.systemname
        );
        let mut out = String::new();
        writeln!(out, "Riversystem {}", self.systemname).unwrap();
        writeln!(
            out,
            "Node Idnr Nodename          Nodetype int Nodetypename Remaining_Mm3"
        )
        .unwrap();
        for node in &self.nodes {
            writeln!(
                out,
                "Node {:>2} {:<20} Nodetype {}  {:<20}  {:.4} ",
                node.common.idnr,
                node.common.nodename,
                node.common.nodetype.as_int(),
                node.common.nodetype.as_str(),
                node.end_water_mm3()?
            )
            .unwrap();
        }
        writeln!(out, "-------------------------------------------").unwrap();
        writeln!(out, "GLOBAL WATERBALANCE").unwrap();
        writeln!(out, "start_water_Mm3   = {:.6}", self.start_water_mm3).unwrap();
        writeln!(out, "inflow_volume_Mm3 = {:.6}", self.inflow_volume_mm3).unwrap();
        writeln!(out, "outflow_Mm3       = {:.6}", self.outgoing_mm3).unwrap();
        writeln!(out, "end_water_Mm3     = {:.6}", self.end_water_mm3).unwrap();
        writeln!(out, "waterbalance      = {:.6}", self.waterbalance).unwrap();
        writeln!(
            out,
            "Note that there might be dead water below LRW in the system"
        )
        .unwrap();
        writeln!(out, "-------------------------------------------").unwrap();
        writeln!(out, "Average_price_Euro           = {:.3}", self.avg_price).unwrap();
        writeln!(out, "RestPrice_Euro               = {:.3}", restprice).unwrap();
        writeln!(
            out,
            "tot_remaining_Mm3            = {:.3}",
            self.tot_remaining_mm3
        )
        .unwrap();
        writeln!(
            out,
            "tot_active_remaining_Mm3     = {:.3}",
            self.tot_active_remaining_mm3
        )
        .unwrap();
        writeln!(
            out,
            "tot_remaining_MWh            = {:.3}",
            self.tot_remaining_mwh
        )
        .unwrap();
        writeln!(
            out,
            "tot_remaining_Euro           = {:.3}",
            self.tot_remaining_euro
        )
        .unwrap();
        writeln!(
            out,
            "Sum_Production_MWh           = {:.3}",
            self.sum_production
        )
        .unwrap();
        writeln!(
            out,
            "tot_income_Euro              = {:.3}",
            self.tot_income_euro
        )
        .unwrap();
        let achieved = if self.sum_production > 1.0 {
            self.tot_income_euro / self.sum_production
        } else {
            restprice
        };
        writeln!(out, "Avg_achieved_price_E_MWh     = {:.3}", achieved).unwrap();
        writeln!(
            out,
            "sum_qmin_cost_Euro           = {:.3}",
            self.sum_qmin_cost
        )
        .unwrap();
        writeln!(
            out,
            "sum_lrw_cost_Euro            = {:.3}",
            self.sum_lrw_cost
        )
        .unwrap();
        writeln!(
            out,
            "sum_startstopcost_Euro       = {:.3}",
            self.sum_startstopcost
        )
        .unwrap();
        writeln!(
            out,
            "sum_max_adjustment_cost      = {:.3}",
            self.sum_max_adjustment_cost
        )
        .unwrap();
        writeln!(
            out,
            "tot_cost_Euro                = {:.3}",
            self.tot_cost_euro
        )
        .unwrap();
        writeln!(
            out,
            "tot_profit_Euro              = {:.3}",
            self.tot_profit_euro
        )
        .unwrap();
        writeln!(
            out,
            "valuefunction_Euro           = {:.3}",
            self.valuefunction_euro
        )
        .unwrap();
        std::fs::write(filename, out)?;
        Ok(())
    }

    fn write_reservoir_data(&self) -> Result<()> {
        let filename = format!("{}reservoirs_{}_out.txt", self.outputdir, self.systemname);
        let mut out = String::new();
        writeln!(
            out,
            "Riversystem {} reservoir fractions [fr] ",
            self.systemname
        )
        .unwrap();
        out.push_str("YYYY MM DD HH ");
        for node in &self.nodes {
            if node.common.nodetype == NodeType::Reservoir {
                write!(out, "{} ", node.common.nodename).unwrap();
            }
        }
        out.push('\n');
        if let Some(first) = self.nodes.first() {
            for t in 0..first.scenario.stps {
                write!(
                    out,
                    "{} {} {} {} ",
                    first.scenario.year[t],
                    first.scenario.month[t],
                    first.scenario.day[t],
                    first.scenario.hour[t]
                )
                .unwrap();
                for node in &self.nodes {
                    if node.common.nodetype == NodeType::Reservoir {
                        write!(out, "{:.4} ", node.scenario.res_fr[t]).unwrap();
                    }
                }
                out.push('\n');
            }
        }
        std::fs::write(filename, out)?;
        Ok(())
    }

    fn write_node_output(&self) -> Result<()> {
        for node in &self.nodes {
            let filename = format!(
                "{}node{}_{}.txt",
                self.outputdir, node.common.idnr, node.common.nodename
            );
            std::fs::write(filename, node.node_output()?)?;
        }
        Ok(())
    }
}

impl Node {
    fn from_config(
        config: &NodeConfig,
        stps: usize,
        max_action_cols: usize,
        reservoir_idx: &mut usize,
        pstation_idx: &mut i32,
    ) -> Result<Self> {
        let mut common = NodeCommon::new(config.node_type(), config.id, config.name.clone());
        let data = match &config.kind {
            NodeConfigKind::Reservoir(res_config) => {
                common.reservoir_idnr = *reservoir_idx;
                *reservoir_idx += 1;
                common.downstream_idnr_hatch =
                    res_config.outlet_hatch.as_ref().map(|h| h.downstream);
                common.downstream_idnr_tunnel = res_config.outlet_tunnel;
                common.downstream_idnr_overflow = res_config.outlet_overflow;
                common.downstream_idnr_auto_qmin = res_config.outlet_auto_qmin;
                if let Some(downstream) = res_config.outlet_overflow {
                    common.downstream_idnr = Some(downstream);
                }
                if let Some(downstream) = res_config.outlet_tunnel {
                    common.downstream_idnr = Some(downstream);
                }
                NodeData::Reservoir(Reservoir::from_config(res_config)?)
            }
            NodeConfigKind::Powerstation(ps_config) => {
                common.pstation_idnr = *pstation_idx;
                *pstation_idx += 1;
                common.downstream_idnr = ps_config.downlink;
                common.local_energy_equivalent = ps_config.local_energy_equivalent;
                common.powstat_min_discharge = ps_config.powstat_min_discharge;
                common.auto_qmin = ps_config.auto_qmin;
                common.max_adjustment_pr_day = ps_config.max_adjustment_pr_day;
                common.max_adjustment_cost = ps_config.max_adjustment_cost;
                NodeData::Powerstation(Powerstation::from_config(ps_config, stps)?)
            }
            NodeConfigKind::Channel(ch_config) => {
                common.downstream_idnr = ch_config.downlink;
                NodeData::Channel(Channel::from_config(ch_config))
            }
        };

        Ok(Self {
            common,
            scenario: Scenario::new(stps, max_action_cols),
            data,
        })
    }

    fn start_water_mm3(&self) -> Result<f64> {
        match &self.data {
            NodeData::Reservoir(reservoir) => Ok(reservoir.filling_at_lrw_mm3
                + reservoir.reservoir_init_fr
                    * (reservoir.filling_at_hrw_mm3 - reservoir.filling_at_lrw_mm3)),
            NodeData::Powerstation(_) => Ok(0.0),
            NodeData::Channel(channel) => Ok(channel.initial_storage_linres_mm3.iter().sum()),
        }
    }

    fn end_water_mm3(&self) -> Result<f64> {
        match &self.data {
            NodeData::Reservoir(reservoir) => Ok(reservoir.res_mm3),
            NodeData::Powerstation(_) => Ok(0.0),
            NodeData::Channel(channel) => Ok(channel
                .casc_reservoirs
                .as_ref()
                .map(|cascade| cascade.total_storage_m3() / 1_000_000.0)
                .unwrap_or(0.0)),
        }
    }

    fn state_line(&self) -> Result<String> {
        let last = self.scenario.stps - 1;
        let mut line = String::new();
        match &self.data {
            NodeData::Reservoir(_) => {
                writeln!(
                    line,
                    "NODE RESERVOIR {} {} {:.5}",
                    self.common.idnr, self.common.nodename, self.scenario.res_fr[last]
                )
                .unwrap();
            }
            NodeData::Powerstation(_) => {
                writeln!(
                    line,
                    "NODE PSTATION {} {} {:.5}",
                    self.common.idnr, self.common.nodename, self.scenario.power[last]
                )
                .unwrap();
            }
            NodeData::Channel(channel) => {
                write!(
                    line,
                    "NODE CHANNEL {} {} ",
                    self.common.idnr, self.common.nodename
                )
                .unwrap();
                let cascade = channel
                    .casc_reservoirs
                    .as_ref()
                    .ok_or_else(|| HerssError::new("Channel routing not initialized"))?;
                for segment in 0..channel.num_cascaded_reservoirs {
                    write!(line, "{:.5} ", cascade.storage_mm3(segment)?).unwrap();
                }
                line.push('\n');
            }
        }
        Ok(line)
    }

    fn node_output(&self) -> Result<String> {
        match &self.data {
            NodeData::Reservoir(reservoir) => self.reservoir_output(reservoir),
            NodeData::Powerstation(powerstation) => self.powerstation_output(powerstation),
            NodeData::Channel(channel) => self.channel_output(channel),
        }
    }

    fn reservoir_output(&self, reservoir: &Reservoir) -> Result<String> {
        let mut out = String::new();
        writeln!(
            out,
            "RESERVOIR node {} {}",
            self.common.idnr, self.common.nodename
        )
        .unwrap();
        writeln!(out, "reservoir_init_fr= {:.5}", reservoir.reservoir_init_fr).unwrap();
        writeln!(out, "yyyy mm dd hh [m3/s] [Euro/MWh] [fr] [m3/s] [Mm3] [masl] [fr] [Euro]         [m3/s]     [m3/s]    [m3/s]   [m3/s]    [m3/s] ").unwrap();
        writeln!(out, "yyyy mm dd hh Inflow Price Action Up_Inflow Res_Mm3 Res_masl Res_fr lrw_cost tunnelflow hatchflow overflow auto_qmin tot_outflow").unwrap();
        for t in 0..self.scenario.stps {
            writeln!(
                out,
                "{} {} {} {} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} ",
                self.scenario.year[t],
                self.scenario.month[t],
                self.scenario.day[t],
                self.scenario.hour[t],
                self.scenario.inflow[t],
                self.scenario.price[t],
                self.scenario.action[t][self.common.idnr],
                self.scenario.up_inflow[t],
                self.scenario.res_mm3[t],
                self.scenario.res_masl[t],
                self.scenario.res_fr[t],
                self.scenario.cost[t],
                self.scenario.tunnelflow_m3s[t],
                self.scenario.hatchflow_m3s[t],
                self.scenario.overflow_m3s[t],
                self.scenario.auto_qmin_m3s[t],
                self.scenario.tot_outflow[t]
            )
            .unwrap();
        }
        Ok(out)
    }

    fn powerstation_output(&self, powerstation: &Powerstation) -> Result<String> {
        let mut out = String::new();
        writeln!(
            out,
            "POWERSTATION node {} {}",
            self.common.idnr, self.common.nodename
        )
        .unwrap();
        writeln!(out, "init_Power = {:.5}", powerstation.init_power).unwrap();
        writeln!(
            out,
            "penstock_config = {}",
            if powerstation.shared_penstock {
                "SHARED"
            } else {
                "SEPARATE"
            }
        )
        .unwrap();
        writeln!(out, "headloss_coef = {:.6}", powerstation.headlosscoef).unwrap();
        write!(out, "yyyy mm dd hh [m3/s]    [Euro/MWh] ").unwrap();
        for g in 0..powerstation.generators.len() {
            write!(out, " [fr_g{}]", g).unwrap();
        }
        writeln!(
            out,
            " [m3/s] [m3/s] [Euro] [Euro] [m] [m] [MWh] [Euro] [Euro] [GWh/Mm3]"
        )
        .unwrap();
        write!(out, "yyyy mm dd hh Up_Inflow Price").unwrap();
        for g in 0..powerstation.generators.len() {
            write!(out, " Action_g{}", g).unwrap();
        }
        writeln!(
            out,
            " tot_outflow auto_qmin income startstopCost Hnetto Hbrutto Power adjust_cost profit est_eekv"
        )
        .unwrap();
        for t in 0..self.scenario.stps {
            write!(
                out,
                "{} {} {} {} {:.4} {:.4}",
                self.scenario.year[t],
                self.scenario.month[t],
                self.scenario.day[t],
                self.scenario.hour[t],
                self.scenario.up_inflow[t],
                self.scenario.price[t]
            )
            .unwrap();
            for generator in &powerstation.generators {
                write!(out, " {:.4}", generator.action[t]).unwrap();
            }
            writeln!(
                out,
                " {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4} {:.4}",
                self.scenario.tot_outflow[t],
                self.scenario.auto_qmin_m3s[t],
                self.scenario.income[t],
                self.scenario.cost[t] - self.scenario.adjust_cost[t],
                self.scenario.hnetto[t],
                self.scenario.hbrutto[t],
                self.scenario.power[t],
                self.scenario.adjust_cost[t],
                self.scenario.profit[t],
                self.scenario.estimated_eekv[t]
            )
            .unwrap();
        }
        Ok(out)
    }

    fn channel_output(&self, channel: &Channel) -> Result<String> {
        let mut out = String::new();
        writeln!(
            out,
            "CHANNEL node {} {}",
            self.common.idnr, self.common.nodename
        )
        .unwrap();
        writeln!(out, "N_CASCADE_LINRES {}", channel.num_cascaded_reservoirs).unwrap();
        writeln!(out, "K_TRAVELTIME_HOURS {:.2}", channel.k_traveltime_hours).unwrap();
        writeln!(
            out,
            "yyyy mm dd hh [m3/s]    [Mm3]       [m3/s]      [Euro]"
        )
        .unwrap();
        writeln!(
            out,
            "yyyy mm dd hh Up_Inflow Storage_Mm3 tot_outflow Qmin_Cost"
        )
        .unwrap();
        for t in 0..self.scenario.stps {
            writeln!(
                out,
                "{} {} {} {} {:.4} {:.8} {:.4} {:.4} ",
                self.scenario.year[t],
                self.scenario.month[t],
                self.scenario.day[t],
                self.scenario.hour[t],
                self.scenario.up_inflow[t],
                self.scenario.channel_storage_mm3[t],
                self.scenario.tot_outflow[t],
                self.scenario.cost[t]
            )
            .unwrap();
        }
        Ok(out)
    }
}

fn generator_efficiency(generator: &Generator, q: f64) -> f64 {
    if (q - generator.eff_curve.xmax).abs() < 1e-12 {
        0.0
    } else {
        generator.eff_curve.x2y(q) / 100.0
    }
}

impl Powerstation {
    fn calc_adjustment_costs(
        &self,
        scenario: &mut Scenario,
        max_adjustment_pr_day: i32,
        max_adjustment_cost: f64,
    ) -> f64 {
        let mut prev_power = self.init_power;
        let mut nr_changes_pr_day = 0;
        let mut sum_cost = 0.0;
        for t in 0..scenario.stps {
            let diff = (prev_power - scenario.power[t]).abs();
            if diff > 0.1 {
                nr_changes_pr_day += 1;
            }
            if t > 2 && (t + 1) % 24 == 0 {
                if nr_changes_pr_day > max_adjustment_pr_day {
                    sum_cost += max_adjustment_cost;
                    scenario.adjust_cost[t] = max_adjustment_cost;
                    scenario.cost[t] += max_adjustment_cost;
                    scenario.profit[t] -= max_adjustment_cost;
                }
                nr_changes_pr_day = 0;
            }
            prev_power = scenario.power[t];
        }
        sum_cost
    }
}
