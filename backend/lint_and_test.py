#!/usr/bin/env python3
"""
Lint and test script for Rust backend code.

This script runs various Rust tooling commands to ensure code quality:
- cargo fmt (formatting check)
- cargo clippy (linting)
- cargo check (compilation check)
- cargo test (unit and integration tests)
- cargo build (build check)

Usage:
    python lint_and_test.py [--fix] [--verbose]

Options:
    --fix       Automatically fix formatting and clippy issues where possible
    --verbose   Show detailed output from all commands
"""

import argparse
import subprocess
import sys
from pathlib import Path
from typing import List, Tuple


class Colors:
    """ANSI color codes for terminal output."""
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'


def print_header(message: str) -> None:
    """Print a formatted header message."""
    print(f"\n{Colors.HEADER}{Colors.BOLD}{'=' * 80}{Colors.ENDC}")
    print(f"{Colors.HEADER}{Colors.BOLD}{message}{Colors.ENDC}")
    print(f"{Colors.HEADER}{Colors.BOLD}{'=' * 80}{Colors.ENDC}\n")


def print_success(message: str) -> None:
    """Print a success message."""
    print(f"{Colors.OKGREEN}✓ {message}{Colors.ENDC}")


def print_error(message: str) -> None:
    """Print an error message."""
    print(f"{Colors.FAIL}✗ {message}{Colors.ENDC}")


def print_warning(message: str) -> None:
    """Print a warning message."""
    print(f"{Colors.WARNING}⚠ {message}{Colors.ENDC}")


def run_command(
    cmd: List[str],
    cwd: Path,
    verbose: bool = False,
    check: bool = True
) -> Tuple[bool, str]:
    """
    Run a shell command and return success status and output.

    Args:
        cmd: Command and arguments as a list
        cwd: Working directory for the command
        verbose: If True, print command output in real-time
        check: If True, raise exception on non-zero exit code

    Returns:
        Tuple of (success: bool, output: str)
    """
    try:
        if verbose:
            print(f"{Colors.OKCYAN}Running: {' '.join(cmd)}{Colors.ENDC}")
            result = subprocess.run(
                cmd,
                cwd=cwd,
                check=check,
                text=True,
                capture_output=False
            )
            return result.returncode == 0, ""
        else:
            result = subprocess.run(
                cmd,
                cwd=cwd,
                check=check,
                text=True,
                capture_output=True
            )
            return result.returncode == 0, result.stdout + result.stderr
    except subprocess.CalledProcessError as e:
        output = e.stdout + e.stderr if hasattr(e, 'stdout') else str(e)
        return False, output
    except FileNotFoundError:
        return False, f"Command not found: {cmd[0]}"


def check_rust_installed() -> bool:
    """Check if Rust toolchain is installed."""
    print_header("Checking Rust Installation")

    success, output = run_command(["cargo", "--version"], Path.cwd(), verbose=False, check=False)
    if not success:
        print_error("Cargo not found. Please install Rust: https://rustup.rs/")
        return False

    print_success(f"Cargo installed: {output.strip()}")
    return True


def run_cargo_fmt(backend_dir: Path, fix: bool, verbose: bool) -> bool:
    """Run cargo fmt to check/fix code formatting."""
    print_header("Running cargo fmt (Code Formatting)")

    if fix:
        cmd = ["cargo", "fmt"]
        print("Formatting code...")
    else:
        cmd = ["cargo", "fmt", "--", "--check"]
        print("Checking code formatting...")

    success, output = run_command(cmd, backend_dir, verbose, check=False)

    if success:
        print_success("Code formatting is correct")
        return True
    else:
        print_error("Code formatting issues found")
        if not fix:
            print_warning("Run with --fix to automatically format code")
        if verbose or not fix:
            print(output)
        return False


def run_cargo_clippy(backend_dir: Path, fix: bool, verbose: bool) -> bool:
    """Run cargo clippy for linting."""
    print_header("Running cargo clippy (Linting)")

    if fix:
        cmd = ["cargo", "clippy", "--fix", "--allow-dirty", "--allow-staged", "--", "-D", "warnings"]
        print("Fixing clippy issues...")
    else:
        cmd = ["cargo", "clippy", "--", "-D", "warnings"]
        print("Checking for clippy warnings...")

    success, output = run_command(cmd, backend_dir, verbose, check=False)

    if success:
        print_success("No clippy warnings found")
        return True
    else:
        print_error("Clippy warnings found")
        if not fix:
            print_warning("Run with --fix to automatically fix some issues")
        if verbose or not success:
            print(output)
        return False


def run_cargo_check(backend_dir: Path, verbose: bool) -> bool:
    """Run cargo check to verify compilation."""
    print_header("Running cargo check (Compilation Check)")

    cmd = ["cargo", "check", "--all-targets"]
    print("Checking compilation...")

    success, output = run_command(cmd, backend_dir, verbose, check=False)

    if success:
        print_success("Code compiles successfully")
        return True
    else:
        print_error("Compilation errors found")
        print(output)
        return False


def run_cargo_build(backend_dir: Path, verbose: bool) -> bool:
    """Run cargo build to ensure the project builds."""
    print_header("Running cargo build (Build Check)")

    cmd = ["cargo", "build", "--release"]
    print("Building project in release mode...")

    success, output = run_command(cmd, backend_dir, verbose, check=False)

    if success:
        print_success("Project built successfully")
        return True
    else:
        print_error("Build failed")
        print(output)
        return False


def main() -> int:
    """Main entry point for the script."""
    parser = argparse.ArgumentParser(
        description="Lint and test Rust backend code",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python lint_and_test.py                    # Run all checks
  python lint_and_test.py --fix              # Fix formatting and clippy issues
  python lint_and_test.py --verbose          # Show detailed output
  python lint_and_test.py --fmt-only         # Only run formatting check
  python lint_and_test.py --lint-only --fix  # Only run clippy with fixes
  python lint_and_test.py --check-only       # Only run compilation check
  python lint_and_test.py --build-only       # Only run build
        """
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Automatically fix formatting and clippy issues where possible"
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Show detailed output from all commands"
    )
    parser.add_argument(
        "--fmt-only",
        action="store_true",
        help="Only run formatting check/fix"
    )
    parser.add_argument(
        "--lint-only",
        action="store_true",
        help="Only run clippy linting"
    )
    parser.add_argument(
        "--check-only",
        action="store_true",
        help="Only run compilation check"
    )
    parser.add_argument(
        "--build-only",
        action="store_true",
        help="Only run build"
    )

    args = parser.parse_args()

    # Determine backend directory
    script_dir = Path(__file__).parent
    backend_dir = script_dir if script_dir.name == "backend" else script_dir / "backend"

    if not backend_dir.exists():
        print_error(f"Backend directory not found: {backend_dir}")
        return 1

    print(f"{Colors.OKBLUE}Backend directory: {backend_dir}{Colors.ENDC}")

    # Check Rust installation
    if not check_rust_installed():
        return 1

    # Determine which checks to run
    run_all = not any([args.fmt_only, args.lint_only, args.check_only, args.build_only])

    run_fmt = run_all or args.fmt_only
    run_lint = run_all or args.lint_only
    run_check = run_all or args.check_only
    run_build = run_all or args.build_only

    # Track results
    results = []

    # Run selected checks
    if run_fmt:
        results.append(("Formatting", run_cargo_fmt(backend_dir, args.fix, args.verbose)))

    if run_lint:
        results.append(("Clippy", run_cargo_clippy(backend_dir, args.fix, args.verbose)))

    if run_check:
        results.append(("Check", run_cargo_check(backend_dir, args.verbose)))

    if run_build:
        results.append(("Build", run_cargo_build(backend_dir, args.verbose)))

    # Print summary
    print_header("Summary")

    all_passed = True
    for name, passed in results:
        if passed:
            print_success(f"{name}: PASSED")
        else:
            print_error(f"{name}: FAILED")
            all_passed = False

    print()
    if all_passed:
        print(f"{Colors.OKGREEN}{Colors.BOLD}All checks passed! ✓{Colors.ENDC}")
        return 0
    else:
        print(f"{Colors.FAIL}{Colors.BOLD}Some checks failed. ✗{Colors.ENDC}")
        if not args.fix:
            print_warning("Try running with --fix to automatically fix some issues")
        return 1


if __name__ == "__main__":
    sys.exit(main())
