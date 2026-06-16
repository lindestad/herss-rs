use crate::error::{HerssError, Result};
use crate::{POINTS_IN_ARRAY, VERY_LARGE_NUMBER};

#[derive(Debug, Clone)]
pub struct ArrayCurve {
    pub xmin: f64,
    pub xmax: f64,
    pub ymin: f64,
    pub ymax: f64,
    pub x_points: Vec<f64>,
    pub y_points: Vec<f64>,
    pub xupper: Vec<f64>,
    pub xlower: Vec<f64>,
    pub yupper: Vec<f64>,
    pub ylower: Vec<f64>,
}

impl Default for ArrayCurve {
    fn default() -> Self {
        Self {
            xmin: 0.0,
            xmax: 0.0,
            ymin: 0.0,
            ymax: 0.0,
            x_points: Vec::new(),
            y_points: Vec::new(),
            xupper: vec![0.0; POINTS_IN_ARRAY],
            xlower: vec![0.0; POINTS_IN_ARRAY],
            yupper: vec![0.0; POINTS_IN_ARRAY],
            ylower: vec![0.0; POINTS_IN_ARRAY],
        }
    }
}

impl ArrayCurve {
    pub fn from_points(points: &[(f64, f64)]) -> Result<Self> {
        let mut curve = Self {
            x_points: points.iter().map(|point| point.0).collect(),
            y_points: points.iter().map(|point| point.1).collect(),
            ..Self::default()
        };
        curve.initialize_arrays()?;
        Ok(curve)
    }

    pub fn initialize_arrays(&mut self) -> Result<()> {
        if self.x_points.len() != self.y_points.len() || self.x_points.len() < 2 {
            return Err(HerssError::new(
                "ArrayCurve requires at least two matching x/y points",
            ));
        }

        self.xmin = self.x_points.iter().copied().fold(f64::INFINITY, f64::min);
        self.xmax = self
            .x_points
            .iter()
            .copied()
            .fold(f64::NEG_INFINITY, f64::max);
        self.ymin = self.y_points.iter().copied().fold(f64::INFINITY, f64::min);
        self.ymax = self
            .y_points
            .iter()
            .copied()
            .fold(f64::NEG_INFINITY, f64::max);

        if (self.xmax - self.xmin).abs() <= f64::EPSILON
            || (self.ymax - self.ymin).abs() <= f64::EPSILON
        {
            return Err(HerssError::new("ArrayCurve cannot normalize a flat axis"));
        }

        for idx in 0..self.x_points.len() {
            self.x_points[idx] = (self.x_points[idx] - self.xmin) / (self.xmax - self.xmin);
            self.y_points[idx] = (self.y_points[idx] - self.ymin) / (self.ymax - self.ymin);
        }

        self.xlower[0] = self.x_points[0];
        self.ylower[0] = self.y_points[0];
        self.xupper[0] = self.x_points[1];
        self.yupper[0] = self.y_points[1];

        let mut idx_points = 0usize;
        let dx =
            (self.x_points[self.x_points.len() - 1] - self.x_points[0]) / POINTS_IN_ARRAY as f64;
        for idx in 1..POINTS_IN_ARRAY {
            let x = self.x_points[0] + idx as f64 * dx;
            while idx_points + 2 < self.x_points.len() && x >= self.x_points[idx_points + 1] {
                idx_points += 1;
            }
            self.xlower[idx] = self.x_points[idx_points];
            self.ylower[idx] = self.y_points[idx_points];
            self.xupper[idx] = self.x_points[idx_points + 1];
            self.yupper[idx] = self.y_points[idx_points + 1];
        }

        Ok(())
    }

    pub fn x2y(&self, x: f64) -> f64 {
        let xt = (x - self.xmin) / (self.xmax - self.xmin);
        if !(0.0..=1.0).contains(&xt) {
            return -VERY_LARGE_NUMBER;
        }

        let first = self.x_points[0];
        let last = self.x_points[self.x_points.len() - 1];
        if xt < first || xt > last {
            return -VERY_LARGE_NUMBER;
        }

        let mut idx = (0.5 + (xt - first) / (last - first) * POINTS_IN_ARRAY as f64) as usize;
        if idx >= POINTS_IN_ARRAY {
            idx = POINTS_IN_ARRAY - 1;
        }

        let denom = self.xupper[idx] - self.xlower[idx];
        if denom.abs() <= f64::EPSILON {
            return self.ymin;
        }
        let slope = (self.yupper[idx] - self.ylower[idx]) / denom;
        let y = slope * (xt - self.xlower[idx]) + self.ylower[idx];
        y * (self.ymax - self.ymin) + self.ymin
    }
}

#[cfg(test)]
mod tests {
    use super::ArrayCurve;
    use crate::VERY_LARGE_NUMBER;

    #[test]
    fn interpolates_linear_points() {
        let curve = ArrayCurve::from_points(&[(0.0, 0.0), (5.0, 50.0), (10.0, 100.0)]).unwrap();
        assert!((curve.x2y(2.5) - 25.0).abs() < 10.0);
    }

    #[test]
    fn returns_sentinel_out_of_bounds() {
        let curve = ArrayCurve::from_points(&[(5.0, 50.0), (15.0, 150.0)]).unwrap();
        assert_eq!(curve.x2y(-1.0), -VERY_LARGE_NUMBER);
    }
}
