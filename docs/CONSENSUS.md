# Monero Consensus Rules

This document describes the consensus rules for the Monero cryptocurrency, including the Proof of Proof (PoP) mechanism introduced in hardfork version 17.

## Overview

Monero uses a proof-of-work (PoW) consensus mechanism based on RandomX. The Proof of Proof (PoP) enhancement allows miners to include "uncle blocks" in their coinbase transactions to increase their chain's weight and help mitigate selfish mining attacks.

## Proof of Proof (PoP) Mechanism

### What is PoP?

PoP is a consensus enhancement that allows miners to reference late blocks (blocks that arrived after a certain time threshold) as "uncle blocks" in their coinbase transactions. This increases the weight of their chain and helps prevent selfish mining attacks by allowing honest miners to include late blocks that would otherwise be orphaned.

### Key Parameters

- **POP_K**: Maximum number of uncle blocks allowed per coinbase transaction (set to 3)
- **POP_D**: Time threshold in seconds for considering a block "late" (set to 5 seconds)
- **Hardfork Version**: PoP is activated starting from hardfork version 17

### How PoP Works

1. **Late Block Detection**: A block is considered "late" if it arrives more than POP_D seconds after another block at the same height
2. **Uncle Inclusion**: Miners can include up to POP_K late blocks as "uncle blocks" in their coinbase transaction's extra field
3. **Chain Weight Calculation**: The chain weight includes the weight of all blocks plus the weight of included uncle blocks
4. **Chain Selection**: When multiple chains exist, the chain with the highest PoP weight is selected
5. **Tie Breaking**: If chains have equal weight, deterministic tie-breaking uses lexicographical comparison of block hashes

### Benefits

- **Selfish Mining Mitigation**: Honest miners can include late blocks from selfish miners as uncles, increasing their chain's weight
- **Network Efficiency**: Reduces block orphan rate by allowing late blocks to contribute to chain weight
- **Fairness**: Provides a mechanism for late blocks to still contribute to the network's security

### Consensus Rules

#### Block Validation
- Blocks must include valid uncle blocks if any are specified
- Uncle blocks must be at the same height as the including block
- Uncle blocks must have valid structure and timestamps
- Maximum POP_K uncle blocks per coinbase transaction

#### Chain Selection
- Chains are compared by their PoP weight (block weight + uncle weight)
- Higher weight chains are preferred
- Equal weight chains use deterministic tie-breaking

#### Hardfork Activation
- PoP rules are enforced starting from hardfork version 17
- Earlier versions do not validate or use uncle blocks

## Technical Implementation

### Uncle Block Structure

Uncle blocks are stored in the coinbase transaction's extra field using the `tx_extra_uncle_block` structure:

```cpp
struct tx_extra_uncle_block {
    uint8_t major_version;    // Hardfork version of the uncle block
    uint8_t minor_version;    // Minor version (typically 0)
    uint64_t timestamp;       // Unix timestamp of the uncle block
    crypto::hash prev_id;     // Hash of the previous block
    uint32_t nonce;           // Nonce used in mining the uncle block
};
```

#### Structure Details

- **major_version**: Must match the hardfork version (17 for PoP-enabled blocks)
- **minor_version**: Currently set to 0, reserved for future use
- **timestamp**: Unix timestamp when the uncle block was created
- **prev_id**: Hash of the block this uncle block builds upon (must be at same height)
- **nonce**: The nonce value that satisfied the proof-of-work for this uncle block

#### Serialization

Uncle blocks are serialized using Monero's standard serialization format and stored in the transaction's extra field with tag `TX_EXTRA_TAG_UNCLE_BLOCK` (0x05).

```cpp
// Multiple uncle blocks are stored sequentially in extra
std::vector<uint8_t> extra;
for (const auto& uncle : uncles) {
    // Write tag
    cryptonote::write_varint(extra, TX_EXTRA_TAG_UNCLE_BLOCK);
    // Write size
    std::string uncle_blob = t_serializable_object_to_blob(uncle);
    cryptonote::write_varint(extra, uncle_blob.size());
    // Write data
    extra.insert(extra.end(), uncle_blob.begin(), uncle_blob.end());
}
```

### Weight Calculation

The PoP chain weight is calculated as:
```
weight = sum(block_weights) + sum(uncle_weights)
```

Where:
- `block_weights` are the weights of all blocks in the chain
- `uncle_weights` are the weights of all included uncle blocks

### Late Block Detection

A block is considered late if:
```
block.timestamp > reference_block.timestamp + POP_D
```

Where `reference_block` is another block at the same height.

## Miner Participation

Miners can participate in PoP by:

1. Monitoring for late blocks at their current mining height
2. Including up to POP_K late blocks in their coinbase transaction
3. Ensuring uncle blocks are properly formatted and valid

### Miner Participation Examples

#### Basic Uncle Block Creation

```cpp
// Example: Creating an uncle block from a late-arriving block
cryptonote::tx_extra_uncle_block uncle;
uncle.major_version = 17;  // Current hardfork version
uncle.minor_version = 0;
uncle.timestamp = late_block.timestamp;
uncle.prev_id = late_block.prev_id;
uncle.nonce = late_block.nonce;

// Validate the uncle block
if (validate_uncle_block(uncle, current_height, 17)) {
    // Include in coinbase transaction
    coinbase_tx.extra = serialize_uncle(uncle);
}
```

#### Multiple Uncle Blocks

```cpp
// Example: Including multiple uncle blocks (up to POP_K = 3)
std::vector<cryptonote::tx_extra_uncle_block> uncles;

for (const auto& late_block : detected_late_blocks) {
    if (uncles.size() >= POP_K) break;  // Maximum 3 uncles

    cryptonote::tx_extra_uncle_block uncle = create_uncle_from_block(late_block);
    if (validate_uncle_block(uncle, current_height, 17)) {
        uncles.push_back(uncle);
    }
}

// Include all valid uncles in coinbase
if (!uncles.empty()) {
    coinbase_tx.extra = serialize_uncles(uncles);
}
```

#### Mining Software Integration

```cpp
// Example mining loop with PoP support
while (mining) {
    // Check for late blocks at current height
    auto late_blocks = detect_late_blocks(current_height);

    // Create coinbase with uncles if available
    auto coinbase = create_coinbase_with_uncles(late_blocks);

    // Mine the block
    auto block = mine_block(coinbase, current_height);

    // Submit to network
    submit_block(block);
}
```

#### Late Block Detection

```cpp
// Example: Detecting late blocks
std::vector<block> detect_late_blocks(uint64_t height) {
    std::vector<block> late_blocks;

    // Get all blocks at current height
    auto blocks_at_height = get_blocks_at_height(height);

    if (blocks_at_height.size() > 1) {
        // Find the earliest block at this height
        auto earliest = *std::min_element(blocks_at_height.begin(),
                                        blocks_at_height.end(),
                                        [](const block& a, const block& b) {
                                            return a.timestamp < b.timestamp;
                                        });

        // Check which blocks are late
        for (const auto& blk : blocks_at_height) {
            if (blk.timestamp > earliest.timestamp + POP_D) {
                late_blocks.push_back(blk);
            }
        }
    }

    return late_blocks;
}
```

## Security Considerations

- Uncle blocks must be validated to prevent invalid blocks from being included
- Chain weight calculations must be accurate to prevent manipulation
- Tie-breaking mechanism ensures deterministic chain selection
- PoP parameters (POP_K, POP_D) are carefully chosen to balance security and efficiency

## Future Enhancements

PoP provides a foundation for future consensus improvements and can be extended to support:
- More sophisticated uncle block validation
- Dynamic parameter adjustment
- Enhanced selfish mining resistance
- Improved network partition handling

## References

- [Monero Research Lab](https://src.getmonero.org/resources/research-lab/)
- Hardfork version 17 implementation
- PoP test suites in `tests/unit_tests/pop_tests.cpp` and `tests/core_tests/pop_tests.cpp`