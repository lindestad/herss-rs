use crate::config::{GlobalConfig, NodeType};
use crate::error::{HerssError, Result};
use crate::util::{DateTime, active_lines, tokens};
use crate::{MAX_NR_NODES, NOT_INIT};

#[derive(Debug, Clone)]
pub struct Dataset {
    pub stps: usize,
    pub nr_nodes: usize,
    pub price: Vec<f64>,
    pub restprice: f64,
    pub inflow: Vec<Vec<f64>>,
    pub action: Vec<Vec<f64>>,
    pub year: Vec<i32>,
    pub month: Vec<i32>,
    pub day: Vec<i32>,
    pub hour: Vec<i32>,
    pub action_colnames: Vec<String>,
    pub delta_t: Vec<i32>,
}

impl Dataset {
    pub fn new(gc: &GlobalConfig) -> Self {
        Self {
            stps: gc.stps,
            nr_nodes: gc.nr_nodes,
            price: vec![NOT_INIT; gc.stps],
            restprice: NOT_INIT,
            inflow: vec![vec![0.0; gc.nr_nodes]; gc.stps],
            action: vec![vec![NOT_INIT; gc.nr_nodes]; gc.stps],
            year: vec![crate::NOT_INIT_I32; gc.stps],
            month: vec![crate::NOT_INIT_I32; gc.stps],
            day: vec![crate::NOT_INIT_I32; gc.stps],
            hour: vec![crate::NOT_INIT_I32; gc.stps],
            action_colnames: Vec::new(),
            delta_t: Vec::new(),
        }
    }

    pub fn read_all_data(&mut self, gc: &GlobalConfig) -> Result<()> {
        self.read_pricefile(gc)?;
        self.read_inflow_file(gc)?;
        self.read_actions_file(gc)?;
        self.multi_temporal_resolution(gc)?;
        Ok(())
    }

    pub fn get_delta_t(&self, timestep: usize) -> Result<i32> {
        self.delta_t
            .get(timestep)
            .copied()
            .ok_or_else(|| HerssError::new(format!("timestep out of bounds: {timestep}")))
    }

    fn read_pricefile(&mut self, gc: &GlobalConfig) -> Result<()> {
        let lines = active_lines(&gc.pricefile)?;
        if lines.len() < self.stps + 2 {
            return Err(HerssError::new(format!(
                "There is an error in the pricefile {}",
                gc.pricefile
            )));
        }

        let rest_cols = tokens(&lines[0]);
        if rest_cols.first().copied() != Some("RESTPRICE") {
            return Err(HerssError::new(format!(
                "There is an error in the pricefile {}",
                gc.pricefile
            )));
        }
        self.restprice = req_col(&rest_cols, 1, &lines[0])?.parse()?;

        let header_cols = tokens(&lines[1]);
        if header_cols.first().copied() != Some("Date") {
            return Err(HerssError::new(format!(
                "There is an error in the pricefile {}",
                gc.pricefile
            )));
        }

        for t in 0..self.stps {
            let cols = tokens(&lines[t + 2]);
            let date = DateTime::parse(req_col(&cols, 0, &lines[t + 2])?)?;
            self.year[t] = date.year;
            self.month[t] = date.month;
            self.day[t] = date.day;
            self.hour[t] = date.hour;
            self.price[t] = req_col(&cols, 1, &lines[t + 2])?.parse()?;
        }
        Ok(())
    }

    fn read_inflow_file(&mut self, gc: &GlobalConfig) -> Result<()> {
        let lines = active_lines(&gc.inflowfile)?;
        if lines.len() < self.stps + 1 {
            return Err(HerssError::new(format!(
                "There is something wrong with the inflowfile {}",
                gc.inflowfile
            )));
        }

        let header_cols = tokens(&lines[0]);
        if header_cols.first().copied() != Some("Date_NodeID") {
            return Err(HerssError::new(format!(
                "There is an error in the inflowfile {}",
                gc.inflowfile
            )));
        }
        let ids = header_cols
            .iter()
            .skip(1)
            .map(|value| value.parse::<usize>())
            .collect::<std::result::Result<Vec<_>, _>>()?;

        if ids.len() != gc.nr_reservoirs {
            return Err(HerssError::new(format!(
                "Number of columns in {} should be number of reservoirs + 1",
                gc.inflowfile
            )));
        }
        for id in &ids {
            if *id >= MAX_NR_NODES || gc.nodetypes.get(*id).copied() != Some(NodeType::Reservoir) {
                return Err(HerssError::new(format!(
                    "The idnr {id} in the inflowfile {} does not match a reservoir node",
                    gc.inflowfile
                )));
            }
        }

        for t in 0..self.stps {
            let line = &lines[t + 1];
            let cols = tokens(line);
            if cols.len() != ids.len() + 1 {
                return Err(HerssError::new(format!(
                    "There is something wrong with the inflowfile {}",
                    gc.inflowfile
                )));
            }
            let date = DateTime::parse(cols[0])?;
            self.expect_date_match(t, date, "inflow")?;
            for (col, id) in ids.iter().copied().enumerate() {
                self.inflow[t][id] = cols[col + 1].parse()?;
            }
        }
        Ok(())
    }

    fn read_actions_file(&mut self, gc: &GlobalConfig) -> Result<()> {
        let lines = active_lines(&gc.actionsfile)?;
        if lines.len() < self.stps + 1 {
            return Err(HerssError::new(format!(
                "There is an error in the actionsfile {}",
                gc.actionsfile
            )));
        }

        let header_cols = tokens(&lines[0]);
        if header_cols.first().copied() != Some("Date_NodeID") {
            return Err(HerssError::new(format!(
                "There is an error in the actionsfile {}",
                gc.actionsfile
            )));
        }
        self.action_colnames = header_cols
            .iter()
            .skip(1)
            .map(|value| (*value).to_owned())
            .collect();
        let active_nodes = self.action_colnames.len();
        if active_nodes != gc.n_action_nodes_from_topology {
            return Err(HerssError::new(format!(
                "Number of action nodes in {} does not match topology",
                gc.actionsfile
            )));
        }
        let width = self.nr_nodes.max(active_nodes);
        self.action = vec![vec![NOT_INIT; width]; self.stps];

        for t in 0..self.stps {
            let line = &lines[t + 1];
            let cols = tokens(line);
            if cols.len() != active_nodes + 1 {
                return Err(HerssError::new(format!(
                    "There is an error in the actionsfile {}",
                    gc.actionsfile
                )));
            }
            let date = DateTime::parse(cols[0])?;
            self.expect_date_match(t, date, "action")?;
            for c in 0..active_nodes {
                self.action[t][c] = cols[c + 1].parse()?;
            }
        }
        Ok(())
    }

    fn multi_temporal_resolution(&mut self, gc: &GlobalConfig) -> Result<()> {
        if self.stps == 0 {
            self.delta_t.clear();
            return Ok(());
        }
        if self.stps == 1 {
            self.delta_t = vec![gc.dt as i32];
            return Ok(());
        }

        let mut epochs = Vec::with_capacity(self.stps);
        for t in 0..self.stps {
            epochs.push(
                DateTime::new(self.year[t], self.month[t], self.day[t], self.hour[t])?
                    .epoch_seconds(),
            );
        }

        self.delta_t.clear();
        for t in 1..self.stps {
            self.delta_t.push((epochs[t] - epochs[t - 1]) as i32);
        }
        self.delta_t
            .push((epochs[self.stps - 1] - epochs[self.stps - 2]) as i32);
        Ok(())
    }

    fn expect_date_match(&self, t: usize, date: DateTime, file_kind: &str) -> Result<()> {
        if self.year[t] != date.year
            || self.month[t] != date.month
            || self.day[t] != date.day
            || self.hour[t] != date.hour
        {
            return Err(HerssError::new(format!(
                "Date mismatch in {file_kind} file at timestep {t}"
            )));
        }
        Ok(())
    }
}

fn req_col<'a>(cols: &'a [&str], idx: usize, line: &str) -> Result<&'a str> {
    cols.get(idx)
        .copied()
        .ok_or_else(|| HerssError::new(format!("Missing column {idx} in line: {line}")))
}
