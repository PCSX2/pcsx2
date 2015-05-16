/*
 *	Copyright (C) 2011-2015 Gregory hainaut
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
                            png::gray_pixel p(data[x]);
                            img.set_pixel(x, y, p);
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
                            png::gray_pixel_16 p(data[2*x]);
                            img.set_pixel(x, y, p);
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
                            png::gray_pixel_16 msb(data[2*x]);
                            png::gray_pixel_16 lsb(data[2*x+2]);

                            img_msb.set_pixel(x, y, msb);
                            img_lsb.set_pixel(x, y, lsb);
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

                            img_msb.set_pixel(x, y, msb);
                            img_lsb.set_pixel(x, y, lsb);
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
                            png::gray_pixel pa(data[4*x+3]);
                            img_alpha.set_pixel(x, y, pa);
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
                            png::rgb_pixel po(data[4*x+0], data[4*x+1], data[4*x+2]);
                            img_opaque.set_pixel(x, y, po);
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
                            png::rgba_pixel p(data[4*x+0], data[4*x+1], data[4*x+2], data[4*x+3]);
                            img.set_pixel(x, y, p);
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
                            png::rgb_pixel po(data[4*x+0], data[4*x+1], data[4*x+2]);
                            img_opaque.set_pixel(x, y, po);

                            png::gray_pixel pa(data[4*x+3]);
                            img_alpha.set_pixel(x, y, pa);
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
}
