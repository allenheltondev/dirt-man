// Re-export the api_keys repository functions from the parent repo directory
// The control plane uses the same DynamoDB operations as the data plane

// Import from the parent repo directory (src/repo/api_keys.rs)
#[path = "../../repo/api_keys.rs"]
mod parent_api_keys;

pub use parent_api_keys::*;
