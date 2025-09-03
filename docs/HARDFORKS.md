# Monero Hardforks

This document describes the hardfork schedule and changes for the Monero cryptocurrency network.

## Hardfork Overview

Monero uses scheduled hardforks to implement new features, improve security, and maintain network consensus. Each hardfork activates at a specific block height and introduces new consensus rules.

## Current Hardfork: Version 17

### Activation Details
- **Block Height**: 2789608 (mainnet), 2083520 (testnet), 1251720 (stagenet)
- **Activation Time**: October 17, 2024 (estimated)
- **Timestamp**: 1756928243

### New Features in Hardfork 17

#### Proof of Proof (PoP) Consensus Enhancement

Hardfork 17 introduces the Proof of Proof (PoP) mechanism, a significant enhancement to Monero's consensus rules designed to mitigate selfish mining attacks and improve network efficiency.

##### Key Changes

1. **Uncle Block Support**
   - Miners can now include "uncle blocks" in their coinbase transactions
   - Uncle blocks are late blocks that arrived after the POP_D time threshold
   - Maximum POP_K = 3 uncle blocks per coinbase transaction

2. **Enhanced Chain Selection**
   - Chain weight now includes the weight of uncle blocks
   - Higher weight chains are preferred during chain selection
   - Deterministic tie-breaking using block hash comparison

3. **Selfish Mining Mitigation**
   - Honest miners can include late blocks from selfish miners as uncles
   - This increases the honest chain's weight and helps prevent attacks

##### Technical Parameters

- **POP_K**: 3 (maximum uncle blocks per transaction)
- **POP_D**: 5 seconds (late block threshold)
- **Activation**: Hardfork version 17

##### Consensus Rules

- Uncle blocks must be properly formatted and valid
- Uncle blocks must reference the same height as the including block
- Chain weight calculation includes uncle block weights
- Invalid uncle blocks cause the entire block to be invalid

## Historical Hardforks

| Version | Block Height | Date | Major Changes |
|---------|-------------|------|---------------|
| 1 | 1 | 2014-04-18 | Initial launch |
| 2 | 1009827 | 2016-03-22 | Ring size >= 3, 120s block time |
| 3 | 1141317 | 2016-09-21 | Coinbase denomination splits |
| 4 | 1220516 | 2017-01-05 | RingCT transactions |
| 5 | 1288616 | 2017-04-15 | Fee algorithm adjustment |
| 6 | 1400000 | 2017-09-16 | RingCT only, minimum ring size 5 |
| 7 | 1546000 | 2018-04-06 | Cryptonight variant 1, ring size >= 7 |
| 8 | 1685555 | 2018-10-18 | Bulletproofs, ring size 11, CNv2 |
| 9 | 1686275 | 2018-10-19 | Bulletproofs required |
| 10 | 1788000 | 2019-03-09 | CN/R, dynamic block weight |
| 11 | 1788720 | 2019-03-10 | Forbid old RingCT format |
| 12 | 1978433 | 2019-11-30 | RandomX, 2-output minimum |
| 13 | 2210000 | 2020-10-17 | CLSAG transaction format |
| 14 | 2210720 | 2020-10-18 | Forbid old MLSAG format |
| 15 | 2688888 | 2022-08-13 | Ring size 16, Bulletproofs+, view tags |
| 16 | 2689608 | 2022-08-14 | Forbid old v14 format |
| **17** | **2789608** | **2024-10-17** | **Proof of Proof (PoP) consensus** |

## Hardfork Process

### Development Phase
1. New features are developed and tested
2. Code is reviewed and merged into the master branch
3. Release branches are created approximately 3 months before activation

### Activation Phase
1. Nodes upgrade to the new software version
2. Network reaches consensus on the hardfork block height
3. New consensus rules activate automatically at the specified height

### Post-Activation
1. Old consensus rules are no longer valid
2. All nodes must run compatible software
3. Network continues with enhanced features

## Testing and Validation

### Test Networks
- **Testnet**: Used for testing new features before mainnet deployment
- **Stagenet**: Additional testing network for stability validation

### Test Coverage
- Unit tests for consensus rules
- Integration tests for hardfork activation
- Network stress testing
- Security audits

## Future Hardforks

Future hardforks will continue to enhance Monero's privacy, security, and efficiency. The community and developers work together to determine the features and timeline for each upgrade.

### Planning Process
1. Research and development of new features
2. Community discussion and feedback
3. Technical specification and implementation
4. Testing and validation
5. Network activation

## References

- [Monero Hardfork Schedule](https://github.com/monero-project/monero#scheduled-software-upgrades)
- [Hardfork Implementation](src/hardforks/hardforks.cpp)
- [Consensus Rules Documentation](CONSENSUS.md)
- [Monero Research Lab](https://src.getmonero.org/resources/research-lab/)