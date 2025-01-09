# Contributing to Video2X

Thank you for considering contributing to Video2X. This document outlines the guidelines for contributing to ensure a smooth and effective development process. Should you have any questions or require assistance, please do not hesitate to reach out to the project maintainers.

## Commit Messages

Commit messages must follow the [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) specification. This helps maintain a consistent and informative project history.

### Commit Message Format

```text
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

#### Common Types

- **feat**: A new feature
- **fix**: A bug fix
- **docs**: Documentation updates
- **perf**: Performance improvements that do not affect the code's behavior
- **style**: Changes that do not affect the code's functionality (e.g., formatting)
- **refactor**: Code changes that neither fix a bug nor add a feature
- **test**: Adding or modifying tests
- **chore**: Maintenance or other non-functional updates

#### Common Scopes

Including a scope is optional but is strongly encouraged. One commit should only address changes to a single module or component. If a change must affect multiple modules, use `*` as the scope.

- **avutils**: The audio/video utilities
- **conversions**: The video format conversion utilities
- **decoder**: The video decoder module
- **encoder**: The video encoder module
- **fsutils**: The file system utilities
- **logging**: Any logging-related changes
- **libplacebo**: The libplacebo filter
- **realesrgan**: The Real-ESRGAN filter
- **realcugan**: The Real-CUGAN filter
- **rife**: The RIFE frame interpolator
- **video2x**: The Video2X command-line interface

#### Example

```
feat(encoder): add support for specifying video pixel format

Add the `pix_fmt` field to the `EncoderConfig` struct to allow users to specify the pixel format for encoding.

Closes #12345
```

## Documentation of Changes

All changes must be documented in the `CHANGELOG.md` file. The changelog must adhere to the [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) format.

### Example Changelog Entry

```markdown
## [Unreleased]

### Added

- Support for specifying video pixel format in the encoder module (#12345).

### Fixed

- A memory leak in the video encoder module (#23456).
```

## Coding Standards

All code contributions must strictly follow the coding standards outlined in this section. These standards help maintain code quality, readability, and consistency throughout the project. Before submitting any code changes, ensure your code adheres to these guidelines.

### C++ Code Style

C++ code must follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html). This ensures consistency and readability across the codebase. Additionally:

- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) instead of raw pointers wherever possible.
- Use `#pragma once` for header guards.
- Use `#include` directives in the following order:
  1. Related header files
  2. C++ standard library headers
  3. Third-party library headers
  4. Project-specific headers
- Never check pointers with implicit conversion to `bool`; always perform an explicit comparison with `nullptr`.
- Always set pointers to `nullptr` after freeing the associated memory.

### Code Formatting

All C++ code must be formatted using `clang-format` with the project's `.clang-format` configuration file before submitting a pull request. This helps maintain a uniform code style.

## Submitting a Pull Request

1. **Fork the repository**: Create a personal fork of the project.
2. **Create a branch**: Create a new branch for your changes:
   ```bash
   git checkout -b <type>/<scope>
   ```
3. **Write code**: Make your changes, ensuring they adhere to the coding standards and are properly documented.
4. **Document changes**: Update `CHANGELOG.md` with your changes.
5. **Commit changes**: Write clear and descriptive commit messages using the Conventional Commits format.
6. **Push changes**: Push your branch to your fork:
   ```bash
   git push origin <type>/<scope>
   ```
7. **Open a pull request**: Submit your pull request to the `master` branch of the original repository. Include a clear description of the changes made and reference any relevant issues.

## Code Reviews

All pull requests will undergo a code review. Please expect feedback from the maintainers after you submit the pull request. We may need further information or changes before merging your pull request.
