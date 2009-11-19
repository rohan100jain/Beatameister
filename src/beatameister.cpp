//-----------------------------------------------------------------------------
// name: beatameister.cpp
// desc: boiler plate GL program
//
// to compile (OS X):
//     g++ -o beatameister beatameister.cpp -framework OpenGL -framework GLUT
//
// to run:
//     ./beatameister
//
// Music 256a | Stanford University | Ge Wang
//     http://ccrma.stanford.edu/courses/256a/
//-----------------------------------------------------------------------------
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <stdio.h>
#include "RtAudio.h"
#include "chuck_fft.h"
#include "xtract/libxtract.h"
#include <sstream>
// Mac OS X
#include <GLUT/glut.h>
// other platforms
// #include <GL/glut.h>
//rt_lpc headers
#include <assert.h>
#include <string.h>
#include <queue>
// STK
#include "RtAudio.h"
#include "Thread.h"

// OpenGL
#if defined(__OS_MACOSX__)
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#if defined(__OS_WINDOWS__) && !defined(__WINDOWS_PTHREAD__)
#include <process.h>
#else
#include <unistd.h>
#endif

#if defined(__MACOSX_CORE__)
#include "midiio_osx.h"
#elif defined(__LINUX_ALSA__)
#include "midiio_alsa.h"
#elif defined(__LINUX_OSS__)
#include "midiio_alsa.h"
#elif defined(__LINUX_JACK__)
#include "midiio_alsa.h"
#else
#include "midiio_win32.h"
#endif
#include "lpc.h"
//end rt_lpc headers

#include "dwt.c"

// OSCPACK
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"
#include "osc/OscOutboundPacketStream.h"

using namespace std;

#define MY_FREQ 44100
#define MY_PIE 3.14159265358979
#define SAMPLE float
// Mappings
// 36: bass, 38: snare, 45: midtom, 42:hihat
// Constants that can be changed via the keyboard
int g_numLastBuffersToSee = 100;
int g_numLastBuffersToUse = 4;


int g_numMaxBuffersToUse = 100;

long g_bufferSize; // size of buffer in samples

// raw sample buffers
vector<SAMPLE *> g_sampleBuffers;
int g_sampleBuffersSize = 400;

// Am I recording?
bool g_recording = false;
// Threshold on RMS Energy to start recording
double g_energyThreshold = 0.01;

// Should opengl thread extract features
bool g_shouldCalculateFeatures = false;

// Globals Representing Features
float g_zeroCrossings = 0, g_rmsAmplitude = 0, g_rolloff = 0, g_centroid = 0, g_kurtosis = 0, g_spectral_stddev = 0, g_pitch = 0;
xtract_mel_filter * g_mMFilters;
float *g_mfcc;

string g_instrument = "";
// zcr threshold
double g_zcrThreshold = 0.1, g_pitchThreshold = 25;
// Samples for feature extraction
SAMPLE * g_samples;
int g_samplesSize = 0;

int g_numBuffersSeen = 0;
int g_start = 0;

//rt_lpc globals
SAMPLE * g_audio_buffer;
SAMPLE * g_another_buffer;
SAMPLE * g_buffest;
SAMPLE * g_residue;
SAMPLE * g_coeff;
GLboolean g_ready = FALSE;
#if defined(__LINUX_ALSA__) || defined(__LINUX_OSS__)
unsigned int g_srate = 24000;
GLboolean do_ola = TRUE;
#elif defined(__MACOSX_CORE__)
unsigned int g_srate = 44100;
GLboolean do_ola = FALSE;
#else
unsigned int g_srate = 22050;
GLboolean do_ola = TRUE;
#endif

lpc_data g_lpc = NULL;
float g_speed = 1.0f;
int g_order = 30;
bool g_balance = false;
bool g_train = true;
//end rt_lpc globals

SAMPLE * g_dwt;

// osc globals
UdpTransmitSocket *g_transmitSocket = NULL;
int SERVERPORT = 8000;
string g_ADDRESS = "127.0.0.1";

// RtAudio callback function
int callback_func( void *output_buffer, void *input_buffer, unsigned int nFrames, double streamTime, RtAudioStreamStatus
 status, void *user_data ) {
	SAMPLE * old_buffer = (SAMPLE *)input_buffer;
	// Local Buffer
	float *m_buffer = (SAMPLE *)malloc(sizeof(SAMPLE)*g_bufferSize);
	// copy 
	memcpy(m_buffer, old_buffer, sizeof(SAMPLE)*g_bufferSize);
	if(g_sampleBuffers.size() == g_sampleBuffersSize) {
		g_sampleBuffers.erase(g_sampleBuffers.begin());
		g_start--;
	}
	g_sampleBuffers.push_back(m_buffer);
	// detect energy via RMS
	float sum = 0;	
	for( int i = 0; i < nFrames; i++ )
    {
		sum+= old_buffer[i]*old_buffer[i];
    }
	sum/=nFrames;
	sum = sqrt(sum);
	
	// cout<<sum<<endl;
	if(!g_recording && sum>g_energyThreshold) {
		// Start Recording
		g_recording = true;
		g_numBuffersSeen = 0;	
		g_start = g_sampleBuffers.size();
		cout<<"Started Recording Automatically"<<endl;
			
	}
		
	if(g_recording) {
		if(sum<g_energyThreshold)
			g_recording = false;
		else {
		if(g_numBuffersSeen == 0) {
			g_samplesSize = 0;
		}
		if(g_numBuffersSeen < g_numLastBuffersToUse) {
			for( int i = 0; i < nFrames; i++ )
	    	{
				// Do Something
				g_samples[g_samplesSize] = old_buffer[i];
				g_samplesSize++;
	    	}
		}
		g_numBuffersSeen++;
		// cout<<g_numBuffersSeen<<endl;
		if(g_numBuffersSeen == g_numLastBuffersToUse) {
			// g_recording = false;
			// g_numBuffersSeen = 0;
			g_shouldCalculateFeatures = true;
			// cout<<"Now it should calculate features"<<endl;
		}
		}
	}
    
	return 0;
}

//-----------------------------------------------------------------------------
// feature detection function prototypes
//-----------------------------------------------------------------------------
float * getSpectrum( );
void initialize_lpc();
void calculate_lpc();
void initMFCC();

void detectZeroCrossings( float& );
void detectRMSAmplitude( float& );
void detectRollOff(float &result, float *spectrum, int n);
// void detectSpectralStandardDeviation(float &result, float *spectrum, int n);
void detectPitch(float &result);
void detectSpectralStandardDeviation(float &result);
void detectSpectralKurtosis(float &result, float *spectrum, int n);
void detectSpectralCentroid(float &result, float *spectrum, int n);
void detectMFCC(float *result, float *spectrum, int n);
void calculate_dwt();

//-----------------------------------------------------------------------------
// function prototypes
//-----------------------------------------------------------------------------
void idleFunc( );
void displayFunc( );
void reshapeFunc( GLsizei width, GLsizei height );
void keyboardFunc( unsigned char, int, int );
void mouseFunc( int button, int state, int x, int y );
void initialize( );




//-----------------------------------------------------------------------------
// global variables and #defines
//-----------------------------------------------------------------------------
#define __PI    3.1415926

#define ABS(x)  (float) (x < 0 ? -x : x)
#define COS(x)  (float) cos( (double) (x) * __PI / 180.0 )
#define SIN(x)  (float) sin( (double) (x) * __PI / 180.0 )


// width and height of the window
GLsizei g_width = 800;
GLsizei g_height = 600;

// light 0 position
GLfloat g_light0_pos[4] = { 2.0f, 8.2f, 4.0f, 1.0f };




//-----------------------------------------------------------------------------
// Name: main( )
// Desc: starting point
//-----------------------------------------------------------------------------
int main( int argc, char ** argv )
{
	// Get RtAudio Instance with default API
	RtAudio *audio = new RtAudio();
    // buffer size
    unsigned int buffer_size = 512;
	// Output Stream Parameters
	RtAudio::StreamParameters outputStreamParams;
	outputStreamParams.deviceId = audio->getDefaultOutputDevice();
	outputStreamParams.nChannels = 1;
	// Input Stream Parameters
	RtAudio::StreamParameters inputStreamParams;
	inputStreamParams.deviceId = audio->getDefaultInputDevice();
	inputStreamParams.nChannels = 1;
	
	// Get RtAudio Stream
	try {
		audio->openStream(
			NULL,
			&inputStreamParams,
			RTAUDIO_FLOAT32,
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
	g_bufferSize = buffer_size;
	// Samples for Feature Extraction in a Buffer
	g_samples = (SAMPLE *)malloc(sizeof(SAMPLE)*g_bufferSize*g_numMaxBuffersToUse);
	g_audio_buffer = (SAMPLE *)malloc(sizeof(SAMPLE)*g_bufferSize*g_numMaxBuffersToUse);
	g_another_buffer = (SAMPLE *)malloc(sizeof(SAMPLE)*g_bufferSize*g_numMaxBuffersToUse);
	g_buffest = (SAMPLE *)malloc(sizeof(SAMPLE)*g_bufferSize*g_numMaxBuffersToUse);
	g_residue = (SAMPLE *)malloc(sizeof(SAMPLE)*g_bufferSize*g_numMaxBuffersToUse);
	g_coeff = (SAMPLE *)malloc(sizeof(SAMPLE)*g_order);
    g_dwt = (SAMPLE *)malloc(sizeof(SAMPLE)*g_bufferSize*g_numMaxBuffersToUse);
	
    // initialize GLUT
    glutInit( &argc, argv );
    // double buffer, use rgb color, enable depth buffer
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    // initialize the window size
    glutInitWindowSize( g_width, g_height );
    // set the window postion
    glutInitWindowPosition( 100, 100 );
    // create the window
    glutCreateWindow( "The New File" );
    
    // set the idle function - called when idle
    glutIdleFunc( idleFunc );
    // set the display function - called when redrawing
    glutDisplayFunc( displayFunc );
    // set the reshape function - called when client area changes
    glutReshapeFunc( reshapeFunc );
    // set the keyboard function - called on keyboard events
    glutKeyboardFunc( keyboardFunc );
    // set the mouse function - called on mouse stuff
    glutMouseFunc( mouseFunc );
    
    // do our own initialization
    initialize();

	// initialize mfcc
	initMFCC();
	
	//init lpc
	initialize_lpc();
	
	// initialize osc
	// Initialize a socket to get a port
	g_transmitSocket = new UdpTransmitSocket( IpEndpointName( g_ADDRESS.c_str(), SERVERPORT ) );
	
	// Start Stream
	try {
        audio->startStream();
    } catch( RtError & err ) {
        // do stuff
        err.printMessage();
        goto cleanup;
    }

    // let GLUT handle the current thread from here
    glutMainLoop();
    
 	// if we get here, then stop!
	try{
		audio->stopStream();
	} 
	catch( RtError & err ) {
		// do stuff
		err.printMessage();
	}

	cleanup:
	    audio->closeStream();
	    delete audio;

    return 0;
}




//-----------------------------------------------------------------------------
// Name: initialize( )
// Desc: sets initial OpenGL states
//       also initializes any application data
//-----------------------------------------------------------------------------
void initialize()
{
    // set the GL clear color - use when the color buffer is cleared
    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
    // set the shading model to 'smooth'
    glShadeModel( GL_SMOOTH );
    // enable depth
    glEnable( GL_DEPTH_TEST );
    // set the front faces of polygons
    glFrontFace( GL_CCW );
    // draw in wireframe
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    
    // enable lighting
    glEnable( GL_LIGHTING );
    // enable lighting for front
    glLightModeli( GL_FRONT_AND_BACK, GL_TRUE );
    // material have diffuse and ambient lighting 
    glColorMaterial( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE );
    // enable color
    glEnable( GL_COLOR_MATERIAL );
    
    // enable light 0
    glEnable( GL_LIGHT0 );    
    // set the position of the lights
    glLightfv( GL_LIGHT0, GL_POSITION, g_light0_pos );
}




//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void reshapeFunc( GLsizei w, GLsizei h )
{
    // save the new window size
    g_width = w; g_height = h;
    // map the view port to the client area
    glViewport( 0, 0, w, h );
    // set the matrix mode to project
    glMatrixMode( GL_PROJECTION );
    // load the identity matrix
    glLoadIdentity( );
    // create the viewing frustum
    gluPerspective( 90.0, (GLfloat) w / (GLfloat) h, .1, 50.0 );
    // set the matrix mode to modelview
    glMatrixMode( GL_MODELVIEW );
    // load the identity matrix
    glLoadIdentity( );
    // position the view point
    gluLookAt( 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
}




//-----------------------------------------------------------------------------
// name: rand2f()
// desc: generate a random float
//-----------------------------------------------------------------------------
GLfloat rand2f( float a, float b )
{
    return a + (b-a)*(rand() / (GLfloat)RAND_MAX);
}




//-----------------------------------------------------------------------------
// Name: keyboardFunc( )
// Desc: key event
//-----------------------------------------------------------------------------
void keyboardFunc( unsigned char key, int x, int y )
{
    switch( key )
    {
        case 'Q':
        case 'q':
            exit(1);
            break;
		case 'z':
			if(g_numLastBuffersToSee < g_sampleBuffersSize) {
				g_numLastBuffersToSee++;
				cout<<"Increasing Window of Visualization to "<<g_numLastBuffersToSee<<endl;
			}
			else {
				cout<<"Can't increase Window of Visualization further. Max reached! "<<endl;
			}
			break;
		case 'x':
			if(g_numLastBuffersToSee > 2) {
				g_numLastBuffersToSee--;
				cout<<"Decreasing Window of Visualization to "<<g_numLastBuffersToSee<<endl;
			}
			else {
				cout<<"Can't Further Decrease Window from "<<g_numLastBuffersToSee<<endl;
			}
			break;
		case 'a':
			if(g_numLastBuffersToUse < g_numMaxBuffersToUse) {
				g_numLastBuffersToUse++;
				cout<<"Increasing Number of previous buffers to use for feature extraction to "<<g_numLastBuffersToUse<<endl;
			}
			else {
				cout<<"Can't Increase Number of previous buffers to use for feature extraction. Max Reached"<<endl;
			}
			break;
		case 's':
			if(g_numLastBuffersToUse > 2) {
				g_numLastBuffersToUse--;
				cout<<"Decreasing Number of previous buffers to use for feature extraction to "<<g_numLastBuffersToUse<<endl;
			}
			else {
				cout<<"Can't Further Decrease Number of previous buffers to use for feature extraction from "<<g_numLastBuffersToUse<<endl;
			}
			break;
		case 'g':
			g_recording = true;
			g_start = g_sampleBuffers.size();
			cout<<"Started Recording"<<endl;
			break;


    }
    
    glutPostRedisplay( );
}




//-----------------------------------------------------------------------------
// Name: mouseFunc( )
// Desc: handles mouse stuff
//-----------------------------------------------------------------------------
void mouseFunc( int button, int state, int x, int y )
{
    if( button == GLUT_LEFT_BUTTON )
    {
        // when left mouse button is down, move left
        if( state == GLUT_DOWN )
        {
        }
        else
        {
        }
    }
    else if ( button == GLUT_RIGHT_BUTTON )
    {
        // when right mouse button down, move right
        if( state == GLUT_DOWN )
        {
        }
        else
        {
        }
    }
    
    glutPostRedisplay( );
}




//-----------------------------------------------------------------------------
// Name: idleFunc( )
// Desc: callback from GLUT
//-----------------------------------------------------------------------------
void idleFunc( )
{
    // render the scene
    glutPostRedisplay( );
    
    // reshape
    // reshapeFunc( g_width, g_height );
}

void beginText() {
	glMatrixMode (GL_PROJECTION);
	
	// push
	glPushMatrix();
	
	// Make the current matrix the identity matrix
	glLoadIdentity();
	
	// Set the projection (to 2D orthographic)
	gluOrtho2D(0, g_width, 0, g_height);
	
	glMatrixMode(GL_MODELVIEW);
}

void endText() {
	glMatrixMode(GL_PROJECTION);
	
	//pop
	glPopMatrix();
	
	glMatrixMode(GL_MODELVIEW);
}

//-----------------------------------------------------------------------------
// name: draw_string()
// desc: ...
//-----------------------------------------------------------------------------
void draw_string( GLfloat x, GLfloat y, GLfloat z, const char * str, GLfloat scale = 1.0f )
{
    GLint len = strlen( str ), i;

    glPushMatrix();
    glTranslatef( x, y, z );
    glScalef( .001f * scale, .001f * scale, .001f * scale );

    for( i = 0; i < len; i++ )
        glutStrokeCharacter( GLUT_STROKE_ROMAN, str[i] );
    
    glPopMatrix();
}

//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc( )
{
    // clear the color and depth buffers
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	
	static float *spectrum;
	double x = -4;
	double xinc = ::fabs(2*x / (g_bufferSize*g_numLastBuffersToSee));

	// push
	glPushMatrix();
		// set the color
		glColor3f(0.25, 0.25, 1.0);
		// draw the center line
		glBegin( GL_LINE_STRIP );
			glVertex3f( x, 2 , 0);
			glVertex3f( -1*x, 2 , 0);
		glEnd();
		
		// Visualize the last g_numLastBuffersToSee buffers
		for(int j=g_sampleBuffers.size()-g_numLastBuffersToSee;j<g_sampleBuffers.size();j++) {
		
			if(j>=g_start && j<=g_start + g_numLastBuffersToUse) {
				// Change Color since this buffer is used for feature detection
				glColor3f(0.25, 1.0, 0.25);				
			}
			else {
				// Reset Color
				glColor3f(0.25, 0.25, 1.0);				
			}
			// Draw the lines		
			glBegin( GL_LINE_STRIP );
		    	for( int i = 0; i < g_bufferSize; i++) {
		        	// set the next vertex
		 			glVertex3f( x, 2 + 3*g_sampleBuffers[j][i] ,0);
		        	// increment x
		        	x += xinc;
		    	}
		   	glEnd();
			
		}
    // pop
	glPopMatrix();
	
	if(!g_recording) {
		x = -3.8;
		xinc = ::fabs(-1*x/g_samplesSize);
		// push
		glPushMatrix();
			// Change Color since this buffer is used for feature detection
			glColor3f(0.25, 1.0, 0.25);	
			
			// draw the center line
			glBegin( GL_LINE_STRIP );
				glVertex3f( x, 0 , 0);
				glVertex3f( 0, 0 , 0);
			glEnd();
						
			// Draw the lines		
			glBegin( GL_LINE_STRIP );
		    	for( int i = 0; i < g_samplesSize; i++) {
		        	// set the next vertex
		 			glVertex3f( x, 3*g_samples[i] ,0);
		        	// increment x
		        	x += xinc;
		    	}
		   	glEnd();			
		// pop
		glPopMatrix();
	    
	}
	if(g_shouldCalculateFeatures) {
		cout<<"Calculating features"<<endl;
		// Reset Features
		g_zeroCrossings = 0;
		g_rmsAmplitude = 0;
		g_rolloff = 0;
		g_kurtosis = 0;
		g_spectral_stddev = 0;
		g_pitch = 0;
		for(int i=0;i<g_mMFilters->n_filters;i++)
			g_mfcc[i] = 0;
		// Calculate Features
		detectZeroCrossings(g_zeroCrossings);
		// detectRMSAmplitude(g_rmsAmplitude);
		// spectrum = getSpectrum();
		// int n = g_samplesSize/2;
		// detectRollOff(g_rolloff, spectrum, n);
		// detectSpectralCentroid(g_centroid, spectrum, n);
		// detectSpectralStandardDeviation(g_spectral_stddev);
		detectPitch(g_pitch);
		// detectSpectralStandardDeviation(g_spectral_stddev, spectrum, n);
		// detectSpectralKurtosis(g_kurtosis, spectrum, n);
		// detectMFCC(g_mfcc, spectrum, n);
		// calculate_dwt();
		// calculate_lpc();
		g_shouldCalculateFeatures = false;
		
		// Instrument Classification
		
		char buffer[1024];
	    osc::OutboundPacketStream m_p( buffer, 1024 );
	    // Message to get a port
	    m_p << osc::BeginBundleImmediate
			<< osc::BeginMessage( "/play" );

	    
		if(g_zeroCrossings > g_zcrThreshold) {
			g_instrument = "snare";
			m_p<<2;
		}
		else {
			if(g_pitch > g_pitchThreshold) {
				g_instrument = "midtom";
				m_p<<1;
			}
			else {
				g_instrument = "bass";
				m_p<<0;
			}
		}
		m_p << osc::EndMessage << osc::EndBundle;
		g_transmitSocket->Send( m_p.Data(), m_p.Size() );
	    
	}
	//glPushMatrix();
	// Render Features
		// set the color
		//glColor3f(0.25, 0.25, 1.0);
	
		ostringstream s,s1,s2,s3,s4,s5;
		s<<"Zero Crossing Rate: "<<g_zeroCrossings;	    
		draw_string(1.5,1,0,s.str().c_str(),1);
		s1<<"RMS Amp: "<<g_rmsAmplitude;	    
		draw_string(1.5,0.5,0,s1.str().c_str(),1);
		s2<<"RollOff: "<<g_rolloff;	    
		draw_string(1.5,0,0,s2.str().c_str(),1);
		s3<<"Centroid: "<<g_centroid;	    
		draw_string(1.5,-0.5,0,s3.str().c_str(),1);
		s4<<"Kurtosis: "<<(g_kurtosis/100000000000.0);	    
		draw_string(1.5,-1,0,s4.str().c_str(),1);
		s5<<"Pitch: "<<g_pitch;	    
		draw_string(1.5,-1.5,0,s5.str().c_str(),1);

		// set the color
		glColor3f(1.0, 0.25, 0.25);
		draw_string(0,0.5,0,g_instrument.c_str(),2);
	
		
	x=-2.5;
	double y = -2;
	xinc = ::fabs(2*x /g_samplesSize);
	// Render Features
	glPushMatrix();
	// set the color
	glColor3f(0.25, 1.0, 0.25);
	// draw the center line
	glBegin( GL_LINE_STRIP );
	glVertex3f( x, y , 0);
	glVertex3f( -1*x, y , 0);
	glEnd();

	draw_string(-3.8,y,0,"DWT: ",1);
	
	//glColor3f(0.25, 1.0, 0.25);				
	// Draw the lines		
	glBegin( GL_LINE_STRIP );
	// Visualize the last g_numLastBuffersToSee buffers
	for(int j=0;j<g_samplesSize;j++) {
		// set the next vertex
		//			if (g_another_buffer[j] > 0)
		//				cout<<"Non zero entity obtained "<<100000000*g_another_buffer[j]<< endl;
		glVertex3f( x, y+3*(g_dwt[j]) ,0);
		x += xinc;
	}
	glEnd();
	// pop
	glPopMatrix();	
	
	x=-2.5;
	y = -1.5;
	xinc = ::fabs(2*x /g_order);
	// Render Features
	glPushMatrix();
	// set the color
	glColor3f(1.0, 0.25, 0.25);
	// draw the center line
	glBegin( GL_LINE_STRIP );
	glVertex3f( x, y , 0);
	glVertex3f( -1*x, y , 0);
	glEnd();
	
	draw_string(-3.8,y,0,"LPC Coeffients: ",1);
	
	//glColor3f(0.25, 1.0, 0.25);				
	// Draw the lines		
	glBegin( GL_LINE_STRIP );
	// Visualize the last g_numLastBuffersToSee buffers
	for(int j=0;j<g_order;j++) {
		// set the next vertex
		//			if (g_another_buffer[j] > 0)
		//				cout<<"Non zero entity obtained "<<100000000*g_another_buffer[j]<< endl;
		glVertex3f( x, y+(g_coeff[j]) ,0);
		x += xinc;
	}
	glEnd();
	// pop
	glPopMatrix();	
	
	
	x = -2.5;
	y = -2.5;
	xinc = ::fabs(2*x /g_mMFilters->n_filters);
	// Render Features
	glPushMatrix();
	// set the color
	glColor3f(0.25, 1.0, 0.25);
	// draw the center line
	glBegin( GL_LINE_STRIP );
	glVertex3f( x, y , 0);
	glVertex3f( -1*x, y , 0);
	glEnd();
	
	draw_string(-3.8,y,0,"MFCC Coeffs.",1);
	
	//glColor3f(0.25, 1.0, 0.25);				
	// Draw the lines		
	glBegin( GL_LINE_STRIP );
	// Visualize the last g_numLastBuffersToSee buffers
	for(int j=0;j<g_mMFilters->n_filters;j++) {
		// set the next vertex
		glVertex3f( x, y+0.1*g_mfcc[j] ,0);
		x += xinc;
	}
	glEnd();
	// pop
	glPopMatrix();	
	
	//glPopMatrix();
	// done

    // flush and swap
    glFlush( );
    glutSwapBuffers( );
}

// ---------------------------------------
// Feature Detecting Functions
// ---------------------------------------

float * getSpectrum( ) {
	
	float *result = (float *)malloc(sizeof(float)*g_samplesSize);
	float argv[3];
	argv[0] = (float)MY_FREQ/g_samplesSize;
	argv[1] = (float)XTRACT_MAGNITUDE_SPECTRUM;
	argv[2] = 0.0;
	xtract[XTRACT_SPECTRUM](g_samples, g_samplesSize, argv, result);
	return result;
}

void detectZeroCrossings(float &result) {
	xtract[XTRACT_ZCR](g_samples,g_samplesSize, NULL, &result );
	cout<<"Extracting Zero Crossing Rate from buffer of size "<<g_samplesSize<<": "<<result<<endl;
}


void detectRMSAmplitude(float &result) {
	xtract[XTRACT_RMS_AMPLITUDE](g_samples,g_samplesSize, NULL, &result );
	cout<<"Extracting Root Mean Square Amplitude from buffer of size "<<g_samplesSize<<": "<<result<<endl;
}

void detectRollOff(float &result, float *spectrum, int n) {
	// the spectrum consists of n amplitudes and n frequencies. So, size 2*n
	float argv[2] = {(float)MY_FREQ/n, 0.85};
	xtract[XTRACT_ROLLOFF](spectrum, n, argv, &result);
	result = spectrum[n + (int)result];
}

void detectSpectralCentroid(float &result, float *spectrum, int n) {

	xtract[XTRACT_SPECTRAL_CENTROID](spectrum, 2*n, NULL, (float *)&result);

}
void detectSpectralStandardDeviation(float &result) {
	// Local Buffer to store stft transform
	float *m_dataBuffer = (SAMPLE *)malloc(sizeof(SAMPLE)*g_samplesSize);
	// copy 
	memcpy(m_dataBuffer, g_samples, sizeof(SAMPLE)*g_samplesSize);
	// Get stft
	rfft(m_dataBuffer, g_samplesSize/2, FFT_FORWARD);
	double mean = 0;
	for(int i=0;i<g_samplesSize;i+=2) {
		double val = sqrt(m_dataBuffer[i]*m_dataBuffer[i] + m_dataBuffer[i+1]*m_dataBuffer[i+1]);
		mean +=val;
	}
	mean /=g_samplesSize/2;
	double stddev = 0;
	for(int i=0;i<g_samplesSize;i+=2) {
		double val = sqrt(m_dataBuffer[i]*m_dataBuffer[i] + m_dataBuffer[i+1]*m_dataBuffer[i+1]);
		stddev +=(val-mean)*(val - mean);
	}
	stddev /= g_samplesSize/2;
	stddev = sqrt(stddev);
	cout<<"Mean: "<<mean<<" Std Dev: "<<stddev<<endl;
	result = stddev;
}

void detectPitch(float &result) {
	cout<<"Detecting pitch of buffer of size "<<g_samplesSize<<endl;
	// Local Buffer to store stft transform
	float *m_dataBuffer = (SAMPLE *)malloc(sizeof(SAMPLE)*g_samplesSize);
	// copy 
	memcpy(m_dataBuffer, g_samples, sizeof(SAMPLE)*g_samplesSize);
	
	// Track Pitch
	float *autocorrelationArray = (SAMPLE *)malloc(sizeof(SAMPLE)*g_samplesSize);
	// memcpy(autocorrelationArray, m_dataBuffer, sizeof(SAMPLE)*g_samplesSize);
	xtract[XTRACT_AUTOCORRELATION](m_dataBuffer,g_samplesSize, NULL, autocorrelationArray );
	// get stft
	rfft(autocorrelationArray, g_samplesSize/2, FFT_FORWARD);
	// calculate the pitch == frequency with 
	// the maximum signal in this stft of autocorrelated signal
	float max = 0;
	int maxfreq = -1;
	for( int i = 0; i < g_samplesSize; i+=2 )
	{
		double val = sqrt(autocorrelationArray[i]*autocorrelationArray[i] + autocorrelationArray[i+1]*autocorrelationArray[i+1]);
		if(val > max) {
			max = val;
			maxfreq = i/2;
		}
	}
	result = maxfreq;
}

// void detectSpectralStandardDeviation(float &result, float *spectrum, int n) {
// 	// the spectrum consists of n amplitudes and n frequencies. So, size 2*n
// 	float mean;
// 	xtract[XTRACT_SPECTRAL_MEAN](spectrum, 2*n, NULL, &mean);
// 	float variance;
// 	xtract[XTRACT_SPECTRAL_VARIANCE](spectrum, 2*n, &mean, &variance);
// 	float stddev;
// 	xtract[XTRACT_SPECTRAL_STANDARD_DEVIATION](spectrum, 2*n, &variance, &stddev);
// 	result = stddev;
// 	cout<<"Mean: "<<mean<<" Std Dev: "<<stddev<<endl;
// 	
// }
void detectSpectralKurtosis(float &result, float *spectrum, int n) {
	// the spectrum consists of n amplitudes and n frequencies. So, size 2*n
	float mean;
	xtract[XTRACT_SPECTRAL_MEAN](spectrum, 2*n, NULL, &mean);
	float variance;
	xtract[XTRACT_SPECTRAL_VARIANCE](spectrum, 2*n, &mean, &variance);
	float stddev;
	xtract[XTRACT_SPECTRAL_STANDARD_DEVIATION](spectrum, 2*n, &variance, &stddev);
	cout<<"Mean: "<<mean<<" Std Dev: "<<stddev<<endl;
	float argv[2] = {mean, stddev};	
	xtract[XTRACT_SPECTRAL_KURTOSIS](spectrum, 2*n, argv, &result);
}

void initMFCC() {
	int winSize = 128; // TODO: Change to sth sane
	g_mMFilters = new xtract_mel_filter();
	g_mMFilters->n_filters = 2;
	g_mMFilters->filters = (float **)malloc(sizeof(float*)*g_mMFilters->n_filters);
	for(int n = 0; n < g_mMFilters->n_filters; n++) {
		g_mMFilters->filters[n] = (float *)malloc(sizeof(float)*winSize);
	}
	// xtract_init_mfcc(gGlobals->mBlockSize, gGlobals->mFrameRate/2.0f,  
	// XTRACT_EQUAL_GAIN,80.0f, 18000.0f,
	//                      mMFilters->n_filters, mMFilters->filters);
	xtract_init_mfcc(g_samplesSize, MY_FREQ/2.0f, XTRACT_EQUAL_GAIN,80.0f, 18000.0f, g_mMFilters->n_filters, g_mMFilters->filters);
	
	g_mfcc = (float *)malloc(sizeof(float)*g_mMFilters->n_filters);
	
}
void detectMFCC(float *result, float *spectrum, int n) {
	// the spectrum consists of n amplitudes and n frequencies. So, size 2*n
	cout<<"Extracting MFCC"<<endl;
	xtract[XTRACT_MEL_COEFFS](spectrum, n, g_mMFilters, result);
	for(int i=0;i<g_mMFilters->n_filters;i++) {
		cout<<result[i]<<" ";
	}
	cout<<endl;
}

void initialize_lpc( )
{
    g_lpc = lpc_create( );
}

void calculate_lpc() {
	static const int LP = 4;
    static long int count = 0;
    static char str[1024];
    static unsigned int wf = 0;
	static SAMPLE * buffer = (SAMPLE *)malloc(sizeof(SAMPLE)*g_bufferSize*g_numMaxBuffersToUse);
	static SAMPLE radii[1024];
    static int keys = 0;
    static float bend = 0;
	
    int i;
    float pitch, power, fval;
	
	memcpy( buffer, g_samples, g_samplesSize * sizeof(SAMPLE) );
	memset( g_another_buffer, 0, g_samplesSize * sizeof(SAMPLE) );
	
	if( g_balance )
		lpc_preemphasis( buffer, g_samplesSize, .5 );
	lpc_analyze( g_lpc, buffer, g_samplesSize, g_coeff, g_order, &power, &pitch, g_residue );
	lpc_synthesize( g_lpc, g_another_buffer, g_samplesSize, g_coeff, g_order, power, pitch / g_speed, !g_train );
	if( g_balance )
		lpc_deemphasis( g_another_buffer, g_samplesSize, .5 );
	cout<<"Done some lpc thingies"<<endl;
}

void calculate_dwt() {
	memcpy(g_dwt, g_samples, g_samplesSize * sizeof(SAMPLE));
	fwt97(g_dwt, g_samplesSize);
}
