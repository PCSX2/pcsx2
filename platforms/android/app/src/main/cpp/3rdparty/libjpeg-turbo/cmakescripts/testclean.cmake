file(GLOB FILES
  testout*
  test_bmp*
  *_GRAY_*.bmp
  *_GRAY_*.png
  *_GRAY_*.ppm
  *_GRAY_*.jpg
  *_GRAY.yuv
  *_420_*.bmp
  *_420_*.png
  *_420_*.ppm
  *_420_*.jpg
  *_420.yuv
  *_422_*.bmp
  *_422_*.png
  *_422_*.ppm
  *_422_*.jpg
  *_422.yuv
  *_444_*.bmp
  *_444_*.png
  *_444_*.ppm
  *_444_*.jpg
  *_444.yuv
  *_440_*.bmp
  *_440_*.png
  *_440_*.ppm
  *_440_*.jpg
  *_440.yuv
  *_411_*.bmp
  *_411_*.png
  *_411_*.ppm
  *_411_*.jpg
  *_411.yuv
  *_441_*.bmp
  *_441_*.png
  *_441_*.ppm
  *_441_*.jpg
  *_441.yuv
  *_410_*.bmp
  *_410_*.png
  *_410_*.ppm
  *_410_*.jpg
  *_410.yuv
  *_24_*.bmp
  *_24_*.png
  *_24_*.ppm
  *_24_*.jpg
  *_24.yuv
  *_LOSSL*S_*.bmp
  *_LOSSL*S_*.ppm
  *_LOSSL*S_*.jpg
  test/croptest.log
  test/indexedcolortest.log
  test/tjbenchtest*.log
  test/tjcomptest*.log
  test/tjdecomptest*.log
  test/tjtrantest*.log)

if(NOT FILES STREQUAL "")
  message(STATUS "Removing test files")
  file(REMOVE ${FILES})
else()
  message(STATUS "No files to remove")
endif()
