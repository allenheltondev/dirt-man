use uuid::Uuid;

/// IdGenerator trait for abstracting UUID generation
/// Provides method for generating UUID v4 identifiers
pub trait IdGenerator: Send + Sync {
    /// Generate a new UUID v4
    /// Returns a string representation of the UUID in hyphenated lowercase format
    fn uuid_v4(&self) -> String;
}

/// Production implementation of IdGenerator using random UUID generation
#[derive(Debug, Clone, Default)]
pub struct RandomIdGenerator;

impl RandomIdGenerator {
    pub fn new() -> Self {
        Self
    }
}

impl IdGenerator for RandomIdGenerator {
    fn uuid_v4(&self) -> String {
        Uuid::new_v4().to_string()
    }
}

/// Test implementation of IdGenerator with fixed/controllable UUIDs
/// Useful for deterministic testing
#[derive(Debug, Clone)]
pub struct FixedIdGenerator {
    uuids: Vec<String>,
    index: std::sync::Arc<std::sync::Mutex<usize>>,
}

impl FixedIdGenerator {
    /// Create a new FixedIdGenerator with a list of UUIDs to return in sequence
    /// When the list is exhausted, it wraps around to the beginning
    pub fn new(uuids: Vec<String>) -> Self {
        Self {
            uuids,
            index: std::sync::Arc::new(std::sync::Mutex::new(0)),
        }
    }

    /// Create a FixedIdGenerator that always returns the same UUID
    pub fn single(uuid: String) -> Self {
        Self::new(vec![uuid])
    }

    /// Create a FixedIdGenerator with a sequence of UUIDs from strings
    pub fn from_strings(uuid_strs: &[&str]) -> Self {
        Self::new(uuid_strs.iter().map(|s| s.to_string()).collect())
    }

    /// Reset the internal counter to start from the beginning
    pub fn reset(&self) {
        let mut index = self.index.lock().unwrap();
        *index = 0;
    }

    /// Get the current index (for testing purposes)
    pub fn current_index(&self) -> usize {
        *self.index.lock().unwrap()
    }
}

impl IdGenerator for FixedIdGenerator {
    fn uuid_v4(&self) -> String {
        let mut index = self.index.lock().unwrap();
        let uuid = self.uuids[*index % self.uuids.len()].clone();
        *index += 1;
        uuid
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_random_id_generator() {
        let generator = RandomIdGenerator::new();

        let uuid1 = generator.uuid_v4();
        let uuid2 = generator.uuid_v4();

        // Verify UUIDs are valid
        assert!(Uuid::parse_str(&uuid1).is_ok());
        assert!(Uuid::parse_str(&uuid2).is_ok());

        // Verify UUIDs are different (extremely unlikely to be the same)
        assert_ne!(uuid1, uuid2);

        // Verify format (lowercase with hyphens)
        assert_eq!(uuid1.len(), 36);
        assert_eq!(uuid1.chars().filter(|c| *c == '-').count(), 4);
        assert!(uuid1
            .chars()
            .all(|c| c.is_ascii_lowercase() || c.is_ascii_digit() || c == '-'));
    }

    #[test]
    fn test_random_id_generator_version() {
        let generator = RandomIdGenerator::new();
        let uuid_str = generator.uuid_v4();
        let uuid = Uuid::parse_str(&uuid_str).unwrap();

        // Verify it's UUID v4
        assert_eq!(uuid.get_version_num(), 4);
    }

    #[test]
    fn test_fixed_id_generator_single() {
        let test_uuid = "550e8400-e29b-41d4-a716-446655440000";
        let generator = FixedIdGenerator::single(test_uuid.to_string());

        // Should return the same UUID every time
        assert_eq!(generator.uuid_v4(), test_uuid);
        assert_eq!(generator.uuid_v4(), test_uuid);
        assert_eq!(generator.uuid_v4(), test_uuid);
    }

    #[test]
    fn test_fixed_id_generator_sequence() {
        let uuids = vec![
            "550e8400-e29b-41d4-a716-446655440000".to_string(),
            "7c9e6679-7425-40de-944b-e07fc1f90ae7".to_string(),
            "a1b2c3d4-e5f6-7890-abcd-ef1234567890".to_string(),
        ];
        let generator = FixedIdGenerator::new(uuids.clone());

        // Should return UUIDs in sequence
        assert_eq!(generator.uuid_v4(), uuids[0]);
        assert_eq!(generator.uuid_v4(), uuids[1]);
        assert_eq!(generator.uuid_v4(), uuids[2]);

        // Should wrap around
        assert_eq!(generator.uuid_v4(), uuids[0]);
        assert_eq!(generator.uuid_v4(), uuids[1]);
    }

    #[test]
    fn test_fixed_id_generator_from_strings() {
        let generator = FixedIdGenerator::from_strings(&[
            "550e8400-e29b-41d4-a716-446655440000",
            "7c9e6679-7425-40de-944b-e07fc1f90ae7",
        ]);

        assert_eq!(generator.uuid_v4(), "550e8400-e29b-41d4-a716-446655440000");
        assert_eq!(generator.uuid_v4(), "7c9e6679-7425-40de-944b-e07fc1f90ae7");
        assert_eq!(generator.uuid_v4(), "550e8400-e29b-41d4-a716-446655440000");
    }

    #[test]
    fn test_fixed_id_generator_reset() {
        let generator = FixedIdGenerator::from_strings(&[
            "550e8400-e29b-41d4-a716-446655440000",
            "7c9e6679-7425-40de-944b-e07fc1f90ae7",
        ]);

        assert_eq!(generator.uuid_v4(), "550e8400-e29b-41d4-a716-446655440000");
        assert_eq!(generator.uuid_v4(), "7c9e6679-7425-40de-944b-e07fc1f90ae7");

        // Reset and start over
        generator.reset();
        assert_eq!(generator.uuid_v4(), "550e8400-e29b-41d4-a716-446655440000");
        assert_eq!(generator.uuid_v4(), "7c9e6679-7425-40de-944b-e07fc1f90ae7");
    }

    #[test]
    fn test_fixed_id_generator_current_index() {
        let generator = FixedIdGenerator::from_strings(&[
            "550e8400-e29b-41d4-a716-446655440000",
            "7c9e6679-7425-40de-944b-e07fc1f90ae7",
        ]);

        assert_eq!(generator.current_index(), 0);
        generator.uuid_v4();
        assert_eq!(generator.current_index(), 1);
        generator.uuid_v4();
        assert_eq!(generator.current_index(), 2);
        generator.uuid_v4();
        assert_eq!(generator.current_index(), 3);
    }

    #[test]
    fn test_id_generator_trait_object() {
        // Verify IdGenerator trait can be used as a trait object
        let random_gen: Box<dyn IdGenerator> = Box::new(RandomIdGenerator::new());
        let fixed_gen: Box<dyn IdGenerator> = Box::new(FixedIdGenerator::single(
            "550e8400-e29b-41d4-a716-446655440000".to_string(),
        ));

        // Both should work through trait object
        let uuid1 = random_gen.uuid_v4();
        assert!(Uuid::parse_str(&uuid1).is_ok());

        let uuid2 = fixed_gen.uuid_v4();
        assert_eq!(uuid2, "550e8400-e29b-41d4-a716-446655440000");
    }

    #[test]
    fn test_fixed_id_generator_thread_safe() {
        use std::sync::Arc;
        use std::thread;

        let generator = Arc::new(FixedIdGenerator::from_strings(&[
            "550e8400-e29b-41d4-a716-446655440000",
            "7c9e6679-7425-40de-944b-e07fc1f90ae7",
        ]));

        let mut handles = vec![];

        // Spawn multiple threads that generate UUIDs
        for _ in 0..10 {
            let gen = Arc::clone(&generator);
            let handle = thread::spawn(move || gen.uuid_v4());
            handles.push(handle);
        }

        // Collect results
        let results: Vec<String> = handles.into_iter().map(|h| h.join().unwrap()).collect();

        // All results should be valid UUIDs from our list
        for result in results {
            assert!(
                result == "550e8400-e29b-41d4-a716-446655440000"
                    || result == "7c9e6679-7425-40de-944b-e07fc1f90ae7"
            );
        }
    }
}
