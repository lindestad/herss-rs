use std::fmt;

pub type Result<T> = std::result::Result<T, HerssError>;

#[derive(Debug)]
pub struct HerssError {
    message: String,
}

impl HerssError {
    pub fn new(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
        }
    }
}

impl fmt::Display for HerssError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.message)
    }
}

impl std::error::Error for HerssError {}

impl From<std::io::Error> for HerssError {
    fn from(value: std::io::Error) -> Self {
        Self::new(value.to_string())
    }
}

impl From<std::num::ParseFloatError> for HerssError {
    fn from(value: std::num::ParseFloatError) -> Self {
        Self::new(value.to_string())
    }
}

impl From<std::num::ParseIntError> for HerssError {
    fn from(value: std::num::ParseIntError) -> Self {
        Self::new(value.to_string())
    }
}
