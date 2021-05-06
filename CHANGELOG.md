# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.3] - 2021-05-06
### Changed
- Ubuntu Focal Fossa support

## [1.1.2] - 2019-04-04
### Changed
- Node waits much longer (>1 year) before deciding to overwrite inactive account

## [1.1.1] - 2019-03-28
### Fixed
- Deleted account handling

## [1.1.0] - 2019-02-15
### Added
- Error codes and more descriptive error info returned by client
- `get_log` now accept requests for other accounts in the same node
- Allow to specify whitelist of IPs allowed to connect to node
- Allow nodes to transparently redirect clients to another endpoints

### Changed
- Client protocol upgrade. New client is not compatible with nodes version 1.0.4 and lower
- New nodes are compatible with older clients
- Client will not accept command with unknown fields
- `get_log` for reused account will not return previous owner history by default
- Dividend calculations for inactive accounts

### Fixed
- Stability improvements
- `decode_raw` validates data length
- Properly handle `change_account_key` command when changing to the same key

## [1.0.4] - 2018-11-15
### Fixed
- Stability improvements for peer communication
- Node logs cleanup

## [1.0.3] - 2018-10-09
### Changed
- Modified DB rollback mechanism to support starting from backup

### Fixed
- Accounts database snapshots during dividend block
- Further stability improvements for peer communication
- Node debug logs cleanup

## [1.0.2] - 2018-09-27
### Fixed
- Node did not stop when there was not enough signatures
- Improved peer discovery
- On rare occasions node could suddenly stop all peer communication [#151](https://github.com/adshares/ads/issues/151)

## [1.0.1] - 2018-09-20
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

[Unreleased]: https://github.com/adshares/ads/compare/v1.1.3...HEAD

[1.1.3]: https://github.com/adshares/ads/compare/v1.1.2...v1.1.3
[1.1.2]: https://github.com/adshares/ads/compare/v1.1.1...v1.1.2
[1.1.1]: https://github.com/adshares/ads/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/adshares/ads/compare/v1.0.4...v1.1.0
[1.0.4]: https://github.com/adshares/ads/compare/v1.0.3...v1.0.4
[1.0.3]: https://github.com/adshares/ads/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/adshares/ads/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/adshares/ads/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/adshares/ads/compare/v0.0.6...v1.0.0
[0.0.6]: https://github.com/adshares/ads/compare/v0.0.5...v0.0.6
[0.0.5]: https://github.com/adshares/ads/compare/v0.0.4...v0.0.5
[0.0.4]: https://github.com/adshares/ads/compare/v0.0.3...v0.0.4
