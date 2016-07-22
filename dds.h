#pragma once

#define FOUR_ZEROES 0, 0, 0, 0
static const char DDS_HEADER[DDS_HEADER_SIZE] = {

  'D', 'D', 'S', ' ', // magic number
  124, 0, 0, 0, // header proper size
  7, 16, 0, 0, // flags
  0, 2, 0, 0,

  0, 2, 0, 0, // dimensions
  FOUR_ZEROES, // pitch
  FOUR_ZEROES, // depth
  FOUR_ZEROES, // mipmap count

  FOUR_ZEROES, // 44 unused bytes
  FOUR_ZEROES,
  FOUR_ZEROES,
  FOUR_ZEROES,

  FOUR_ZEROES,
  FOUR_ZEROES,
  FOUR_ZEROES,
  FOUR_ZEROES,

  FOUR_ZEROES,
  FOUR_ZEROES,
  FOUR_ZEROES,
  32, 0, 0, 0, // pixel format - size

  2, 0, 0, 0, // flags - DDPF_ALPHA, apparently the original file uses this
  FOUR_ZEROES, // FourCC
  8, 0, 0, 0, // 8 bits per channel
  FOUR_ZEROES, // red maxk

  FOUR_ZEROES, // green mask
  FOUR_ZEROES, // blue mask
  255, 0, 0, 0, // alpha mask - end of pixel format
  2, 16, 0, 0, // something weird; 0x1000 is required but I'm not sure about 0x2

  FOUR_ZEROES, // this is a simple texture so we don't need to do anything fancy here
  FOUR_ZEROES,
  FOUR_ZEROES,
  FOUR_ZEROES, // unused fields
};
