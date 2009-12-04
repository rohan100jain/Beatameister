/* 
    Example of two different ways to process received OSC messages using oscpack.
    Receives the messages from the SimpleSend.cpp example.
*/

#include <iostream>

#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"
#include "osc/OscOutboundPacketStream.h"
#include "RtAudio.h"
#include "Drummer.h"
#include "Voicer.h"
#include "FileWvIn.h";

#define ADDRESS "127.0.0.1"
#define PORT 8000
#define OUTPUT_BUFFER_SIZE 1024

#define MY_FREQ 44100
#define MY_PIE 3.14159265358979
#define SAMPLE double

#ifndef _STK_DEBUG_
#define _STK_DEBUG_
#endif

using namespace std;
using namespace stk;
RtAudio *g_dac = 0;

Voicer *g_voicer = NULL;

int g_instruments[4] = {36, 45, 38, 42};

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
		new_buffer[j] = g_voicer->tick() + g_fin.tick();
	}
	return 0;
}


// Main Class
class MainPacketListener : public osc::OscPacketListener {
protected:
	
    virtual void ProcessMessage( const osc::ReceivedMessage& m, 
				const IpEndpointName& remoteEndpoint )
    {
		std::cout<<"Got Message"<<std::endl;
	    try{
			if( strcmp( m.AddressPattern(), "/play" ) == 0 ){
				
               	osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
				int id = (arg++)->AsInt32();
				// get note
				StkFloat note = g_instruments[id];
				std::cout<<"Trying to strike! "<<note<<std::endl;
				g_voicer->noteOn( note, 127);
            }
			
		}catch( osc::Exception& e ){
	            // any parsing errors such as unexpected argument types, or 
	            // missing arguments get thrown as exceptions.
	            std::cout << "error while parsing message: "
	                << m.AddressPattern() << ": " << e.what() << "\n";
	     }
    
	}
	
};

int main(int argc, char* argv[])
{
	// Set the global sample rate before creating class instances.
  	Stk::setSampleRate( 44100.0 );
  	Stk::showWarnings( true );
  	Stk::setRawwavePath( "stk-4.4.1/rawwaves/" );
  
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
        g_fin.openFile( "TomVega.wav" );
        // change the rate
        g_fin.setRate( 1 );
		// normalize the peak
		g_fin.normalize();
    } catch( StkError & e )
    {
        cerr << "baaaaaaaaad..." << endl;
        return 1;
    }
	
	// Alocate Drum Voice
	g_voicer = new Voicer();
	
	// Initial Drummer
	Drummer *m_drummer = new Drummer();
	g_voicer->addInstrument(m_drummer);
	Drummer *m_drummer2 = new Drummer();
	g_voicer->addInstrument(m_drummer2);
	Drummer *m_drummer3 = new Drummer();
	g_voicer->addInstrument(m_drummer3);
	Drummer *m_drummer4 = new Drummer();
	g_voicer->addInstrument(m_drummer4);
	
	// Start Listening For Clients
	MainPacketListener listener;
	UdpListeningReceiveSocket s(
            IpEndpointName( IpEndpointName::ANY_ADDRESS, PORT ),
            &listener );

	std::cout << "press ctrl-c to end\n";
	    
	// Start Stream
	try {
        g_dac->startStream();
    } catch( RtError & err ) {
        // do stuff
        err.printMessage();
        goto cleanup;
    }
    
    s.RunUntilSigInt();

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


