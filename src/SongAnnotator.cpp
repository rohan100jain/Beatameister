#include <iostream>
#include <termios.h>
#include "RtAudio.h"
#include "FileWvIn.h";
#include "Drummer.h"
#include <fstream>
#include <sys/time.h>
#include <map>
#define MY_FREQ 44100
#define MY_PIE 3.14159265358979
#define SAMPLE double

#ifndef _STK_DEBUG_
#define _STK_DEBUG_
#endif

using namespace std;
using namespace stk;

RtAudio *g_dac = 0;
Drummer *g_drummer = NULL;
int g_instruments[4] = {36, 45, 38, 42};
long g_time;


// file out
ofstream g_off("beats.txt");
// file in
FileWvIn g_fin;

map<long,int> g_beatMap;
map<long,int>::iterator g_mapIter;

// Get Character From Terminal
char getch(void) {
	char buf = 0;
	struct termios old = {0};
	if (tcgetattr(0, &old) < 0)
		perror("tcsetattr()");
	old.c_lflag &= ~ICANON;
	old.c_lflag &= ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSANOW, &old) < 0)
		perror("tcsetattr ICANON");
	if (read(0, &buf, 1) < 0)
		perror ("read()");
	old.c_lflag |= ICANON;
	old.c_lflag |= ECHO;
	if (tcsetattr(0, TCSADRAIN, &old) < 0)
		perror ("tcsetattr ~ICANON");
	return (buf);
}

// callback function
int callback_func( void *output_buffer, void *input_buffer, unsigned int nFrames, double streamTime, RtAudioStreamStatus
 status, void *user_data ) {
	static int count = 0;
	count++;
	if(count % 100 == 0) {
		timeval time;
		gettimeofday(&time, NULL);
		long t = (time.tv_sec * 1000) + (time.tv_usec / 1000);
		cout<<"Time: "<<(t - g_time)<<endl;
		count = 0;
	}
	SAMPLE * new_buffer = (SAMPLE *)output_buffer;
	// zero it out
	memset( new_buffer, 0, nFrames * sizeof(SAMPLE));
	// add it to accumulate
	if(g_beatMap.size() > 0) {
		timeval time;
		gettimeofday(&time, NULL);
		long t = (time.tv_sec * 1000) + (time.tv_usec / 1000);	
		long delta = t - g_time;
		if(g_mapIter != g_beatMap.end()) {
			if(g_mapIter->first <= delta) {
				g_drummer->noteOn(g_instruments[g_mapIter->second], 1);
				g_mapIter++;
			}
		}
	}
	for(int j=0;j<nFrames;j++) {
		new_buffer[j] =  g_drummer->tick() + g_fin.tick();
	}
	return 0;
}

// Take Keyboard Input
void takeInput() {
	cout<<"Taking input"<<endl;
	while(true) {
		char c = getch();
		if(c == 'q')
			break;
		else {
			timeval time;
			gettimeofday(&time, NULL);
			long t = (time.tv_sec * 1000) + (time.tv_usec / 1000);
			
			long delta = t - g_time;
			
			if(c == 'a') {
				cout<<delta<<"\tbass"<<endl;
				g_off<<delta<<"\tbass"<<endl;
				g_drummer->noteOn( g_instruments[0], 1.0);				
			}
			if(c == 's') {
				cout<<delta<<"\tmidtom"<<endl;
				g_off<<delta<<"\tmidtom"<<endl;
				g_drummer->noteOn( g_instruments[1], 1.0);				
			}
			if(c == 'k') {
				cout<<delta<<"\tsnare"<<endl;
				g_off<<delta<<"\tsnare"<<endl;
				g_drummer->noteOn( g_instruments[2], 1.0);				
			}
			if(c == 'l') {
				cout<<delta<<"\thihat"<<endl;
				g_off<<delta<<"\thihat"<<endl;
				g_drummer->noteOn( g_instruments[3], 1.0);				
			}
		}
	}
}

int main(int argc, char* argv[])
{
	if(argc > 1) {
		ifstream iff(argv[1]);
		string s,beatname;
		long t;
		int n;
		while(getline(iff, s) != NULL) {
			istringstream iss(s);
			iss>>t>>beatname;
			if(beatname == "bass")
				g_beatMap.insert(make_pair(t,0));
			else if(beatname == "midtom")
				g_beatMap.insert(make_pair(t,1));
			else if(beatname == "snare")
				g_beatMap.insert(make_pair(t,2));
			else if(beatname == "hihat")
				g_beatMap.insert(make_pair(t,3));
		}
		g_mapIter = g_beatMap.begin();
	}
	// Set the global sample rate before creating class instances.
  	Stk::setSampleRate( 44100 );
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
	
	g_drummer = new Drummer();

	timeval time;
	gettimeofday(&time, NULL);
	g_time = (time.tv_sec * 1000) + (time.tv_usec / 1000);
	
	g_dac->startStream();
	
	takeInput();
	
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


