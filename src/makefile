
CXX=g++
C=gcc
INCLUDES=-Ioscpack/ -I./stk-4.4.1/include/ -I./stk-4.4.1/src/include/
RTAUDIOPATH=./
FLAGS=-D__MACOSX_CORE__  $(INCLUDES) -c
LIBS=-framework CoreAudio -framework CoreMIDI -framework CoreFoundation \
	-framework IOKit -framework Carbon  -framework OpenGL \
	-framework GLUT -framework Foundation \
	-framework AppKit -lstdc++ \
	-L/opt/local/lib/ \
	/usr/local/lib/libxtract.0.dylib -lm -lfftw3f

OSCPACK_DIR=oscpack/
IP_DIR=$(OSCPACK_DIR)ip/
OSC_DIR=$(OSCPACK_DIR)osc/
OSCPACK_OBJS=$(OSC_DIR)OscOutboundPacketStream.o $(OSC_DIR)OscTypes.o \
	    $(OSC_DIR)OscReceivedElements.o $(OSC_DIR)OscPrintReceivedElements.o \
		$(IP_DIR)posix/NetworkingUtils.o $(IP_DIR)posix/UdpSocket.o \
		$(IP_DIR)IpEndpointName.o

STKRELEASE_DIR = stk-4.4.1/src/Release/
STK_OBJS = $(STKRELEASE_DIR)Stk.o $(STKRELEASE_DIR)Mutex.o $(STKRELEASE_DIR)Drummer.o $(STKRELEASE_DIR)FileWvin.o $(STKRELEASE_DIR)OnePole.o $(STKRELEASE_DIR)FileRead.o $(STKRELEASE_DIR)Voicer.o  $(STKRELEASE_DIR)Flute.o  $(STKRELEASE_DIR)DelayL.o  $(STKRELEASE_DIR)ADSR.o  $(STKRELEASE_DIR)PoleZero.o  $(STKRELEASE_DIR)SineWave.o $(STKRELEASE_DIR)Noise.o 

OBJS =   RtAudio.o chuck_fft.o Beatameister.o lpc.o Thread.o Stk.o midiio_osx.o
OBJS2 = SynthesisEngine.o RtAudio.o
all:Beatameister SynthesisEngine

Beatameister: $(OBJS)
	$(CXX) -o Beatameister $(OBJS) $(STK_OBJS) $(OSCPACK_OBJS) $(LIBS)

SynthesisEngine: $(OBJS2)
	$(CXX) -o SynthesisEngine $(OBJS2) $(STK_OBJS) $(OSCPACK_OBJS) $(LIBS)

Beatameister.o: Beatameister.cpp $(RTAUDIOPATH)RtAudio.h
	$(CXX) $(FLAGS) Beatameister.cpp 

SynthesisEngine.o: SynthesisEngine.cpp $(RTAUDIOPATH)RtAudio.h
	$(CXX) $(FLAGS) SynthesisEngine.cpp 

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
