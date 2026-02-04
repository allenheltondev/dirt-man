// Declare modules at the root level
pub mod cursor;
pub mod domain;
pub mod error;
pub mod id_generator;
pub mod time;
pub mod validators;

// Test utilities module (available in test and integration test builds)
#[cfg(any(test, feature = "test-utils"))]
pub mod test_utils;

// Re-export everything under a shared namespace for external access
pub mod shared {
    pub use super::cursor;
    pub use super::domain;
    pub use super::error;
    pub use super::id_generator;
    pub use super::time;
    pub use super::validators;
}

// Also re-export at root for convenience
pub use cursor::*;
pub use domain::*;
pub use error::*;
pub use id_generator::*;
pub use time::*;
pub use validators::*;
