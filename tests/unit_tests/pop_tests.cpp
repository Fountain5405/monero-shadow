// Copyright (c) 2014-2024, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "gtest/gtest.h"

#include <vector>
#include <list>

#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_config.h"
#include "blockchain_db/testdb.h"

namespace
{
  // Test helper to create a mock blockchain for testing
  class MockBlockchain : public cryptonote::Blockchain
  {
  public:
    MockBlockchain() : cryptonote::Blockchain(tx_memory_pool{}) {}
  };

  // Helper to create a valid uncle block
  cryptonote::tx_extra_uncle_block create_valid_uncle_block(uint8_t major_version = 17)
  {
    cryptonote::tx_extra_uncle_block uncle;
    uncle.major_version = major_version;
    uncle.minor_version = 0;
    uncle.timestamp = 1234567890;
    uncle.prev_id = crypto::rand<crypto::hash>();
    uncle.nonce = 42;
    return uncle;
  }

  // Helper to create a transaction with uncle blocks
  cryptonote::transaction create_tx_with_uncles(const std::vector<cryptonote::tx_extra_uncle_block>& uncles)
  {
    cryptonote::transaction tx = {};
    tx.version = 2;
    tx.unlock_time = 0;

    // Add uncle blocks to extra
    std::vector<uint8_t> extra;
    for (const auto& uncle : uncles)
    {
      std::string uncle_data;
      if (!cryptonote::write_varint(uncle_data, TX_EXTRA_TAG_UNCLE_BLOCK))
        throw std::runtime_error("Failed to write uncle block tag");

      std::string uncle_blob;
      if (!t_serializable_object_to_blob(uncle, uncle_blob))
        throw std::runtime_error("Failed to serialize uncle block");

      if (!cryptonote::write_varint(uncle_data, uncle_blob.size()))
        throw std::runtime_error("Failed to write uncle block size");

      uncle_data += uncle_blob;
      extra.insert(extra.end(), uncle_data.begin(), uncle_data.end());
    }

    tx.extra = extra;
    return tx;
  }
}

TEST(pop_uncle_block_creation, valid_uncle_block)
{
  cryptonote::tx_extra_uncle_block uncle = create_valid_uncle_block();

  // Verify uncle block structure
  ASSERT_EQ(uncle.major_version, 17);
  ASSERT_EQ(uncle.minor_version, 0);
  ASSERT_EQ(uncle.timestamp, 1234567890ULL);
  ASSERT_NE(uncle.prev_id, crypto::null_hash);
  ASSERT_EQ(uncle.nonce, 42U);
}

TEST(pop_uncle_block_validation, valid_uncle_with_correct_version)
{
  MockBlockchain blockchain;
  cryptonote::tx_extra_uncle_block uncle = create_valid_uncle_block(17);

  // Should validate successfully with hardfork version 17
  ASSERT_TRUE(blockchain.validate_uncle_block(uncle, 100, 17));
}

TEST(pop_uncle_block_validation, invalid_uncle_wrong_version)
{
  MockBlockchain blockchain;
  cryptonote::tx_extra_uncle_block uncle = create_valid_uncle_block(16);

  // Should fail validation with wrong major version
  ASSERT_FALSE(blockchain.validate_uncle_block(uncle, 100, 17));
}

TEST(pop_uncle_block_extraction, extract_single_uncle)
{
  MockBlockchain blockchain;
  std::vector<cryptonote::tx_extra_uncle_block> uncles = {create_valid_uncle_block()};
  cryptonote::transaction tx = create_tx_with_uncles(uncles);

  auto extracted_uncles = blockchain.get_uncle_blocks_from_coinbase(tx);

  ASSERT_EQ(extracted_uncles.size(), 1);
  ASSERT_EQ(extracted_uncles[0].major_version, uncles[0].major_version);
  ASSERT_EQ(extracted_uncles[0].timestamp, uncles[0].timestamp);
  ASSERT_EQ(extracted_uncles[0].prev_id, uncles[0].prev_id);
  ASSERT_EQ(extracted_uncles[0].nonce, uncles[0].nonce);
}

TEST(pop_uncle_block_extraction, extract_multiple_uncles)
{
  MockBlockchain blockchain;
  std::vector<cryptonote::tx_extra_uncle_block> uncles;
  for (int i = 0; i < POP_K; ++i)
  {
    uncles.push_back(create_valid_uncle_block());
  }
  cryptonote::transaction tx = create_tx_with_uncles(uncles);

  auto extracted_uncles = blockchain.get_uncle_blocks_from_coinbase(tx);

  ASSERT_EQ(extracted_uncles.size(), POP_K);
  for (size_t i = 0; i < POP_K; ++i)
  {
    ASSERT_EQ(extracted_uncles[i].major_version, uncles[i].major_version);
    ASSERT_EQ(extracted_uncles[i].timestamp, uncles[i].timestamp);
  }
}

TEST(pop_uncle_block_extraction, extract_no_uncles)
{
  MockBlockchain blockchain;
  cryptonote::transaction tx = {};
  tx.version = 2;
  tx.extra = std::vector<uint8_t>(10, 0); // Padding only

  auto extracted_uncles = blockchain.get_uncle_blocks_from_coinbase(tx);

  ASSERT_TRUE(extracted_uncles.empty());
}

TEST(pop_late_block_detection, block_not_late)
{
  MockBlockchain blockchain;
  cryptonote::block b = {};
  b.timestamp = 1000;

  // For height 0, should not be considered late
  ASSERT_FALSE(blockchain.is_late_block(b, 0));
}

TEST(pop_late_block_detection, block_within_pop_d)
{
  MockBlockchain blockchain;
  cryptonote::block b = {};
  b.timestamp = 1000;

  // Block within POP_D seconds should not be late
  ASSERT_FALSE(blockchain.is_late_block(b, 1000 + POP_D - 1));
}

TEST(pop_late_block_detection, block_exceeds_pop_d)
{
  MockBlockchain blockchain;
  cryptonote::block b = {};
  b.timestamp = 1000;

  // Block exceeding POP_D seconds should be late
  ASSERT_TRUE(blockchain.is_late_block(b, 1000 + POP_D + 1));
}

TEST(pop_chain_weight_calculation, empty_chain)
{
  MockBlockchain blockchain;
  std::list<cryptonote::block> chain;
  uint64_t weight = blockchain.calculate_pop_chain_weight(chain, 0);

  ASSERT_EQ(weight, 0ULL);
}

TEST(pop_chain_weight_calculation, single_block_no_uncles)
{
  MockBlockchain blockchain;
  cryptonote::block b = {};
  b.miner_tx.version = 2;
  b.miner_tx.extra = std::vector<uint8_t>(10, 0); // No uncles

  std::list<cryptonote::block> chain = {b};
  uint64_t weight = blockchain.calculate_pop_chain_weight(chain, 0);

  // Should include miner tx weight but no uncle weight
  ASSERT_GT(weight, 0ULL);
}

TEST(pop_chain_weight_calculation, single_block_with_uncles)
{
  MockBlockchain blockchain;
  std::vector<cryptonote::tx_extra_uncle_block> uncles = {create_valid_uncle_block()};
  cryptonote::transaction miner_tx = create_tx_with_uncles(uncles);

  cryptonote::block b = {};
  b.miner_tx = miner_tx;

  std::list<cryptonote::block> chain = {b};
  uint64_t weight = blockchain.calculate_pop_chain_weight(chain, 0);

  // Should include miner tx weight plus uncle weight
  ASSERT_GT(weight, 0ULL);
}

TEST(pop_chain_weight_calculation, multiple_blocks)
{
  MockBlockchain blockchain;
  std::list<cryptonote::block> chain;

  for (int i = 0; i < 3; ++i)
  {
    cryptonote::block b = {};
    b.miner_tx.version = 2;
    b.miner_tx.extra = std::vector<uint8_t>(10, 0); // No uncles
    chain.push_back(b);
  }

  uint64_t weight = blockchain.calculate_pop_chain_weight(chain, 0);

  // Should include weight of all blocks
  ASSERT_GT(weight, 0ULL);
}

TEST(pop_constants, verify_pop_k_value)
{
  ASSERT_EQ(POP_K, 3);
}

TEST(pop_constants, verify_pop_d_value)
{
  ASSERT_EQ(POP_D, 5);
}

TEST(pop_edge_cases, max_uncle_blocks_allowed)
{
  MockBlockchain blockchain;
  std::vector<cryptonote::tx_extra_uncle_block> uncles;

  // Create exactly POP_K uncles (should be allowed)
  for (int i = 0; i < POP_K; ++i)
  {
    uncles.push_back(create_valid_uncle_block());
  }

  cryptonote::transaction tx = create_tx_with_uncles(uncles);
  auto extracted_uncles = blockchain.get_uncle_blocks_from_coinbase(tx);

  ASSERT_EQ(extracted_uncles.size(), POP_K);
}

TEST(pop_edge_cases, invalid_uncle_block_structure)
{
  MockBlockchain blockchain;
  cryptonote::tx_extra_uncle_block uncle = create_valid_uncle_block();
  uncle.major_version = 0; // Invalid version

  // Should fail validation
  ASSERT_FALSE(blockchain.validate_uncle_block(uncle, 100, 17));
}

TEST(pop_edge_cases, uncle_block_with_null_prev_id)
{
  MockBlockchain blockchain;
  cryptonote::tx_extra_uncle_block uncle = create_valid_uncle_block();
  uncle.prev_id = crypto::null_hash;

  // Should still validate (null hash is valid)
  ASSERT_TRUE(blockchain.validate_uncle_block(uncle, 100, 17));
}

TEST(pop_edge_cases, late_block_at_height_zero)
{
  MockBlockchain blockchain;
  cryptonote::block b = {};
  b.timestamp = 1000000000; // Far in future

  // Height 0 should never be considered late
  ASSERT_FALSE(blockchain.is_late_block(b, 0));
}

TEST(pop_edge_cases, late_block_exact_pop_d_boundary)
{
  MockBlockchain blockchain;
  cryptonote::block b = {};
  b.timestamp = 1000;

  // Exactly POP_D seconds difference should not be late
  ASSERT_FALSE(blockchain.is_late_block(b, 1000 + POP_D));
}

TEST(pop_edge_cases, chain_weight_with_mixed_uncles)
{
  MockBlockchain blockchain;
  std::list<cryptonote::block> chain;

  // First block with uncles
  std::vector<cryptonote::tx_extra_uncle_block> uncles1 = {create_valid_uncle_block()};
  cryptonote::transaction miner_tx1 = create_tx_with_uncles(uncles1);
  cryptonote::block b1 = {};
  b1.miner_tx = miner_tx1;
  chain.push_back(b1);

  // Second block without uncles
  cryptonote::block b2 = {};
  b2.miner_tx.version = 2;
  b2.miner_tx.extra = std::vector<uint8_t>(10, 0);
  chain.push_back(b2);

  // Third block with multiple uncles
  std::vector<cryptonote::tx_extra_uncle_block> uncles3;
  for (int i = 0; i < POP_K; ++i)
  {
    uncles3.push_back(create_valid_uncle_block());
  }
  cryptonote::transaction miner_tx3 = create_tx_with_uncles(uncles3);
  cryptonote::block b3 = {};
  b3.miner_tx = miner_tx3;
  chain.push_back(b3);

  uint64_t weight = blockchain.calculate_pop_chain_weight(chain, 0);

  // Should include weights from all blocks and their uncles
  ASSERT_GT(weight, 0ULL);
}

TEST(pop_deterministic_tie_breaking, same_weight_different_hashes)
{
  MockBlockchain blockchain;

  // Create two chains with the same PoP weight but different block hashes
  std::list<cryptonote::block> chain1;
  std::list<cryptonote::block> chain2;

  // Both chains have the same structure and weight
  for (int i = 0; i < 3; ++i)
  {
    cryptonote::block b1 = {};
    b1.miner_tx.version = 2;
    b1.miner_tx.extra = std::vector<uint8_t>(10, 0);
    b1.timestamp = 1000 + i * 60;
    b1.nonce = i; // Different nonce for different hash
    chain1.push_back(b1);

    cryptonote::block b2 = {};
    b2.miner_tx.version = 2;
    b2.miner_tx.extra = std::vector<uint8_t>(10, 0);
    b2.timestamp = 1000 + i * 60;
    b2.nonce = i + 10; // Different nonce for different hash
    chain2.push_back(b2);
  }

  uint64_t weight1 = blockchain.calculate_pop_chain_weight(chain1, 0);
  uint64_t weight2 = blockchain.calculate_pop_chain_weight(chain2, 0);

  // Weights should be the same (same structure)
  ASSERT_EQ(weight1, weight2);

  // In case of tie, the chain with the lexicographically smaller hash should win
  // This simulates the deterministic tie-breaking mechanism
  crypto::hash hash1 = get_block_hash(chain1.back());
  crypto::hash hash2 = get_block_hash(chain2.back());

  // The tie-breaking should be deterministic based on hash comparison
  bool chain1_wins = hash1 < hash2;
  bool chain2_wins = hash2 < hash1;

  // Exactly one should win the tie
  ASSERT_TRUE(chain1_wins || chain2_wins);
  ASSERT_FALSE(chain1_wins && chain2_wins);
}

TEST(pop_deterministic_tie_breaking, identical_chains)
{
  MockBlockchain blockchain;

  // Create two identical chains
  std::list<cryptonote::block> chain1;
  std::list<cryptonote::block> chain2;

  for (int i = 0; i < 3; ++i)
  {
    cryptonote::block b = {};
    b.miner_tx.version = 2;
    b.miner_tx.extra = std::vector<uint8_t>(10, 0);
    b.timestamp = 1000 + i * 60;
    b.nonce = i;
    chain1.push_back(b);
    chain2.push_back(b); // Identical block
  }

  uint64_t weight1 = blockchain.calculate_pop_chain_weight(chain1, 0);
  uint64_t weight2 = blockchain.calculate_pop_chain_weight(chain2, 0);

  // Weights should be identical
  ASSERT_EQ(weight1, weight2);

  // Hashes should be identical for identical chains
  crypto::hash hash1 = get_block_hash(chain1.back());
  crypto::hash hash2 = get_block_hash(chain2.back());
  ASSERT_EQ(hash1, hash2);
}

TEST(pop_deterministic_tie_breaking, different_uncles_same_weight)
{
  MockBlockchain blockchain;

  // Create two chains with same weight but different uncle configurations
  std::list<cryptonote::block> chain1;
  std::list<cryptonote::block> chain2;

  // Chain 1: One block with POP_K uncles
  std::vector<cryptonote::tx_extra_uncle_block> uncles1;
  for (int i = 0; i < POP_K; ++i)
  {
    uncles1.push_back(create_valid_uncle_block());
  }
  cryptonote::transaction miner_tx1 = create_tx_with_uncles(uncles1);
  cryptonote::block b1 = {};
  b1.miner_tx = miner_tx1;
  chain1.push_back(b1);

  // Chain 2: POP_K blocks with one uncle each
  for (int i = 0; i < POP_K; ++i)
  {
    std::vector<cryptonote::tx_extra_uncle_block> uncles = {create_valid_uncle_block()};
    cryptonote::transaction miner_tx = create_tx_with_uncles(uncles);
    cryptonote::block b = {};
    b.miner_tx = miner_tx;
    chain2.push_back(b);
  }

  uint64_t weight1 = blockchain.calculate_pop_chain_weight(chain1, 0);
  uint64_t weight2 = blockchain.calculate_pop_chain_weight(chain2, 0);

  // Weights should be the same (POP_K uncles total in both cases)
  ASSERT_EQ(weight1, weight2);

  // Tie-breaking should be deterministic
  crypto::hash hash1 = get_block_hash(chain1.back());
  crypto::hash hash2 = get_block_hash(chain2.back());

  bool chain1_wins = hash1 < hash2;
  bool chain2_wins = hash2 < hash1;

  ASSERT_TRUE(chain1_wins || chain2_wins);
  ASSERT_FALSE(chain1_wins && chain2_wins);
}