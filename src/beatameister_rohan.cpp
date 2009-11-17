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

using namespace std;

#define MY_FREQ 44100
#define MY_PIE 3.14159265358979
#define SAMPLE float

// Constants that can be changed via the keyboard
int g_numLastBuffersToSee = 20;
int g_numLastBuffersToUse = 10;

int g_numMaxBuffersToUse = 100;

long g_bufferSize; // size of buffer in samples

// raw sample buffers
vector<SAMPLE *> g_sampleBuffers;
int g_sampleBuffersSize = 400;

// Am I recording?
bool g_recording = false;

// Should opengl thread extract features
bool g_shouldCalculateFeatures = false;

// Globals Representing Features
float g_zeroCrossings = 0;

// Samples for feature extraction
SAMPLE * g_samples;
int g_samplesSize = 0;

int g_numBuffersSeen = 0;
int g_start = 0;

//rt_lpc globals
SAMPLE * g_audio_buffer;
SAMPLE * g_another_buffer;
SAMPLE * g_buffest;
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
bool g_balance = true;
bool g_train = true;
//end rt_lpc globals

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
	
	if(g_recording) {
		if(g_numBuffersSeen == 0) {
			g_samplesSize = 0;
		}
		for( int i = 0; i < nFrames; i++ )
	    {
			// Do Something
			g_samples[g_samplesSize] = old_buffer[i];
			g_samplesSize++;
	    }
		g_numBuffersSeen++;
		if(g_numBuffersSeen == g_numLastBuffersToUse) {
			g_recording = false;
			g_numBuffersSeen = 0;
			g_shouldCalculateFeatures = true;
		}
	}
    
	return 0;
}

//-----------------------------------------------------------------------------
// feature detection function prototypes
//-----------------------------------------------------------------------------
void detectZeroCrossings( float& );
void initialize_lpc();
void calculate_lpc();

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

	// Start Stream
	try {
        audio->startStream();
    } catch( RtError & err ) {
        // do stuff
        err.printMessage();
        goto cleanup;
    }
	
	//init lpc
	initialize_lpc();
	
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
		case 'b':
			g_balance = !g_balance;
			fprintf( stderr, "%susing preemphasis/deemphasis filter\n", g_balance ? "" : "NOT " );
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

// To render string
void renderBitmapString(
		float x, 
		float y, 
		float z, 
		void *font, 
		char *string) {  
  char *c;
  glRasterPos3f(x, y,z);
  for (c=string; *c != '\0'; c++) {
    glutBitmapCharacter(font, *c);
  }
}

//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc( )
{
    // clear the color and depth buffers
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	
	
	double x = -4;
	double xinc = ::fabs(2*x / (g_bufferSize*g_numLastBuffersToSee));

	double y = 2;
	// push
	glPushMatrix();
		// set the color
		glColor3f(0.25, 0.25, 1.0);
		// draw the center line
		glBegin( GL_LINE_STRIP );
			glVertex3f( x, y , 0);
			glVertex3f( -1*x, y , 0);
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
		 			glVertex3f( x, y + 3*g_sampleBuffers[j][i] ,0);
		        	// increment x
		        	x += xinc;
		    	}
		   	glEnd();
			
		}
    // pop
	glPopMatrix();
	
	if(g_shouldCalculateFeatures) {
		// Calculate Features
		detectZeroCrossings(g_zeroCrossings);
		calculate_lpc();
		g_shouldCalculateFeatures = false;
		
	}
	
	x=-4;
	y = -2;
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
		
		//glColor3f(0.25, 1.0, 0.25);				
		// Draw the lines		
		glBegin( GL_LINE_STRIP );
		// Visualize the last g_numLastBuffersToSee buffers
		for(int j=0;j<g_samplesSize;j++) {
				// set the next vertex
	//			if (g_another_buffer[j] > 0)
	//				cout<<"Non zero entity obtained "<<100000000*g_another_buffer[j]<< endl;
				glVertex3f( x, y+10*g_another_buffer[j] ,0);
				x += xinc;
		}
		glEnd();
	// pop
	glPopMatrix();
	// done

    // flush and swap
    glFlush( );
    glutSwapBuffers( );
}

// ---------------------------------------
// Feature Detecting Functions
// ---------------------------------------

void detectZeroCrossings(float &result) {
	xtract[XTRACT_ZCR](g_samples,g_samplesSize, NULL, &result );
	cout<<"Extracting Zero Crossing Rate from buffer of size "<<g_samplesSize<<": "<<result<<endl;
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
	static SAMPLE * residue = (SAMPLE *)malloc(sizeof(SAMPLE)*g_bufferSize*g_numMaxBuffersToUse);
	static SAMPLE coefs[1024], radii[1024];
    static int keys = 0;
    static float bend = 0;
	
    int i;
    float pitch, power, fval;

	memcpy( buffer, g_samples, g_samplesSize * sizeof(SAMPLE) );
	memset( g_another_buffer, 0, g_samplesSize * sizeof(SAMPLE) );
	
	if( g_balance )
		lpc_preemphasis( buffer, g_samplesSize, .5 );
	lpc_analyze( g_lpc, buffer, g_samplesSize, coefs, g_order, &power, &pitch, residue );
	lpc_synthesize( g_lpc, g_another_buffer, g_samplesSize, coefs, g_order, power, pitch / g_speed, !g_train );
	if( g_balance )
		lpc_deemphasis( g_another_buffer, g_samplesSize, .5 );
	cout<<"Done some lpc thingies"<<endl;
}
