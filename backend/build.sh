#!/bin/bash
# Build script for Serverless Backend API Lambda functions
# This script builds both Data Plane and Control Plane Lambda functions using cargo-lambda

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Building Serverless Backend API${NC}"
echo -e "${GREEN}========================================${NC}"

# Check if cargo-lambda is installed
if ! command -v cargo-lambda &> /dev/null; then
    echo -e "${RED}Error: cargo-lambda is not installed${NC}"
    echo -e "${YELLOW}Install it with: pip install cargo-lambda${NC}"
    echo -e "${YELLOW}Or via Homebrew: brew install cargo-lambda${NC}"
    exit 1
fi

# Check if we're in the backend directory
if [ ! -f "template.yaml" ]; then
    echo -e "${RED}Error: template.yaml not found${NC}"
    echo -e "${YELLOW}Please run this script from the backend/ directory${NC}"
    exit 1
fi

# Build shared crate first
echo -e "\n${YELLOW}Building shared crate...${NC}"
cd shared
cargo build --release
cd ..

# Build Data Plane Lambda
echo -e "\n${YELLOW}Building Data Plane Lambda...${NC}"
cd data
cargo lambda build --release --arm64
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Data Plane Lambda built successfully${NC}"
else
    echo -e "${RED}✗ Data Plane Lambda build failed${NC}"
    exit 1
fi
cd ..

# Build Control Plane Lambda
echo -e "\n${YELLOW}Building Control Plane Lambda...${NC}"
cd control
cargo lambda build --release --arm64
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Control Plane Lambda built successfully${NC}"
else
    echo -e "${RED}✗ Control Plane Lambda build failed${NC}"
    exit 1
fi
cd ..

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "\nBinaries are located in:"
echo -e "  - data/target/lambda/data/"
echo -e "  - control/target/lambda/control/"
echo -e "\nNext steps:"
echo -e "  1. Run ${YELLOW}./deploy.sh <environment>${NC} to deploy"
echo -e "  2. Or run ${YELLOW}sam deploy${NC} manually with your parameters"
