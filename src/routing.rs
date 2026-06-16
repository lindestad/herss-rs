use crate::error::{HerssError, Result};
use crate::{MAX_NR_CASCADED_RESERVOIRS, MAX_TRAVELTIME_HOURS};

#[derive(Debug, Clone, Copy)]
struct ReservoirStepResult {
    storage_new_m3: f64,
    q_out_avg_m3s: f64,
}

#[derive(Debug, Clone)]
pub(crate) struct CascadedReservoirs {
    storage_m3: Vec<f64>,
    k_res_seconds: f64,
}

impl CascadedReservoirs {
    pub fn new(k_traveltime_hours: f64, num_reservoirs: usize) -> Result<Self> {
        if !(0.0..=MAX_TRAVELTIME_HOURS).contains(&k_traveltime_hours) {
            return Err(HerssError::new(format!(
                "Travel time must be between 0 and {MAX_TRAVELTIME_HOURS} hours"
            )));
        }
        if !(1..=MAX_NR_CASCADED_RESERVOIRS).contains(&num_reservoirs) {
            return Err(HerssError::new(format!(
                "Number of cascaded reservoirs must be between 1 and {MAX_NR_CASCADED_RESERVOIRS}"
            )));
        }

        Ok(Self {
            storage_m3: vec![0.0; num_reservoirs],
            k_res_seconds: k_traveltime_hours * 3600.0 / num_reservoirs as f64,
        })
    }

    pub fn set_initial_storage(&mut self, initial_storage_mm3: &[f64]) -> Result<()> {
        if initial_storage_mm3.len() != self.storage_m3.len() {
            return Err(HerssError::new(
                "Initial storage size does not match the number of reservoirs",
            ));
        }
        for (idx, value) in initial_storage_mm3.iter().copied().enumerate() {
            if value < 0.0 {
                return Err(HerssError::new("Initial storage cannot be negative"));
            }
            self.storage_m3[idx] = value * 1_000_000.0;
        }
        Ok(())
    }

    pub fn route(&mut self, q_in_m3s: f64, dt_seconds: f64) -> f64 {
        let mut q_for_next = q_in_m3s;
        for storage in &mut self.storage_m3 {
            let result = route_one_reservoir(*storage, q_for_next, self.k_res_seconds, dt_seconds);
            *storage = result.storage_new_m3;
            q_for_next = result.q_out_avg_m3s;
        }
        q_for_next
    }

    pub fn total_storage_m3(&self) -> f64 {
        self.storage_m3.iter().sum()
    }

    pub fn storage_mm3(&self, idx: usize) -> Result<f64> {
        self.storage_m3
            .get(idx)
            .map(|value| value / 1_000_000.0)
            .ok_or_else(|| HerssError::new("Index out of range in getStorageMm3"))
    }
}

fn route_one_reservoir(
    storage_old_m3: f64,
    q_in_m3s: f64,
    k_seconds: f64,
    dt_seconds: f64,
) -> ReservoirStepResult {
    if k_seconds <= 0.0 {
        let q_out_avg_m3s = (storage_old_m3 + q_in_m3s * dt_seconds) / dt_seconds;
        return ReservoirStepResult {
            storage_new_m3: 0.0,
            q_out_avg_m3s: q_out_avg_m3s.max(0.0),
        };
    }

    let factor = (-dt_seconds / k_seconds).exp();
    let storage_new_m3 = storage_old_m3 * factor + k_seconds * (1.0 - factor) * q_in_m3s;
    let volume_out_m3 = storage_old_m3 + q_in_m3s * dt_seconds - storage_new_m3;
    ReservoirStepResult {
        storage_new_m3,
        q_out_avg_m3s: (volume_out_m3 / dt_seconds).max(0.0),
    }
}

#[cfg(test)]
mod tests {
    use super::CascadedReservoirs;

    #[test]
    fn one_hour_routing_matches_formula() {
        let mut route = CascadedReservoirs::new(1.0, 1).unwrap();
        route.set_initial_storage(&[0.0]).unwrap();
        let q = route.route(2.0, 3600.0);
        let factor = (-1.0f64).exp();
        let expected_storage = 3600.0 * (1.0 - factor) * 2.0;
        let expected_q = ((2.0 * 3600.0) - expected_storage) / 3600.0;
        assert!((q - expected_q).abs() < 1e-12);
    }
}
