//-----------------------------------------------------------------------------
// name: thenewfile.cpp
// desc: boiler plate GL program
//
// to compile (OS X):
//     g++ -o thenewfile thenewfile.cpp -framework OpenGL -framework GLUT
//
// to run:
//     ./thenewfile
//
// Music 256a | Stanford University | Ge Wang
//     http://ccrma.stanford.edu/courses/256a/
//-----------------------------------------------------------------------------
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <stdio.h>
#include <vector>
#include <assert.h>
#include <string.h>
#include <queue>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/time.h>

//music processing
#include "chuck_fft.h"
#include "xtract/libxtract.h"

// OpenGL
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

// Stk
#include "RtAudio.h"
#include "Drummer.h"
#include "FileWvIn.h";

// RtAudio #defs
#define MY_FREQ 44100
#define MY_PIE 3.14159265358979
#define SAMPLE float



using namespace std;
using namespace stk;


//display state
int g_displayState = 0;

//beats and song file name
string g_beatFileName = "blink.txt";
string g_songFileName = "blink.wav";

int g_hackIndex = 0;
// playing back
long g_time;
// file in
FileWvIn g_fin;

vector<int> g_instruments;
RtAudio *g_dac = 0;
Drummer *g_drummer = NULL;

//beat map stuff
map<long,int> g_beatMap;
map<long,int>::iterator g_mapIter;

// Constants that can be changed via the keyboard
int g_numLastBuffersToSee = 100;
int g_numLastBuffersToUse = 2;

//recording sample globals
vector<SAMPLE *> g_sampleBuffers;
int g_sampleBuffersSize = 400;
int g_numMaxBuffersToUse = 100;
long g_bufferSize; // size of buffer in samples
SAMPLE * g_samples;
int g_samplesSize = 0;
int g_numBuffersSeen = 0;
int g_start = 0;

// Am I recording?
bool g_recording = false, g_recording1 = false, g_recording2 = false, firstTime = false;
// Threshold on RMS Energy to start recording
double g_energyThreshold1 = 0.02, g_energyThreshold2 = 0.9, g_ratioThreshold = 100;

// Should opengl thread extract features
bool g_shouldCalculateFeatures = false;

// Features and instrument
float g_zeroCrossings = 0, g_centroid = 0, g_pitch = 0;
string g_instrument = "";
int g_instrumentId = -1;

// zcr threshold
double g_zcrThreshold = 0.1, g_pitchThreshold = 1.1;

//-----------------------------------------------------------------------------
// callback function
//-----------------------------------------------------------------------------
int callback_func( void *output_buffer, void *input_buffer, unsigned int nFrames, double streamTime, RtAudioStreamStatus
				  status, void *user_data ) {
	
	timeval time;
	gettimeofday(&time, NULL);
	long hackTime = (time.tv_sec * 1000) + (time.tv_usec / 1000) - g_time;
	
	if(hackTime/300 == (g_hackIndex+1) && g_hackIndex<g_instruments.size()) {
		g_drummer->noteOn(g_instruments[g_hackIndex], 1);
		g_hackIndex++;
	}
	
	SAMPLE * old_buffer = (SAMPLE *)input_buffer;
	SAMPLE * new_buffer = (SAMPLE *)output_buffer;
	for(int i=0;i<nFrames;i++) {
			new_buffer[i] =  g_drummer->tick() + g_fin.tick()/3;
	}
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
	float ratio = 0;
	for( int i = 0; i < nFrames; i++ )
    {
		sum+= m_buffer[i]*m_buffer[i];
    }
	sum/=nFrames;
	sum = sqrt(sum);

	if(sum > g_energyThreshold1) {
		if(!g_recording1) {
			firstTime = true;
		}
		g_recording1 = true;
	}
	if(sum > g_energyThreshold2)
		g_recording2 = true;
	if(g_recording1 && sum < g_energyThreshold1)
		g_recording1 = false;
	if(g_recording2 && sum < g_energyThreshold2)
		g_recording2 = false;
	
	if(!g_recording) {
		if(g_recording2 || firstTime) {			
			// Start Recording
			g_recording = true;
			g_numBuffersSeen = 0;
			g_samplesSize = 0;
			g_start = g_sampleBuffers.size()-1;
			cout<<"Started Recording Automatically"<<endl;
			g_instrument = "";			
		}
	}
	else {
		if(!firstTime && !g_recording2)
			g_recording = false;
		if(!g_recording1) {
			g_recording = false;
			firstTime = false;
		}
	}
	
	if(g_recording) {
			if(g_numBuffersSeen < g_numLastBuffersToUse) {
				for( int i = 0; i < nFrames; i++ )
				{
					// Do Something
					g_samples[g_samplesSize] = m_buffer[i];
					g_samplesSize++;
				}
			}
			g_numBuffersSeen++;
			// cout<<g_numBuffersSeen<<endl;
			if(g_numBuffersSeen == g_numLastBuffersToUse) {
				// g_recording = false;
				// g_numBuffersSeen = 0;
				g_shouldCalculateFeatures = true;
				firstTime = false;
				// cout<<"Now it should calculate features"<<endl;
			}
	}
    
	return 0;	
}

//-----------------------------------------------------------------------------
// feature detection function prototypes
//-----------------------------------------------------------------------------
float * getSpectrum(  SAMPLE * samples, int samplesSize);
void init_beatsMap();
void detectZeroCrossings(  SAMPLE * samples, int samplesSize, float& );
void detectPitch( SAMPLE * samples, int samplesSize, float &result);
void detectSpectralCentroid(float &result, float *spectrum, int n);

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
    // initialize GLUT
    glutInit( &argc, argv );
    // double buffer, use rgb color, enable depth buffer
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    // initialize the window size
    glutInitWindowSize( g_width, g_height );
    // set the window postion
    glutInitWindowPosition( 100, 100 );
    // create the window
    glutCreateWindow( "Beat Box Hero" );
    
	glutFullScreen();
	
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

	// Set the global sample rate before creating class instances.
  	Stk::setSampleRate( 44100 );
  	Stk::showWarnings( true );
  	Stk::setRawwavePath( "stk-4.4.1/rawwaves/" );
	
		
	g_instruments.push_back(36);
	g_instruments.push_back(45);
	g_instruments.push_back(38);
	g_instruments.push_back(42);
	
	
	// Get RtAudio Instance with default API
	g_dac = new RtAudio();
    // buffer size
    unsigned int buffer_size = 512;
	// Output Stream Parameters
	RtAudio::StreamParameters outputStreamParams;
	outputStreamParams.deviceId = g_dac->getDefaultOutputDevice();
	outputStreamParams.nChannels = 1;
	
	// Input Stream Parameters
	RtAudio::StreamParameters inputStreamParams;
	inputStreamParams.deviceId = g_dac->getDefaultInputDevice();
	inputStreamParams.nChannels = 1;
	
	
	// Get RtAudio Stream
	try {
		g_dac->openStream(
						  &outputStreamParams,
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
	// Samples for Feature Extraction in a Buffer
	g_bufferSize = buffer_size;
	g_samples = (SAMPLE *)malloc(sizeof(SAMPLE)*g_bufferSize*g_numMaxBuffersToUse);
	
	g_drummer = new Drummer();
	
    // let GLUT handle the current thread from here
    glutMainLoop();
    
	return 0;
}


void init_playing() {
		ifstream iff(g_beatFileName.c_str());
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
		
		try 
		{
			// read the file
			g_fin.openFile(g_songFileName.c_str());
			// change the rate
			g_fin.setRate( 1 );
			// normalize the peak
			g_fin.normalize();
		} catch( StkError & e )
		{
			cerr << "baaaaaaaaad..." << endl;
			return;
		}	
	timeval time;
	gettimeofday(&time, NULL);
	g_time = (time.tv_sec * 1000) + (time.tv_usec / 1000);
	
	// Start Stream
	try {
        g_dac->startStream();
    } catch( RtError & err ) {
        // do stuff
        err.printMessage();
    }
	
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
//	glOrtho (-8, 8, -10, 10, 0.1, 50);
    gluPerspective( 50.0, (GLfloat) w / (GLfloat) h, .1, 50.0 );
    // set the matrix mode to modelview
    glMatrixMode( GL_MODELVIEW );
    // load the identity matrix
    glLoadIdentity( );
    // position the view point
    gluLookAt( 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
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
			if (g_displayState == 0) {
				cout <<"Pressed a button "<<x<<"  "<<y<<endl;
				if (x>285 && y>528 && x<725 && y<548) {
					g_beatFileName = "blink.txt";
					g_songFileName = "blink.wav";
					cout<<"Pressed option1"<<endl;
					g_displayState = 1;
				}
				if (x>288 && y> 626 && x<826 && y<654) {
					g_beatFileName = "offsprings.txt";
					g_songFileName = "offsprings.wav";
					cout<<"Pressed option2"<<endl;		
					g_displayState = 1;
				}
			}
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

int g_windowSize = 2500;
int g_displacementFromBottom = 600;
int g_hitSize = 400;
float y_temp = 0;
vector<long> hits;
float g_initSize = 1;
float g_size = g_initSize;
float g_rate = 0.005;
float g_finalSize = 5;
int g_index = 1;
int g_totalBalls = 0;
int g_totalHits = 0;
//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc( )
{
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	
	if (g_displayState == 0) {
		
		glPushMatrix();
		
		ostringstream s1,s2,s3,s4;
		s1<<"What's my age again - Blink 182";
		draw_string(-4.5,-1,0,s1.str().c_str(),2);
		s2<<"Why don't you get a job - The Offspring";
		draw_string(-4.5,-2,0,s2.str().c_str(),2);
		s3<<"Select Song";
		draw_string(-5,0,0,s3.str().c_str(),3);
		s4<<"BeatBox Hero";
		draw_string(-3,3,0,s4.str().c_str(),6);
		
		
		glPopMatrix();
		
	}
	
	if (g_displayState == 1){
		ostringstream s;
		s<<"Ready";
		draw_string(-g_size/10,0,0,s.str().c_str(),g_size);

		if (g_size >= g_finalSize)
			g_index++;
		else {
			g_size += g_rate;
		}
		if (g_index > 500) {
			g_displayState = 5;
			init_playing();
			g_size = g_initSize;
		}
	}
	
	if (g_displayState == 5) {
		static float *spectrum;
		GLfloat x = 4, y = 4.5, z = -20;

		
		double xinc = 2.0*x/g_instruments.size();

		// Draw Lines
		// push
		glPushMatrix();
			glColor3f(0.7,0.7,0.7 );
			for(int i=0;i<g_instruments.size()+1;i++) {
				glBegin( GL_LINE_STRIP );
					glVertex3f(-1*x + i*xinc  , y ,0);
					glVertex3f(-1*x + i*xinc, -1*y ,0 );
				glEnd();
			}

		
		// pop
		glPopMatrix();
		
	//	static bool doesHit = false;
		g_instrumentId = -1;
		if(g_shouldCalculateFeatures) {
			cout<<"Calculating features"<<endl;
			// Reset Features
			g_zeroCrossings = 0;
			g_centroid = 0;
			g_pitch = 0.0;
			// Local Buffer
			float *m_dataBuffer = (SAMPLE *)malloc(sizeof(SAMPLE)*g_samplesSize);
			// copy 
			memcpy(m_dataBuffer, g_samples, sizeof(SAMPLE)*g_samplesSize);		
			int m_samplesSize = g_samplesSize;
			// Calculate Features
			detectZeroCrossings(m_dataBuffer, m_samplesSize,  g_zeroCrossings);
			spectrum = getSpectrum(m_dataBuffer, m_samplesSize);
			int n = g_samplesSize/2;
			detectSpectralCentroid(g_centroid, spectrum, n);
			detectPitch(m_dataBuffer, m_samplesSize, g_pitch);
			g_shouldCalculateFeatures = false;
			
			if(g_zeroCrossings > g_zcrThreshold) {
				if(g_centroid < 6000) {
					g_instrument = "snare";
					g_instrumentId = 2;
				}
				else {
					g_instrument = "hihat";
					g_instrumentId = 3;
				}
			}
			else {
				if(g_pitch > g_pitchThreshold) {
					g_instrument = "midtom";
					g_instrumentId = 1;
				}
				else {
					g_instrument = "bass";
					g_instrumentId = 0;
				}
			}
		}
		
		/*ostringstream s,s1,s2,s3,s4,s5,s6,s7;
		s<<"Zero Crossing Rate: "<<g_zeroCrossings;	    
		draw_string(1.5,1,0,s.str().c_str(),1);
		s5<<"Pitch: "<<g_pitch;	    
		draw_string(1.5,-1.5,0,s5.str().c_str(),1);
		s7<<"Spectral Centroid: "<<g_centroid;	    
		draw_string(1.5,-2.5,0,s7.str().c_str(),1);
		*/
		// set the color
//		glColor3f(1.0, 0.25, 0.25);
//		draw_string(0,0.5,0,g_instrument.c_str(),2);
		
		
	//	if(t%2 == 0)
	//		doesHit = true;
	//	else
	//		doesHit = false;
		float percent_hit = 0;
		if (g_totalBalls > 0) {
			percent_hit = (float)g_totalHits/(float)g_totalBalls*100;
		}
		ostringstream s,p;
		s<<"Score: "<<g_totalHits*5;
		draw_string(5,4,0,s.str().c_str(),2);
		p<<(int)percent_hit<<"%";
		draw_string(5.5,-y+1.6*y/100*percent_hit+0.2,0,p.str().c_str(),2);
		
		glColor3f(0.5,0.8,0.5 );
		glRectf(5.5,-y+1.6*y/100*percent_hit , 6,-y);

		
		timeval time;
		gettimeofday(&time, NULL);
		long t = (time.tv_sec * 1000) + (time.tv_usec / 1000) - g_time;
		
		// Draw spheres
		long start = t - g_displacementFromBottom;
		glPushMatrix();
			glColor3f(0.5,0.5,0.5 );
			// Draw Rectangle
			glRectf(-1*x - 1, -y + (t-start + g_hitSize/2-50)*2*y/g_windowSize, x + 1, -y + (t-start - g_hitSize/2-50)*2*y/g_windowSize);
		
			glColor3f(1,1,1 );
			// Add Names
		if(g_hackIndex == 1)
			draw_string(-1*x+0.2*xinc,-1*y+0.2,0,"Boom",4);
		else
			draw_string(-1*x+0.2*xinc,-1*y+0.2,0,"Boom",3);
		
		if(g_hackIndex == 2)
			draw_string(-1*x+1.2*xinc,-1*y+0.2,0,"Tok",4);
		else
			draw_string(-1*x+1.2*xinc,-1*y+0.2,0,"Tok",3);
		if(g_hackIndex == 3)
			draw_string(-1*x+2.2*xinc,-1*y+0.2,0,"Cha",4);
		else
			draw_string(-1*x+2.2*xinc,-1*y+0.2,0,"Cha",3);		
		if(g_hackIndex == 4)
			draw_string(-1*x+3.2*xinc,-1*y+0.2,0,"Chi",4);
		else
			draw_string(-1*x+3.2*xinc,-1*y+0.2,0,"Chi",3);
		
		glPopMatrix();
		g_totalBalls = 0;
		while(g_mapIter != g_beatMap.end()) {
			long key = g_mapIter->first;
			int beat = g_mapIter->second;
			glPushMatrix();
			switch(beat) {
				case 0: 
					glColor3f(0.8,0.2,0.2 );
					break;
				case 1: 
					glColor3f(0.2,0.8,0.2 );
					break;
				case 2: 
					glColor3f(0.2,0.2,0.8 );
					break;
				case 3: 
					glColor3f(0.8,0.8,0.5 );
					break;
			}
			GLfloat radius = 0.3;
			if (key <=t+g_hitSize/2)
				g_totalBalls++;
			
			if(key >= t - g_hitSize/2 && key <= t+g_hitSize/2) {
				if(find(hits.begin(), hits.end(), key) == hits.end()) {
					if((g_instrumentId == beat)||(beat == 2 && g_instrumentId == 3)||(beat ==3 && g_instrumentId == 2)) {
						glColor3f(0.8,0.8,0.8 );
						draw_string(-1*x-1,0,0,"hit",5);
						hits.push_back(key);
						radius = 0.5;
						StkFloat note = g_instruments[beat];
						std::cout<<"Trying to strike! "<<note<<std::endl;				
						g_drummer->noteOn( note, 1.0);
						g_totalHits++;
					}
				}
				else {
					glColor3f(0.8,0.8,0.8 );
					draw_string(-1*x-1,0,0,"hit",5);
					radius = 0.5;
				}
			}
			
	//		std::cout<<key<<" "<<t<<" "<<radius<<std::endl;
			glTranslatef(-1*x + (beat + 0.5)*xinc, -y + (key-start)*2*y/g_windowSize , 0);
			glutSolidSphere(radius,25,25);
			g_mapIter++;		
			glPopMatrix();
		}
		g_mapIter = g_beatMap.begin();
		
		x=-4;
		xinc = ::fabs(2*x / (g_bufferSize*g_numLastBuffersToSee));
		y=3;
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
			
			if(j>=g_start && j<g_start + g_numLastBuffersToUse) {
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
				glVertex3f( x, y+ 3*g_sampleBuffers[j][i] ,0);
				// increment x
				x += xinc;
			}
			glEnd();
			
		}
		x=-4;
		glBegin( GL_LINE_STRIP );
		glVertex3f( x, y_temp , 0);
		glVertex3f( -1*x, y_temp , 0);
		glEnd();	
		
		// pop
		glPopMatrix();
	}
	
    // flush and swap
    glFlush( );
    glutSwapBuffers( );
}


// ---------------------------------------
// Feature Detecting Functions
// ---------------------------------------

float * getSpectrum( SAMPLE * samples, int samplesSize) {
	
	float *result = (float *)malloc(sizeof(float)*samplesSize);
	float argv[3];
	argv[0] = (float)MY_FREQ/samplesSize;
	argv[1] = (float)XTRACT_MAGNITUDE_SPECTRUM;
	argv[2] = 0.0;
	xtract[XTRACT_SPECTRUM](samples, samplesSize, argv, result);
	return result;
}

void detectZeroCrossings( SAMPLE * samples, int samplesSize, float &result) {
	// Local Buffer
	float *m_dataBuffer = (SAMPLE *)malloc(sizeof(SAMPLE)*samplesSize);
	// copy 
	memcpy(m_dataBuffer, samples, sizeof(SAMPLE)*samplesSize);
	float mean;
	xtract[XTRACT_MEAN](m_dataBuffer, samplesSize, NULL, &mean);
	for(int i=0;i<samplesSize;i++) {
		m_dataBuffer[i] -= mean;
	}
	xtract[XTRACT_ZCR](m_dataBuffer,samplesSize, NULL, &result );
	cout<<"Extracting Zero Crossing Rate from buffer of size "<<samplesSize<<": "<<result<<endl;
}


void detectSpectralCentroid(float &result, float *spectrum, int n) {
	xtract[XTRACT_SPECTRAL_CENTROID](spectrum, 2*n, NULL, (float *)&result);
}


void detectPitch( SAMPLE * samples, int samplesSize, float &result) {
	cout<<"Detecting pitch of buffer of size "<<samplesSize<<endl;
	// Local Buffer to store stft transform
	float *m_dataBuffer = (SAMPLE *)malloc(sizeof(SAMPLE)*samplesSize);
	// copy 
	memcpy(m_dataBuffer, samples, sizeof(SAMPLE)*samplesSize);
	
	// Track Pitch
	float *autocorrelationArray = (SAMPLE *)malloc(sizeof(SAMPLE)*samplesSize);
	// memcpy(autocorrelationArray, m_dataBuffer, sizeof(SAMPLE)*samplesSize);
	xtract[XTRACT_AUTOCORRELATION](m_dataBuffer,samplesSize, NULL, autocorrelationArray );
	// get stft
	rfft(autocorrelationArray, samplesSize/2, FFT_FORWARD);
	// calculate the pitch == frequency with 
	// the maximum signal in this stft of autocorrelated signal
	float max = 0;
	int maxfreq = -1;
	for( int i = 0; i < samplesSize; i+=2 )
	{
		double val = sqrt(autocorrelationArray[i]*autocorrelationArray[i] + autocorrelationArray[i+1]*autocorrelationArray[i+1]);
		if(val > max) {
			max = val;
			maxfreq = i/2;
		}
	}
	result = 100*(float)maxfreq/(float)samplesSize;
}

