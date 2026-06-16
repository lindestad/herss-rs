use crate::error::{HerssError, Result};
use crate::util::{active_lines, count_cols, parse_bool, parse_optional_i32, raw_lines, tokens};
use crate::{
    MAX_NR_GENERATORS, MAX_NR_NODES, MAX_NR_POINTS_CURVE, NOT_INIT_USIZE, VERY_LARGE_NUMBER,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NodeType {
    Reservoir,
    Pstation,
    Channel,
}

impl NodeType {
    pub fn as_int(self) -> i32 {
        match self {
            Self::Reservoir => 0,
            Self::Pstation => 1,
            Self::Channel => 2,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Reservoir => "RESERVOIR",
            Self::Pstation => "PSTATION",
            Self::Channel => "CHANNEL",
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct Topology {
    pub nodes: Vec<NodeConfig>,
    pub n_action_nodes_from_topology: usize,
}

#[derive(Debug, Clone)]
pub(crate) struct NodeConfig {
    pub id: usize,
    pub name: String,
    pub kind: NodeConfigKind,
}

impl NodeConfig {
    pub fn node_type(&self) -> NodeType {
        match &self.kind {
            NodeConfigKind::Reservoir(_) => NodeType::Reservoir,
            NodeConfigKind::Powerstation(_) => NodeType::Pstation,
            NodeConfigKind::Channel(_) => NodeType::Channel,
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) enum NodeConfigKind {
    Reservoir(ReservoirConfig),
    Powerstation(PowerstationConfig),
    Channel(ChannelConfig),
}

#[derive(Debug, Clone)]
pub(crate) struct HatchConfig {
    pub downstream: usize,
    pub min_q: f64,
    pub max_q: f64,
    pub hatch_masl: f64,
}

#[derive(Debug, Clone)]
pub(crate) struct ReservoirConfig {
    pub hrw: f64,
    pub lrw: f64,
    pub res_penalty: f64,
    pub floodlevel_penalty: f64,
    pub fast_overflow: bool,
    pub use_reservoir_geometry: bool,
    pub width_m: f64,
    pub length_m: f64,
    pub theta: f64,
    pub bottom_masl: f64,
    pub use_reservoir_curve: bool,
    pub reservoir_curve: Vec<(f64, f64)>,
    pub overflow_curve: Vec<(f64, f64)>,
    pub outlet_hatch: Option<HatchConfig>,
    pub outlet_tunnel: Option<usize>,
    pub outlet_overflow: Option<usize>,
    pub outlet_auto_qmin: Option<usize>,
}

impl Default for ReservoirConfig {
    fn default() -> Self {
        Self {
            hrw: VERY_LARGE_NUMBER,
            lrw: VERY_LARGE_NUMBER,
            res_penalty: -VERY_LARGE_NUMBER,
            floodlevel_penalty: -VERY_LARGE_NUMBER,
            fast_overflow: false,
            use_reservoir_geometry: false,
            width_m: -VERY_LARGE_NUMBER,
            length_m: -VERY_LARGE_NUMBER,
            theta: -VERY_LARGE_NUMBER,
            bottom_masl: -VERY_LARGE_NUMBER,
            use_reservoir_curve: false,
            reservoir_curve: Vec::new(),
            overflow_curve: Vec::new(),
            outlet_hatch: None,
            outlet_tunnel: None,
            outlet_overflow: None,
            outlet_auto_qmin: None,
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct GeneratorConfig {
    pub turbine_curve: Vec<(f64, f64)>,
    pub max_discharge: f64,
}

#[derive(Debug, Clone)]
pub(crate) struct PowerstationConfig {
    pub downlink: Option<usize>,
    pub static_gen_efficiency: f64,
    pub headlosscoef: f64,
    pub powstat_masl: f64,
    pub powstat_min_discharge: f64,
    pub powstat_startstop: f64,
    pub shared_penstock: bool,
    pub local_energy_equivalent: f64,
    pub auto_qmin: f64,
    pub max_adjustment_pr_day: i32,
    pub max_adjustment_cost: f64,
    pub generators: Vec<GeneratorConfig>,
}

impl Default for PowerstationConfig {
    fn default() -> Self {
        Self {
            downlink: None,
            static_gen_efficiency: -VERY_LARGE_NUMBER,
            headlosscoef: -VERY_LARGE_NUMBER,
            powstat_masl: -VERY_LARGE_NUMBER,
            powstat_min_discharge: -VERY_LARGE_NUMBER,
            powstat_startstop: -VERY_LARGE_NUMBER,
            shared_penstock: false,
            local_energy_equivalent: -VERY_LARGE_NUMBER,
            auto_qmin: -VERY_LARGE_NUMBER,
            max_adjustment_pr_day: crate::NOT_INIT_I32,
            max_adjustment_cost: crate::NOT_INIT,
            generators: Vec::new(),
        }
    }
}

#[derive(Debug, Clone)]
pub(crate) struct ChannelConfig {
    pub downlink: Option<usize>,
    pub k_traveltime_hours: f64,
    pub num_cascaded_reservoirs: usize,
    pub decay: f64,
}

impl Default for ChannelConfig {
    fn default() -> Self {
        Self {
            downlink: None,
            k_traveltime_hours: crate::NOT_INIT,
            num_cascaded_reservoirs: 1,
            decay: crate::NOT_INIT,
        }
    }
}

#[derive(Debug, Clone)]
pub struct GlobalConfig {
    pub globalfile: String,
    pub topologyfile: String,
    pub actionsfile: String,
    pub pricefile: String,
    pub outputfile: String,
    pub inflowfile: String,
    pub systemname: String,
    pub start_statefile: String,
    pub out_statefile: String,
    pub outputdir: String,
    pub inputdir: String,
    pub write_nodefiles: bool,
    pub printglobalinfo: bool,
    pub printeconomicinfo: bool,
    pub nr_nodes: usize,
    pub nr_pstations: usize,
    pub nr_reservoirs: usize,
    pub nr_channels: usize,
    pub stps: usize,
    pub dt: usize,
    pub dt_last: usize,
    pub n_action_nodes: usize,
    pub n_action_nodes_from_topology: usize,
    pub n_inflow_nodes: usize,
    pub nodetypes: Vec<NodeType>,
    pub(crate) topology: Option<Topology>,
}

impl GlobalConfig {
    pub fn from_file(globalfile: &str) -> Result<Self> {
        let mut gc = Self::default();
        gc.globalfile = globalfile.to_owned();
        gc.read_global_file()?;
        Ok(gc)
    }

    fn read_global_file(&mut self) -> Result<()> {
        for line in raw_lines(&self.globalfile)? {
            let trimmed = line.trim();
            if trimmed.is_empty()
                || trimmed.starts_with('#')
                || !trimmed.chars().any(char::is_alphanumeric)
            {
                continue;
            }

            let cols = tokens(trimmed);
            if cols.len() < 2 {
                continue;
            }
            let keyword = cols[0];
            let value = cols[1];
            match keyword {
                "ACTIONFILE" => self.actionsfile = value.to_owned(),
                "INFLOWFILE" => self.inflowfile = value.to_owned(),
                "PRICEFILE" => self.pricefile = value.to_owned(),
                "TOPOLOGYFILE" => self.topologyfile = value.to_owned(),
                "PRINT_GLOBAL_INFO" => self.printglobalinfo = value == "TRUE",
                "PRINT_ECONOMIC_INFO" => self.printeconomicinfo = value == "TRUE",
                "SYSTEMNAME" => self.systemname = value.to_owned(),
                "STARTSTATEFILE" => self.start_statefile = value.to_owned(),
                "OUTSTATEFILE" => self.out_statefile = value.to_owned(),
                "OUTPUTFILE" => self.outputfile = value.to_owned(),
                "WRITE_NODEFILES" => self.write_nodefiles = value.parse::<i32>()? != 0,
                "OUTPUTDIR" => self.outputdir = value.to_owned(),
                "INPUTDIR" => self.inputdir = value.to_owned(),
                "DT" | "DT_LAST" => {}
                _ => {
                    return Err(HerssError::new(format!(
                        "Could not recognize global config keyword {keyword}"
                    )));
                }
            }
        }
        Ok(())
    }

    pub fn set_directories_and_filenames(&mut self) {
        self.topologyfile = format!("{}{}", self.inputdir, self.topologyfile);
        self.pricefile = format!("{}{}", self.inputdir, self.pricefile);
        self.inflowfile = format!("{}{}", self.inputdir, self.inflowfile);
        self.actionsfile = format!("{}{}", self.inputdir, self.actionsfile);
        self.start_statefile = format!("{}{}", self.inputdir, self.start_statefile);
        self.out_statefile = format!("{}{}", self.outputdir, self.out_statefile);
        self.outputfile = format!("{}{}", self.outputdir, self.outputfile);
    }

    pub fn diagnose(&mut self) -> Result<()> {
        let topology = parse_topology(&self.topologyfile)?;
        self.nr_nodes = topology.nodes.len();
        self.nr_reservoirs = topology
            .nodes
            .iter()
            .filter(|node| node.node_type() == NodeType::Reservoir)
            .count();
        self.nr_pstations = topology
            .nodes
            .iter()
            .filter(|node| node.node_type() == NodeType::Pstation)
            .count();
        self.nr_channels = topology
            .nodes
            .iter()
            .filter(|node| node.node_type() == NodeType::Channel)
            .count();
        self.n_action_nodes_from_topology = topology.n_action_nodes_from_topology;
        self.nodetypes = topology.nodes.iter().map(NodeConfig::node_type).collect();

        if self.nr_nodes == 0 {
            return Err(HerssError::new(format!(
                "Number of nodes zero or lower in topology {}",
                self.topologyfile
            )));
        }
        if self.nr_nodes > MAX_NR_NODES {
            return Err(HerssError::new(format!(
                "Number of nodes exceeds maximum {MAX_NR_NODES}"
            )));
        }

        self.read_action_header()?;
        self.read_inflow_header()?;
        self.topology = Some(topology);
        Ok(())
    }

    pub fn check_nr_steps(&mut self) -> Result<()> {
        let lines = raw_lines(&self.pricefile)?;
        if lines.len() < 2 {
            return Err(HerssError::new(format!(
                "There is an error in the pricefile {}",
                self.pricefile
            )));
        }
        self.stps = lines
            .iter()
            .skip(2)
            .filter(|line| {
                let trimmed = line.trim();
                !trimmed.is_empty() && !trimmed.starts_with('#')
            })
            .count();
        Ok(())
    }

    fn read_action_header(&mut self) -> Result<()> {
        let lines = active_lines(&self.actionsfile)?;
        let Some(header) = lines.first() else {
            return Err(HerssError::new(format!(
                "Actionsfile {} is empty",
                self.actionsfile
            )));
        };
        let cols = tokens(header);
        if cols.first().copied() != Some("Date_NodeID") {
            return Err(HerssError::new(format!(
                "There is an error in the actionsfile {}",
                self.actionsfile
            )));
        }
        self.n_action_nodes = cols.len().saturating_sub(1);
        if self.n_action_nodes != self.n_action_nodes_from_topology {
            return Err(HerssError::new(format!(
                "Number of action nodes in {} ({}) does not match topology ({})",
                self.actionsfile, self.n_action_nodes, self.n_action_nodes_from_topology
            )));
        }
        Ok(())
    }

    fn read_inflow_header(&mut self) -> Result<()> {
        let lines = active_lines(&self.inflowfile)?;
        let Some(header) = lines.first() else {
            return Err(HerssError::new(format!(
                "Inflowfile {} is empty",
                self.inflowfile
            )));
        };
        let cols = tokens(header);
        if cols.first().copied() != Some("Date_NodeID") {
            return Err(HerssError::new(format!(
                "There is an error in the inflowfile {}",
                self.inflowfile
            )));
        }
        self.n_inflow_nodes = cols.len().saturating_sub(1);
        if self.n_inflow_nodes != self.nr_reservoirs {
            return Err(HerssError::new(format!(
                "Number of inflow nodes in {} ({}) does not match reservoirs ({})",
                self.inflowfile, self.n_inflow_nodes, self.nr_reservoirs
            )));
        }
        Ok(())
    }
}

impl Default for GlobalConfig {
    fn default() -> Self {
        Self {
            globalfile: "ERROR_STR_NOT_INIT".to_owned(),
            topologyfile: "ERROR_STR_NOT_INIT".to_owned(),
            actionsfile: "ERROR_STR_NOT_INIT".to_owned(),
            pricefile: "ERROR_STR_NOT_INIT".to_owned(),
            outputfile: "ERROR_STR_NOT_INIT".to_owned(),
            inflowfile: "ERROR_STR_NOT_INIT".to_owned(),
            systemname: "ERROR_STR_NOT_INIT".to_owned(),
            start_statefile: "ERROR_STR_NOT_INIT".to_owned(),
            out_statefile: "ERROR_STR_NOT_INIT".to_owned(),
            outputdir: "ERROR_STR_NOT_INIT".to_owned(),
            inputdir: "ERROR_STR_NOT_INIT".to_owned(),
            write_nodefiles: false,
            printglobalinfo: false,
            printeconomicinfo: false,
            nr_nodes: NOT_INIT_USIZE,
            nr_pstations: NOT_INIT_USIZE,
            nr_reservoirs: NOT_INIT_USIZE,
            nr_channels: NOT_INIT_USIZE,
            stps: NOT_INIT_USIZE,
            dt: NOT_INIT_USIZE,
            dt_last: NOT_INIT_USIZE,
            n_action_nodes: NOT_INIT_USIZE,
            n_action_nodes_from_topology: 0,
            n_inflow_nodes: NOT_INIT_USIZE,
            nodetypes: Vec::new(),
            topology: None,
        }
    }
}

fn parse_topology(filename: &str) -> Result<Topology> {
    let lines = active_lines(filename)?;
    let mut nodes = Vec::new();
    let mut n_action_nodes_from_topology = 0usize;
    let mut idx = 0usize;

    while idx < lines.len() {
        let cols = tokens(&lines[idx]);
        if cols.first().copied() != Some("NODE") {
            idx += 1;
            continue;
        }
        if cols.len() < 4 {
            return Err(HerssError::new(format!(
                "Invalid NODE line in topology {filename}: {}",
                lines[idx]
            )));
        }

        let node_type = cols[1];
        let id: usize = cols[2].parse()?;
        let name = cols[3].to_owned();
        let (kind, next_idx, action_count) = match node_type {
            "RESERVOIR" => {
                let (config, next_idx, action_count) = parse_reservoir(filename, &lines, idx)?;
                (NodeConfigKind::Reservoir(config), next_idx, action_count)
            }
            "PSTATION" => {
                let (config, next_idx, action_count) = parse_powerstation(filename, &lines, idx)?;
                (NodeConfigKind::Powerstation(config), next_idx, action_count)
            }
            "CHANNEL" => {
                let (config, next_idx) = parse_channel(filename, &lines, idx)?;
                (NodeConfigKind::Channel(config), next_idx, 0)
            }
            _ => {
                return Err(HerssError::new(format!(
                    "Invalid nodetype {node_type} in topology {filename}"
                )));
            }
        };

        n_action_nodes_from_topology += action_count;
        nodes.push(NodeConfig { id, name, kind });
        idx = next_idx;
    }

    nodes.sort_by_key(|node| node.id);
    for (expected, node) in nodes.iter().enumerate() {
        if node.id != expected {
            return Err(HerssError::new(format!(
                "Node ids must be contiguous and ordered from zero; expected {expected}, got {}",
                node.id
            )));
        }
    }

    Ok(Topology {
        nodes,
        n_action_nodes_from_topology,
    })
}

fn parse_reservoir(
    filename: &str,
    lines: &[String],
    start: usize,
) -> Result<(ReservoirConfig, usize, usize)> {
    let mut config = ReservoirConfig::default();
    let mut action_count = 0usize;
    let mut idx = start + 1;

    while idx < lines.len() {
        let cols = tokens(&lines[idx]);
        if cols.is_empty() {
            idx += 1;
            continue;
        }
        match cols[0] {
            "NODE" => break,
            "ENDNODE" => {
                idx += 1;
                break;
            }
            "HRW" => config.hrw = req_col(&cols, 1, &lines[idx])?.parse()?,
            "LRW" => config.lrw = req_col(&cols, 1, &lines[idx])?.parse()?,
            "RES_PENALTY" => config.res_penalty = req_col(&cols, 1, &lines[idx])?.parse()?,
            "FLOODLEVEL_PENALTY" => {
                config.floodlevel_penalty = req_col(&cols, 1, &lines[idx])?.parse()?
            }
            "FAST_OVERFLOW" => {
                config.fast_overflow = parse_bool(req_col(&cols, 1, &lines[idx])?, "FAST_OVERFLOW")?
            }
            "OUTLET_HATCH" => {
                if let Some(downstream) = parse_optional_i32(req_col(&cols, 1, &lines[idx])?)? {
                    if count_cols(&lines[idx]) != 5 {
                        return Err(HerssError::new(format!(
                            "OUTLET_HATCH line should have 5 columns in {filename}: {}",
                            lines[idx]
                        )));
                    }
                    config.outlet_hatch = Some(HatchConfig {
                        downstream,
                        min_q: req_col(&cols, 2, &lines[idx])?.parse()?,
                        max_q: req_col(&cols, 3, &lines[idx])?.parse()?,
                        hatch_masl: req_col(&cols, 4, &lines[idx])?.parse()?,
                    });
                    action_count += 1;
                }
            }
            "OUTLET_TUNNEL" => {
                config.outlet_tunnel = parse_optional_i32(req_col(&cols, 1, &lines[idx])?)?
            }
            "OUTLET_AUTO_QMIN" => {
                if parse_optional_i32(req_col(&cols, 1, &lines[idx])?)?.is_some() {
                    return Err(HerssError::new("OUTLET_AUTO_QMIN is work in progress"));
                }
            }
            "RESERVOIR_GEOMETRY" => config.use_reservoir_geometry = true,
            "WIDTH_M" => config.width_m = req_col(&cols, 1, &lines[idx])?.parse()?,
            "LENGTH_M" => config.length_m = req_col(&cols, 1, &lines[idx])?.parse()?,
            "THETA" => config.theta = req_col(&cols, 1, &lines[idx])?.parse()?,
            "BOTTOM_MASL" => {
                config.bottom_masl = req_col(&cols, 1, &lines[idx])?.parse()?;
                config.use_reservoir_geometry = true;
            }
            "RESERVOIR_CURVE" => {
                let n_points: usize = req_col(&cols, 1, &lines[idx])?.parse()?;
                if n_points > MAX_NR_POINTS_CURVE {
                    return Err(HerssError::new("nr_points_res_curve > MAX_NR_POINTS_CURVE"));
                }
                config.use_reservoir_curve = true;
                config.reservoir_curve = parse_point_rows(lines, idx + 1, n_points)?;
                idx += n_points + 1;
                continue;
            }
            "OVERFLOW_CURVE" => {
                let n_points: usize = req_col(&cols, 1, &lines[idx])?.parse()?;
                if n_points > MAX_NR_POINTS_CURVE {
                    return Err(HerssError::new(
                        "nr_points_ovefl_curve > MAX_NR_POINTS_CURVE",
                    ));
                }
                config.outlet_overflow = Some(req_col(&cols, 2, &lines[idx])?.parse()?);
                config.overflow_curve = parse_point_rows(lines, idx + 1, n_points)?;
                idx += n_points + 1;
                continue;
            }
            _ => {}
        }
        idx += 1;
    }

    if config.outlet_overflow.is_none() {
        return Err(HerssError::new(format!(
            "Reservoir in {filename} is missing OVERFLOW_CURVE"
        )));
    }
    if config.use_reservoir_geometry && config.use_reservoir_curve {
        return Err(HerssError::new(
            "Cannot use both reservoir geometry and reservoir curve in the same reservoir",
        ));
    }
    Ok((config, idx, action_count))
}

fn parse_powerstation(
    filename: &str,
    lines: &[String],
    start: usize,
) -> Result<(PowerstationConfig, usize, usize)> {
    let mut config = PowerstationConfig::default();
    let mut idx = start + 1;
    let mut action_count = 0usize;

    while idx < lines.len() {
        let cols = tokens(&lines[idx]);
        if cols.is_empty() {
            idx += 1;
            continue;
        }
        match cols[0] {
            "NODE" => break,
            "ENDNODE" => {
                idx += 1;
                break;
            }
            "DOWNLINK_IDNR" => {
                config.downlink = parse_optional_i32(req_col(&cols, 1, &lines[idx])?)?
            }
            "STATIC_GENERATOR_EFFICIENCY" => {
                config.static_gen_efficiency = req_col(&cols, 1, &lines[idx])?.parse()?
            }
            "HEADLOSSCOEF" => config.headlosscoef = req_col(&cols, 1, &lines[idx])?.parse()?,
            "SHARED_PENSTOCK" => {
                config.shared_penstock =
                    parse_bool(req_col(&cols, 1, &lines[idx])?, "SHARED_PENSTOCK")?
            }
            "POWSTAT_MASL" => config.powstat_masl = req_col(&cols, 1, &lines[idx])?.parse()?,
            "POWSTAT_MIN_DISCHARGE" => {
                config.powstat_min_discharge = req_col(&cols, 1, &lines[idx])?.parse()?
            }
            "POWSTAT_STARTSTOP" => {
                config.powstat_startstop = req_col(&cols, 1, &lines[idx])?.parse()?
            }
            "LOCAL_ENERGY_EQUIVALENT" => {
                config.local_energy_equivalent = req_col(&cols, 1, &lines[idx])?.parse()?
            }
            "AUTO_QMIN" => config.auto_qmin = req_col(&cols, 1, &lines[idx])?.parse()?,
            "MAX_ADJUST" => {
                config.max_adjustment_pr_day = req_col(&cols, 1, &lines[idx])?.parse()?;
                if config.max_adjustment_pr_day > -1 {
                    return Err(HerssError::new(format!(
                        "MAX_ADJUST is work in progress in topology {filename}"
                    )));
                }
            }
            "NR_GENERATORS" => {
                let nr_generators: usize = req_col(&cols, 1, &lines[idx])?.parse()?;
                if nr_generators > MAX_NR_GENERATORS {
                    return Err(HerssError::new(format!(
                        "Maximum {MAX_NR_GENERATORS} generators allowed per powerstation"
                    )));
                }
                action_count += nr_generators;
                idx += 1;
                for generator_id in 0..nr_generators {
                    let gen_cols = tokens(lines.get(idx).ok_or_else(|| {
                        HerssError::new("Reached end of topology while reading generator")
                    })?);
                    if gen_cols.first().copied() != Some("GENERATOR")
                        || req_col(&gen_cols, 1, &lines[idx])?.parse::<usize>()? != generator_id
                    {
                        return Err(HerssError::new(format!(
                            "Expected GENERATOR {generator_id}"
                        )));
                    }
                    idx += 1;

                    let curve_cols = tokens(lines.get(idx).ok_or_else(|| {
                        HerssError::new("Reached end of topology while reading turbine curve")
                    })?);
                    if curve_cols.first().copied() != Some("TURBINE_CURVE") {
                        return Err(HerssError::new(format!(
                            "Expected TURBINE_CURVE for generator {generator_id}"
                        )));
                    }
                    let n_points: usize = req_col(&curve_cols, 1, &lines[idx])?.parse()?;
                    idx += 1;
                    let turbine_curve = parse_point_rows(lines, idx, n_points)?;
                    idx += n_points;

                    let max_cols = tokens(lines.get(idx).ok_or_else(|| {
                        HerssError::new("Reached end of topology while reading generator discharge")
                    })?);
                    if max_cols.first().copied() != Some("GENERATOR_MAX_DISCHARGE") {
                        return Err(HerssError::new(format!(
                            "Expected GENERATOR_MAX_DISCHARGE for generator {generator_id}"
                        )));
                    }
                    let max_discharge = req_col(&max_cols, 1, &lines[idx])?.parse()?;
                    config.generators.push(GeneratorConfig {
                        turbine_curve,
                        max_discharge,
                    });
                    idx += 1;
                }
                continue;
            }
            _ => {}
        }
        idx += 1;
    }

    Ok((config, idx, action_count))
}

fn parse_channel(filename: &str, lines: &[String], start: usize) -> Result<(ChannelConfig, usize)> {
    let node_cols = tokens(&lines[start]);
    let mut config = ChannelConfig::default();
    if node_cols.len() >= 5 {
        config.downlink = parse_optional_i32(node_cols[4])?;
    }

    let mut idx = start + 1;
    while idx < lines.len() {
        let cols = tokens(&lines[idx]);
        if cols.is_empty() {
            idx += 1;
            continue;
        }
        match cols[0] {
            "NODE" => break,
            "ENDNODE" => {
                idx += 1;
                break;
            }
            "TRAVELTIME" => config.k_traveltime_hours = req_col(&cols, 1, &lines[idx])?.parse()?,
            "DECAY" => config.decay = req_col(&cols, 1, &lines[idx])?.parse()?,
            "K_TRAVELTIME_HOURS" => {
                config.k_traveltime_hours = req_col(&cols, 1, &lines[idx])?.parse()?
            }
            "N_CASCADE_LINRES" => {
                config.num_cascaded_reservoirs = req_col(&cols, 1, &lines[idx])?.parse()?
            }
            "QMIN" => {
                let nr_periods: i32 = req_col(&cols, 1, &lines[idx])?.parse()?;
                if nr_periods > 0 {
                    return Err(HerssError::new(format!(
                        "QMIN is work in progress in topology {filename}"
                    )));
                }
            }
            _ => {
                return Err(HerssError::new(format!(
                    "Invalid keyword in topology {filename}: {}",
                    lines[idx]
                )));
            }
        }
        idx += 1;
    }

    Ok((config, idx))
}

fn req_col<'a>(cols: &'a [&str], idx: usize, line: &str) -> Result<&'a str> {
    cols.get(idx)
        .copied()
        .ok_or_else(|| HerssError::new(format!("Missing column {idx} in line: {line}")))
}

fn parse_point_rows(lines: &[String], start: usize, n_points: usize) -> Result<Vec<(f64, f64)>> {
    let mut points = Vec::with_capacity(n_points);
    for idx in start..start + n_points {
        let line = lines
            .get(idx)
            .ok_or_else(|| HerssError::new("Reached end of topology while reading curve"))?;
        let cols = tokens(line);
        points.push((
            req_col(&cols, 0, line)?.parse()?,
            req_col(&cols, 1, line)?.parse()?,
        ));
    }
    Ok(points)
}
