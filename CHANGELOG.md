# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.1] - 2018-08-24
### Changed
- New log format
- Log level can be changed without recompilation

### Fixed
- Vote counting during full synchronization of the node
- Now secret provided from terminal is not printed on screen
- Fixed network timeout when using client in a interactive mode

## [1.0.0] - 2018-08-24
### Changed
- Genesis file for the launch
- Update README

## [0.0.6] - 2018-08-10
### Added
- Profit sharing among TOP and VIP nodes

### Changed
- Genesis file for test launch

### Fixed
- Vote counting in get_block command
- Ability to send externally signed transactions

## [0.0.5] - 2018-08-01
### Added
- Support passing version by cmake command.
- Contributing guide
- "Dry run" test case

### Fixed
- Node stops after incorrect `send_many` request  - #134
- Fee checking
- Internal transactions numbers

## [0.0.4] - 2018-07-27
### Changed
- Update default parameters
- Creating a dev version with reference to the last tag

[Unreleased]: https://github.com/adshares/ads/compare/v1.0.1...HEAD

[1.0.1]: https://github.com/adshares/ads/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/adshares/ads/compare/v0.0.6...v1.0.0
[0.0.6]: https://github.com/adshares/ads/compare/v0.0.5...v0.0.6
[0.0.5]: https://github.com/adshares/ads/compare/v0.0.4...v0.0.5
[0.0.4]: https://github.com/adshares/ads/compare/v0.0.3...v0.0.4
