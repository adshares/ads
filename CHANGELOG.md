# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.0.1] - 2018-07-DD
### Added
- New methods:
  - `broadcast` broadcast a message on the network
  - `change_account_key` change public key of my account
  -  `change_node_key`	change public key of a node
  -  `create_account`	create new user account
  -  `create_node`	create new node
  -  `get_account`	get account info by address
  -  `get_blocks`	download and store block headers
  -  `get_broadcast`	download broadcast messages
  -  `get_log`	download and store my transaction log
  -  `get_me`	get my account info
  -  `get_transaction`	get info about a single transaction
  -  `get_vipkeys`	get list of public keys of VIP nodes
  -  `get_signatures`	get signatures of a block
  -  `get_block`	get block header and list of nodes
  -  `get_accounts`	get account of a node
  -  `get_message_list`	get list of messages in a block
  -  `get_message`	get message in a block
  -  `retrieve_funds`	retrieve funds from a remote node
  -  `send_again`	resend a transaction
  -  `send_one`	send payment to one destination
  -  `send_many`	send payment to many destinations
  -  `set_account_status`	set status bits of a user account
  -  `set_node_status`	set status bits of a node
  -  `unset_account_status`	unset status bits of a user account
  -  `unset_node_status`	unset status bits of a node

[Unreleased]: https://github.com/adshares/ads/compare/v0.0.1...HEAD

[0.0.2]: https://github.com/adshares/ads/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/adshares/ads/releases/tag/v0.0.1