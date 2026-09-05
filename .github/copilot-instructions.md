## Project Overview
A C++20 application to manage settings with fzy search capabilities
- **src** : main source code
- **tests** : unit test suit

## Build Commands
```bash
cmake --preset debug          # Configure (debug preset)
cmake --build --preset debug  # Build all targets
```

## Build for Coverage
```bash
cmake --preset debug-coverage
cmake --build --preset debug-coverage
```

Then to run the test use
```bash
cd {to-test-exe-path}
Microsoft.CodeCoverage.Console.exe instrument [<input-file>]
Microsoft.CodeCoverage.Console.exe collect [<command> [<args>...]] --output {test-filename}.coverage
Microsoft.CodeCoverage.Console.exe merge <files>... -f cobertura -o coverage.cobertura.xml

```

## Code Conventions (STRICT)
- **snake_case** for EVERYTHING: variables, methods, functions, classes, namespaces
- **trailing return types** for all functions
- **[[nodiscard]]** decorator only when really necesary to prevent code errors like event subscribers, but not on every non-void return method

## Documentation folders
- There are a few documentation folders:
  - `docs/`: For user-facing documentation, including README, user guides, and tutorials
  - `docs/internal/`: For internal dev design docs, architecture notes, and developer guides
  - `issues/`: For issue tracking, feature requests, and archived discussions
  - `issues/archived/`: For archived issues and discussions that are no longer active and MUST NOT BE USED as reference for current development. These are kept for historical purposes only and may contain outdated information. Please do not reference these files for current development.
  - `issues/mitigation/`: For mitigation plans of static analysis of source code that are being actively worked on. These files contain the probably up-to-date information and can be used as reference for current development. However, be careful to check the timestamps and ensure that the information is still relevant to your current work.

  ## Limitations
  - Do not use documentation files in `issues/archived/` or `docs/benchmarks` for current development. These files are kept for historical purposes only and may contain outdated information. Please do not reference these files for current development.
  