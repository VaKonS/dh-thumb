# Adjust these paths for your Posix version of MinGW.
W2XPOSIXMINGW=/usr/lib/gcc/i686-w64-mingw32/6.2-posix
W2XCOMMONMINGW=/usr/i686-w64-mingw32
CC=i686-w64-mingw32-g++-posix
STRIP=i686-w64-mingw32-strip
WINDRES=i686-w64-mingw32-windres
CXX_FLAGS= -static-libstdc++ -static-libgcc -static -pthread -fopenmp -march=i686 -Wall -std=c++11 -mno-avx2 -mno-avx -mno-sse4 -mno-ssse3 -msse3 -mfpmath=sse -O2 -fomit-frame-pointer -finline-functions
CXX_INCLUDES = -I../include -I../opencv/opencv-build -I../opencv/opencv-master/include -I../opencv/opencv-master/modules/core/include -I../opencv/opencv-master/modules/calib3d/include -I../opencv/opencv-master/modules/features2d/include -I../opencv/opencv-master/modules/flann/include -I../opencv/opencv-master/modules/highgui/include -I../opencv/opencv-master/modules/imgcodecs/include -I../opencv/opencv-master/modules/videoio/include -I../opencv/opencv-master/modules/imgproc/include -I../opencv/opencv-master/modules/ml/include -I../opencv/opencv-master/modules/objdetect/include -I../opencv/opencv-master/modules/photo/include -I../opencv/opencv-master/modules/shape/include -I../opencv/opencv-master/modules/stitching/include -I../opencv/opencv-master/modules/superres/include -I../opencv/opencv-master/modules/video/include -I../opencv/opencv-master/modules/videostab/include -I$(W2XCOMMONMINGW)/include
LIBAFLAGS= -static-libstdc++ -static-libgcc -static ../opencv/opencv-build/lib/libopencv_world320.a ../opencv/opencv-build/3rdparty/lib/libIlmImf.a ../opencv/opencv-build/3rdparty/lib/liblibpng.a ../opencv/opencv-build/3rdparty/lib/liblibjasper.a ../opencv/opencv-build/3rdparty/lib/liblibwebp.a ../opencv/opencv-build/3rdparty/lib/liblibjpeg.a ../opencv/opencv-build/3rdparty/lib/liblibtiff.a ../opencv/opencv-build/3rdparty/lib/libzlib.a $(W2XCOMMONMINGW)/lib/libm.a $(W2XPOSIXMINGW)/libgomp.a $(W2XCOMMONMINGW)/lib/libpthread.a $(W2XCOMMONMINGW)/lib/libgdi32.a $(W2XCOMMONMINGW)/lib/libcomctl32.a $(W2XCOMMONMINGW)/lib/libcomdlg32.a $(W2XPOSIXMINGW)/libstdc++.a $(W2XPOSIXMINGW)/libgcc.a

all: rdr-static

rdr-static: rdr.o rdr_resource.o
	$(CC) -o $@.exe $^ $(LIBAFLAGS)
	$(STRIP) $@.exe

rdr_resource.o:
	$(WINDRES) --codepage=65001 "rdr.rc" -o "rdr_resource.o"

resources: rdr.o rdr_resource.o
	$(WINDRES) --codepage=65001 "rdr.rc" -o "rdr_resource.o"
	$(CC) -o rdr-static.exe $^ $(LIBAFLAGS)
	$(STRIP) rdr-static.exe

%.o: %.cpp
	$(CC) $(CXX_INCLUDES) -o $@ $(CXX_FLAGS) -c $+

clean:
	rm -f *.o rdr-static.exe
