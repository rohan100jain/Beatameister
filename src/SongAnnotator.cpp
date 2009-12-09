#include <iostream>

#include "RtAudio.h"
#include "FileWvIn.h";

#define MY_FREQ 44100
#define MY_PIE 3.14159265358979
#define SAMPLE double

#ifndef _STK_DEBUG_
#define _STK_DEBUG_
#endif

using namespace std;
using namespace stk;

RtAudio *g_dac = 0;

// file in
FileWvIn g_fin;

// callback function
int callback_func( void *output_buffer, void *input_buffer, unsigned int nFrames, double streamTime, RtAudioStreamStatus
 status, void *user_data ) {
	SAMPLE * new_buffer = (SAMPLE *)output_buffer;
	// zero it out
	memset( new_buffer, 0, nFrames * sizeof(SAMPLE));
	// add it to accumulate
	for(int j=0;j<nFrames;j++) {
		new_buffer[j] = g_fin.tick();
	}
	return 0;
}

int main(int argc, char* argv[])
{
	// Set the global sample rate before creating class instances.
  	Stk::setSampleRate( 44100 );
  	Stk::showWarnings( true );
  
	// Get RtAudio Instance with default API
	g_dac = new RtAudio();
    // buffer size
    unsigned int buffer_size = 512;
	// Output Stream Parameters
	RtAudio::StreamParameters outputStreamParams;
	outputStreamParams.deviceId = g_dac->getDefaultOutputDevice();
	outputStreamParams.nChannels = 1;
	
	// Get RtAudio Stream
	try {
		g_dac->openStream(
			&outputStreamParams,
			NULL,
			RTAUDIO_FLOAT64,
			MY_FREQ,
			&buffer_size,
			callback_func,
			NULL
			);
	}
	catch(RtError &err) {
		err.printMessage();
		exit(1);
	}
	
	try 
    {
        // read the file
        g_fin.openFile( "song.wav" );
        // change the rate
        g_fin.setRate( 1 );
		// normalize the peak
		g_fin.normalize();
    } catch( StkError & e )
    {
        cerr << "baaaaaaaaad..." << endl;
        return 1;
    }
	
	g_dac->startStream();
	
	// idle wait
	while( true ) {
		usleep( 10000 );
	}
	
	
 	// if we get here, then stop!
	try{
		g_dac->stopStream();
	} 
	catch( RtError & err ) {
		// do stuff
		err.printMessage();
	}

	cleanup:
	    g_dac->closeStream();
	  	delete g_dac;
	

    
	return 0;
}


