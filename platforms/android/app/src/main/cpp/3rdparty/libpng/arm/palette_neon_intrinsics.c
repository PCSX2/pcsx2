/* palette_neon_intrinsics.c - NEON optimised palette expansion functions
 *
 * Copyright (c) 2018-2026 Cosmin Truta
 * Copyright (c) 2017-2018 Arm Holdings. All rights reserved.
 * Written by Richard Townsend <Richard.Townsend@arm.com>, February 2017.
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */

/* Build an RGBA8 palette from the separate RGB and alpha palettes. */
static void
png_riffle_palette_neon(png_byte *riffled_palette, const png_color *palette,
    const png_byte *trans_alpha, int num_trans)
{
   int i;

   /* Initially black, opaque. */
   uint8x16x4_t w = {{
      vdupq_n_u8(0x00),
      vdupq_n_u8(0x00),
      vdupq_n_u8(0x00),
      vdupq_n_u8(0xff),
   }};

   png_debug(1, "in png_riffle_palette_neon");

   /* First, riffle the RGB colours into an RGBA8 palette.
    * The alpha component is set to opaque for now.
    */
   for (i = 0; i < 256; i += 16)
   {
      uint8x16x3_t v = vld3q_u8((const png_byte *)(palette + i));
      w.val[0] = v.val[0];
      w.val[1] = v.val[1];
      w.val[2] = v.val[2];
      vst4q_u8(riffled_palette + i * 4, w);
   }

   /* Fix up the missing transparency values. */
   for (i = 0; i < num_trans; i++)
      riffled_palette[i * 4 + 3] = trans_alpha[i];
}

/* Expands a palettized row into RGBA8. */
static png_uint_32
png_target_do_expand_palette_rgba8_neon(const png_uint_32 *riffled_palette,
    png_uint_32 row_width, const png_byte **ssp, png_byte **ddp)
{
   const png_uint_32 pixels_per_chunk = 4;
   png_uint_32 i;

   png_debug(1, "in png_do_expand_palette_rgba8_neon");

   if (row_width < pixels_per_chunk)
      return 0;

   /* This function originally gets the last byte of the output row.
    * The NEON part writes forward from a given position, so we have
    * to seek this back by 4 pixels x 4 bytes.
    */
   *ddp = *ddp - (pixels_per_chunk * 4 - 1);

   for (i = 0; i + pixels_per_chunk <= row_width; i += pixels_per_chunk)
   {
      uint32x4_t cur;
      const png_byte *sp = *ssp - i;
      png_byte *dp = *ddp - i * 4;
      cur = vld1q_dup_u32 (riffled_palette + *(sp - 3));
      cur = vld1q_lane_u32(riffled_palette + *(sp - 2), cur, 1);
      cur = vld1q_lane_u32(riffled_palette + *(sp - 1), cur, 2);
      cur = vld1q_lane_u32(riffled_palette + *(sp - 0), cur, 3);
      vst1q_u32((void *)dp, cur);
   }

   /* Undo the pre-adjustment of *ddp before the pointer handoff,
    * so the scalar fallback in pngrtran.c receives a dp that points
    * to the correct position.
    */
   *ddp = *ddp + (pixels_per_chunk * 4 - 1);
   *ssp = *ssp - i;
   *ddp = *ddp - i * 4;
   return i;
}

/* Expands a palettized row into RGB8. */
static png_uint_32
png_target_do_expand_palette_rgb8_neon(const png_color *paletteIn,
    png_uint_32 row_width, const png_byte **ssp, png_byte **ddp)
{
   /* TODO: This case is VERY dangerous: */
   const png_byte *palette = (const png_byte *)paletteIn;

   const png_uint_32 pixels_per_chunk = 8;
   png_uint_32 i;

   png_debug(1, "in png_do_expand_palette_rgb8_neon");

   if (row_width <= pixels_per_chunk)
      return 0;

   /* Seeking this back by 8 pixels x 3 bytes. */
   *ddp = *ddp - (pixels_per_chunk * 3 - 1);

   for (i = 0; i + pixels_per_chunk <= row_width; i += pixels_per_chunk)
   {
      uint8x8x3_t cur;
      const png_byte *sp = *ssp - i;
      const png_byte *dp = *ddp - i * 3;
      cur = vld3_dup_u8(palette + *(sp - 7) * 3);
      cur = vld3_lane_u8(palette + *(sp - 6) * 3, cur, 1);
      cur = vld3_lane_u8(palette + *(sp - 5) * 3, cur, 2);
      cur = vld3_lane_u8(palette + *(sp - 4) * 3, cur, 3);
      cur = vld3_lane_u8(palette + *(sp - 3) * 3, cur, 4);
      cur = vld3_lane_u8(palette + *(sp - 2) * 3, cur, 5);
      cur = vld3_lane_u8(palette + *(sp - 1) * 3, cur, 6);
      cur = vld3_lane_u8(palette + *(sp - 0) * 3, cur, 7);
      vst3_u8((void *)dp, cur);
   }

   /* Undo the pre-adjustment of *ddp before the pointer handoff,
    * so the scalar fallback in pngrtran.c receives a dp that points
    * to the correct position.
    */
   *ddp = *ddp + (pixels_per_chunk * 3 - 1);
   *ssp = *ssp - i;
   *ddp = *ddp - i * 3;
   return i;
}
