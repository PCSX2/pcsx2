/*
 *	Copyright (C) 2015-2015 Gregory hainaut
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSPng.h"

namespace GSPng {

    void Save(GSPng::Format fmt, const string& file, char* image, int w, int h, int pitch)
    {
#ifdef ENABLE_OGL_PNG
        std::string root = file;
        root.replace(file.length()-4, 4, "");

        uint8* data = (uint8*)image;

        switch (fmt) {
            case R8I_PNG:
                {
                    png::image<png::gray_pixel> img(w, h);
                    for(int y = 0; y < h; y++, data += pitch) {
                        for (int x = 0; x < w; x++) {
                            img[y][x] = png::gray_pixel(data[x]);
                        }
                    }
                    img.write(root + "_R8.png");
                }
                break;

            case R16I_PNG:
                {
                    png::image<png::gray_pixel_16> img(w, h);
                    for(int y = 0; y < h; y++, data += pitch) {
                        for (int x = 0; x < w; x++) {
                            img[y][x] = png::gray_pixel_16(data[2*x]);
                        }
                    }
                    img.write(root + "_R16.png");
                }
                break;

            case R32I_PNG:
                {
                    png::image<png::gray_pixel_16> img_msb(w, h);
                    png::image<png::gray_pixel_16> img_lsb(w, h);
                    for(int y = 0; y < h; y++, data += pitch) {
                        for (int x = 0; x < w; x++) {
                            img_msb[y][x] = png::gray_pixel_16(data[2*x]);
                            img_lsb[y][x] = png::gray_pixel_16(data[2*x+2]);
                        }
                    }
                    img_msb.write(root + "_R32I_msb.png");
                    img_lsb.write(root + "_R32I_lsb.png");
                }
                break;

            case DEPTH_PNG:
                {
                    png::image<png::gray_pixel_16> img_msb(w, h);
                    png::image<png::gray_pixel_16> img_lsb(w, h);
                    for(int y = 0; y < h; y++, data += pitch) {
                        for (int x = 0; x < w; x++) {
                            // TODO packed or not
                            uint32 depth = data[4*x]; //floorf((float)data[2*x] * exp2f(32));

                            png::gray_pixel_16 msb(depth >> 16);
                            png::gray_pixel_16 lsb((depth >> 16) ? 0xFFFF : depth & 0xFFFF);

                            img_msb[y][x] = msb;
                            img_lsb[y][x] = lsb;
                        }
                    }
                    img_msb.write(root + "_msb.png");
                    img_lsb.write(root + "_lsb.png");
                }
                break;

            case ALPHA_PNG:
                {
                    png::image<png::gray_pixel> img_alpha(w, h);
                    for(int y = 0; y < h; y++, data += pitch) {
                        for (int x = 0; x < w; x++) {
                            img_alpha[y][x] = png::gray_pixel(data[4*x+3]);
                        }
                    }
                    img_alpha.write(root + "_alpha.png");
                }
                break;

            case RGB_PNG:
                {
                    png::image<png::rgb_pixel>  img_opaque(w, h);
                    for(int y = 0; y < h; y++, data += pitch) {
                        for (int x = 0; x < w; x++) {
                            img_opaque[y][x] = png::rgb_pixel(data[4*x+0], data[4*x+1], data[4*x+2]);
                        }
                    }
                    img_opaque.write(root + ".png");
                }
                break;

            case RGBA_PNG:
                {
                    png::image<png::rgba_pixel>  img(w, h);
                    for(int y = 0; y < h; y++, data += pitch) {
                        for (int x = 0; x < w; x++) {
                            img[y][x] = png::rgba_pixel(data[4*x+0], data[4*x+1], data[4*x+2], data[4*x+3]);
                        }
                    }
                    img.write(root + "_full.png");
                }
                break;

            case RGB_A_PNG:
                {
                    png::image<png::rgb_pixel>  img_opaque(w, h);
                    png::image<png::gray_pixel> img_alpha(w, h);
                    for(int y = 0; y < h; y++, data += pitch) {
                        for (int x = 0; x < w; x++) {
                            img_opaque[y][x] = png::rgb_pixel(data[4*x+0], data[4*x+1], data[4*x+2]);
                            img_alpha[y][x]  = png::gray_pixel(data[4*x+3]);
                        }
                    }
                    img_opaque.write(root + ".png");
                    img_alpha.write(root + "_alpha.png");
                }
                break;

            default:
                ASSERT(0);
        }
#endif
    }

    Transaction::Transaction(GSPng::Format fmt, const string& file, char* image, int w, int h, int pitch)
        : m_fmt(fmt), m_file(file), m_w(w), m_h(h), m_pitch(pitch)
    {
        // Note: yes it would be better to use shared pointer
        m_image = (char*)_aligned_malloc(pitch*h, 32);
        if (m_image)
            memcpy(m_image, image, pitch*h);
    }

    Transaction::~Transaction()
    {
        if (m_image)
            _aligned_free(m_image);
    }

    void Worker::Process(shared_ptr<Transaction>& item)
    {
        Save(item->m_fmt, item->m_file, item->m_image, item->m_w, item->m_h, item->m_pitch);
    }

}
