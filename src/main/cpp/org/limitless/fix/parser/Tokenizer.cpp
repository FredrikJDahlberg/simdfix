//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include "org/limitless/fix/parser/Tokenizer.h"

namespace org::limitless::fix::parser {

const uint8x16_t Tokenizer::TagEnds = vdupq_n_u8(TAG_END);
const uint8x16_t Tokenizer::EndMask = vdupq_n_u8(0x01);
const uint8x16_t Tokenizer::Zeros = vdupq_n_u8(0x30);
const uint8x16_t Tokenizer::Nines = vdupq_n_u8(0x39);
const uint8x16_t Tokenizer::Invalid = vdupq_n_u8(0x00);

}