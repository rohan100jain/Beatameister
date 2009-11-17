
CXX=g++
C=gcc
INCLUDES=
RTAUDIOPATH=./
FLAGS=-D__MACOSX_CORE__ -c
LIBS=-framework CoreAudio -framework CoreMIDI -framework CoreFoundation \
	-framework IOKit -framework Carbon  -framework OpenGL \
	-framework GLUT -framework Foundation \
	-framework AppKit -lstdc++ \
	-L/usr/local/lib/ \
	libxtract.0.dylib -lm -lfftw3f

OBJS=   RtAudio.o chuck_fft.o Beatameister.o lpc.o Thread.o Stk.o midiio_osx.o

Beatameister: $(OBJS)
	$(CXX) -o Beatameister $(OBJS) $(LIBS)

Beatameister.o: Beatameister.cpp $(RTAUDIOPATH)RtAudio.h
	$(CXX) $(FLAGS) Beatameister.cpp 

RtAudio.o: $(RTAUDIOPATH)RtAudio.h $(RTAUDIOPATH)RtAudio.cpp $(RTAUDIOPATH)RtError.h
	$(CXX) $(FLAGS) $(RTAUDIOPATH)RtAudio.cpp

chuck_fft.o: chuck_fft.c
	$(C) $(FLAGS) chuck_fft.c

lpc.o: lpc.cpp 
	$(CXX) $(FLAGS) lpc.cpp

Thread.o: Thread.cpp 
	$(CXX) $(FLAGS) Thread.cpp

clean:
	rm -f *~ *# *.o Beatameister
