//
// Created by Haifa Bogdan Adnan on 06/08/2018.
//

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

//#include "argon2.h"

#include "../../argon2/defs.h"

#if defined(__x86_64__) && !defined(BUILD_REF)
#include "blamka-round-opt.h"
#else
#include "blamka-round-ref.h"
#endif

#ifndef BUILD_REF

#if defined(__AVX512F__)
static void fill_block(__m512i *state, const block *ref_block,
                       block *next_block) {
    __m512i block_XY[ARGON2_512BIT_WORDS_IN_BLOCK];
    unsigned int i;

    for (i = 0; i < ARGON2_512BIT_WORDS_IN_BLOCK; i++) {
        block_XY[i] = state[i] = _mm512_xor_si512(
            state[i], _mm512_loadu_si512((const __m512i *)ref_block->v + i));
    }

    for (i = 0; i < 2; ++i) {
        BLAKE2_ROUND_1(
            state[8 * i + 0], state[8 * i + 1], state[8 * i + 2], state[8 * i + 3],
            state[8 * i + 4], state[8 * i + 5], state[8 * i + 6], state[8 * i + 7]);
    }

    for (i = 0; i < 2; ++i) {
        BLAKE2_ROUND_2(
            state[2 * 0 + i], state[2 * 1 + i], state[2 * 2 + i], state[2 * 3 + i],
            state[2 * 4 + i], state[2 * 5 + i], state[2 * 6 + i], state[2 * 7 + i]);
    }

    if(next_block != NULL) {
        for (i = 0; i < ARGON2_512BIT_WORDS_IN_BLOCK; i++) {
            state[i] = _mm512_xor_si512(state[i], block_XY[i]);
            _mm512_storeu_si512((__m512i *)next_block->v + i, state[i]);
        }
    }
    else {
        for (i = 0; i < ARGON2_512BIT_WORDS_IN_BLOCK; i++) {
            state[i] = _mm512_xor_si512(state[i], block_XY[i]);
        }
    }
}
#elif defined(__AVX2__)
static void fill_block(__m256i *state, const block *ref_block,
                       block *next_block) {
    __m256i block_XY[ARGON2_HWORDS_IN_BLOCK];
    unsigned int i;

    for (i = 0; i < ARGON2_HWORDS_IN_BLOCK; i++) {
        block_XY[i] = state[i] = _mm256_xor_si256(
                state[i], _mm256_load_si256((const __m256i *)ref_block->v + i));
    }

    for (i = 0; i < 4; ++i) {
        BLAKE2_ROUND_1(state[8 * i + 0], state[8 * i + 4], state[8 * i + 1], state[8 * i + 5],
                       state[8 * i + 2], state[8 * i + 6], state[8 * i + 3], state[8 * i + 7]);
    }

    for (i = 0; i < 4; ++i) {
        BLAKE2_ROUND_2(state[ 0 + i], state[ 4 + i], state[ 8 + i], state[12 + i],
                       state[16 + i], state[20 + i], state[24 + i], state[28 + i]);
    }

    if(next_block != NULL) {
        for (i = 0; i < ARGON2_HWORDS_IN_BLOCK; i++) {
            state[i] = _mm256_xor_si256(state[i], block_XY[i]);
            _mm256_store_si256((__m256i *)next_block->v + i, state[i]);
        }
    }
    else {
        for (i = 0; i < ARGON2_HWORDS_IN_BLOCK; i++) {
            state[i] = _mm256_xor_si256(state[i], block_XY[i]);
        }
    }
}
#elif defined(__x86_64__)
static void fill_block(__m128i *state, const block *ref_block,
                       block *next_block) {
    __m128i block_XY[ARGON2_OWORDS_IN_BLOCK];
    unsigned int i;

    for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++) {
        block_XY[i] = state[i] = _mm_xor_si128(
            state[i], _mm_loadu_si128((const __m128i *)ref_block->v + i));
    }

    for (i = 0; i < 8; ++i) {
        BLAKE2_ROUND(state[8 * i + 0], state[8 * i + 1], state[8 * i + 2],
            state[8 * i + 3], state[8 * i + 4], state[8 * i + 5],
            state[8 * i + 6], state[8 * i + 7]);
    }

    for (i = 0; i < 8; ++i) {
        BLAKE2_ROUND(state[8 * 0 + i], state[8 * 1 + i], state[8 * 2 + i],
            state[8 * 3 + i], state[8 * 4 + i], state[8 * 5 + i],
            state[8 * 6 + i], state[8 * 7 + i]);
    }

    if(next_block != NULL) {
        for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++) {
            state[i] = _mm_xor_si128(state[i], block_XY[i]);
            _mm_storeu_si128((__m128i *)next_block->v + i, state[i]);
        }
    }
    else {
        for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++) {
            state[i] = _mm_xor_si128(state[i], block_XY[i]);
        }
    }
}
#endif
#else
void copy_block(block *dst, const block *src) {
    memcpy(dst->v, src->v, sizeof(uint64_t) * ARGON2_QWORDS_IN_BLOCK);
}

void xor_block(block *dst, const block *src) {
    int i;
    for (i = 0; i < ARGON2_QWORDS_IN_BLOCK; ++i) {
        dst->v[i] ^= src->v[i];
    }
}

static void fill_block(block *prev_block, const block *ref_block,
                       block *next_block) {
    block block_tmp;
    unsigned i;

    xor_block(prev_block, ref_block);
    copy_block(&block_tmp, prev_block);

    /* Apply Blake2 on columns of 64-bit words: (0,1,...,15) , then
     (16,17,..31)... finally (112,113,...127) */
    for (i = 0; i < 8; ++i) {
        BLAKE2_ROUND_NOMSG(
                           prev_block->v[16 * i], prev_block->v[16 * i + 1], prev_block->v[16 * i + 2],
                           prev_block->v[16 * i + 3], prev_block->v[16 * i + 4], prev_block->v[16 * i + 5],
                           prev_block->v[16 * i + 6], prev_block->v[16 * i + 7], prev_block->v[16 * i + 8],
                           prev_block->v[16 * i + 9], prev_block->v[16 * i + 10], prev_block->v[16 * i + 11],
                           prev_block->v[16 * i + 12], prev_block->v[16 * i + 13], prev_block->v[16 * i + 14],
                           prev_block->v[16 * i + 15]);
    }

    /* Apply Blake2 on rows of 64-bit words: (0,1,16,17,...112,113), then
     (2,3,18,19,...,114,115).. finally (14,15,30,31,...,126,127) */
    for (i = 0; i < 8; i++) {
        BLAKE2_ROUND_NOMSG(
                           prev_block->v[2 * i], prev_block->v[2 * i + 1], prev_block->v[2 * i + 16],
                           prev_block->v[2 * i + 17], prev_block->v[2 * i + 32], prev_block->v[2 * i + 33],
                           prev_block->v[2 * i + 48], prev_block->v[2 * i + 49], prev_block->v[2 * i + 64],
                           prev_block->v[2 * i + 65], prev_block->v[2 * i + 80], prev_block->v[2 * i + 81],
                           prev_block->v[2 * i + 96], prev_block->v[2 * i + 97], prev_block->v[2 * i + 112],
                           prev_block->v[2 * i + 113]);
    }

    xor_block(prev_block, &block_tmp);
    if(next_block != NULL)
        copy_block(next_block, prev_block);
}

#endif

void fill_memory_blocks(void *memory, int threads, void *user_data) {
    block *ref_block = NULL, *curr_block = NULL;
    uint32_t curr_offset, ref_offset;
    uint32_t starting_index = 2, i;
#ifndef  BUILD_REF
#if defined(__AVX512F__)
    __m512i state[ARGON2_512BIT_WORDS_IN_BLOCK];
#elif defined(__AVX2__)
    __m256i state[ARGON2_HWORDS_IN_BLOCK];
#elif defined(__x86_64__)
    __m128i state[ARGON2_OWORDS_IN_BLOCK];
#endif
#else
    block state_;
    block *state = &state_;
#endif

    block *blocks = memory;

    memcpy(state, (void *)(blocks + 1), ARGON2_BLOCK_SIZE);

    int32_t *address = blocks_addresses + (starting_index * 2);

    for (i = starting_index; i < ARGON2_MEMORY_BLOCKS; ++i, address += 2) {
        ref_block = blocks + address[1];
        curr_block = (address[0] == -1) ? NULL : (blocks + address[0]);

        fill_block(state, ref_block, curr_block);
    }
}

