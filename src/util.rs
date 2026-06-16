use crate::MAX_WORDS;
use crate::error::{HerssError, Result};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) struct DateTime {
    pub year: i32,
    pub month: i32,
    pub day: i32,
    pub hour: i32,
}

impl DateTime {
    pub fn parse(input: &str) -> Result<Self> {
        let value = input.trim();
        if value.contains('-') {
            let parts: Vec<&str> = value.split('-').collect();
            if parts.len() != 3 && parts.len() != 4 {
                return Err(HerssError::new(format!("Invalid date format: {value}")));
            }
            return Self::new(
                parts[0].parse()?,
                parts[1].parse()?,
                parts[2].parse()?,
                if parts.len() == 4 {
                    parts[3].parse()?
                } else {
                    0
                },
            );
        }

        match value.len() {
            8 => Self::new(
                value[0..4].parse()?,
                value[4..6].parse()?,
                value[6..8].parse()?,
                0,
            ),
            10 => Self::new(
                value[0..4].parse()?,
                value[4..6].parse()?,
                value[6..8].parse()?,
                value[8..10].parse()?,
            ),
            _ => Err(HerssError::new(format!("Invalid date format: {value}"))),
        }
    }

    pub fn new(year: i32, month: i32, day: i32, hour: i32) -> Result<Self> {
        if !(1..=12).contains(&month) || !(1..=31).contains(&day) || !(0..=23).contains(&hour) {
            return Err(HerssError::new(format!(
                "Invalid date: {year:04}-{month:02}-{day:02}-{hour:02}"
            )));
        }
        Ok(Self {
            year,
            month,
            day,
            hour,
        })
    }

    pub fn epoch_seconds(self) -> i64 {
        days_from_civil(self.year, self.month, self.day) * 86_400 + i64::from(self.hour) * 3_600
    }
}

// Howard Hinnant's civil-date conversion, returning days since 1970-01-01.
fn days_from_civil(year: i32, month: i32, day: i32) -> i64 {
    let y = i64::from(year) - if month <= 2 { 1 } else { 0 };
    let era = if y >= 0 { y } else { y - 399 } / 400;
    let yoe = y - era * 400;
    let m = i64::from(month);
    let doy = (153 * (m + if m > 2 { -3 } else { 9 }) + 2) / 5 + i64::from(day) - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    era * 146_097 + doe - 719_468
}

pub(crate) fn active_lines(filename: &str) -> Result<Vec<String>> {
    let contents = std::fs::read_to_string(filename)
        .map_err(|err| HerssError::new(format!("Could not open {filename}: {err}")))?;
    Ok(contents
        .lines()
        .map(str::trim)
        .filter(|line| !line.is_empty() && !line.starts_with('#'))
        .map(ToOwned::to_owned)
        .collect())
}

pub(crate) fn raw_lines(filename: &str) -> Result<Vec<String>> {
    let contents = std::fs::read_to_string(filename)
        .map_err(|err| HerssError::new(format!("Could not open {filename}: {err}")))?;
    Ok(contents.lines().map(ToOwned::to_owned).collect())
}

pub(crate) fn tokens(line: &str) -> Vec<&str> {
    line.split_whitespace().collect()
}

pub(crate) fn count_cols(line: &str) -> usize {
    tokens(line).len().min(MAX_WORDS)
}

pub(crate) fn parse_bool(value: &str, key: &str) -> Result<bool> {
    match value {
        "TRUE" => Ok(true),
        "FALSE" => Ok(false),
        _ => Err(HerssError::new(format!("{key} must be TRUE or FALSE"))),
    }
}

pub(crate) fn parse_optional_i32(value: &str) -> Result<Option<usize>> {
    let parsed: i32 = value.parse()?;
    if parsed >= 0 {
        Ok(Some(parsed as usize))
    } else {
        Ok(None)
    }
}

pub(crate) fn parse_f64_or_zero(value: Option<&str>) -> f64 {
    value.and_then(|v| v.parse::<f64>().ok()).unwrap_or(0.0)
}

#[cfg(test)]
mod tests {
    use super::DateTime;

    #[test]
    fn parses_supported_dates() {
        let a = DateTime::parse("2022090116").unwrap();
        let b = DateTime::parse("2022-09-01-16").unwrap();
        assert_eq!(a, b);
        assert_eq!(a.year, 2022);
        assert_eq!(a.month, 9);
        assert_eq!(a.day, 1);
        assert_eq!(a.hour, 16);
    }

    #[test]
    fn computes_hour_difference() {
        let a = DateTime::parse("2022090100").unwrap();
        let b = DateTime::parse("2022090116").unwrap();
        assert_eq!(b.epoch_seconds() - a.epoch_seconds(), 16 * 3600);
    }
}
