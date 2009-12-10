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
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/time.h>
// Mac OS X
#include <GLUT/glut.h>
// other platforms
// #include <GL/glut.h>

// Stk
#include "RtAudio.h"
#include "Drummer.h"

// RtAudio #defs
#define MY_FREQ 44100
#define MY_PIE 3.14159265358979
#define SAMPLE double

using namespace std;
using namespace stk;

long g_time;
vector<int> g_instruments;
RtAudio *g_dac = 0;
Drummer *g_drummer = NULL;

map<long,int> g_beatMap;
map<long,int>::iterator g_mapIter;

//-----------------------------------------------------------------------------
// callback function
//-----------------------------------------------------------------------------
int callback_func( void *output_buffer, void *input_buffer, unsigned int nFrames, double streamTime, RtAudioStreamStatus
				  status, void *user_data ) {
	SAMPLE * new_buffer = (SAMPLE *)output_buffer;
	// zero it out
	memset( new_buffer, 0, nFrames * sizeof(SAMPLE));
	// add it to accumulate
	for(int j=0;j<nFrames;j++) {
		new_buffer[j] = g_drummer->tick();
		//+ g_fin.tick()/3;
	}
	return 0;
}

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
    
	//glutFullScreen();
	
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
    
	timeval time;
	gettimeofday(&time, NULL);
	g_time = (time.tv_sec * 1000) + (time.tv_usec / 1000);
	
	// Read in beats file
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
	
	g_instruments.push_back(36);
	g_instruments.push_back(45);
	g_instruments.push_back(38);
	g_instruments.push_back(42);
	
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
	g_drummer = new Drummer();
	// Start Stream
	try {
        g_dac->startStream();
    } catch( RtError & err ) {
        // do stuff
        err.printMessage();
        goto cleanup;
    }
	
    // let GLUT handle the current thread from here
    glutMainLoop();
    
	cleanup:
		g_dac->closeStream();
		delete g_dac;
	
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
int g_displacementFromBottom = 400;
int g_hitSize = 300;
vector<long> hits;
//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc( )
{
    static GLfloat x = 4, y = 4.5, z = -20;
	
	timeval time;
	gettimeofday(&time, NULL);
	long t = (time.tv_sec * 1000) + (time.tv_usec / 1000) - g_time;
	
    // clear the color and depth buffers
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	GLfloat xinc = 2.0*x/g_instruments.size();

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
    
	static bool doesHit = false;
	// Draw spheres
	long start = t + g_windowSize - g_displacementFromBottom;
	if(t%2 == 0)
		doesHit = true;
	else
		doesHit = false;
	
	
	glPushMatrix();
		glColor3f(0.5,0.5,0.5 );
		// Draw Rectangle
		glRectf(-1*x - 1, y - (start-t - g_hitSize/2)*2*y/g_windowSize, x + 1, y - (start-t + g_hitSize/2)*2*y/g_windowSize);
	
		glColor3f(1,1,1 );
		// Add Names
		draw_string(-1*x+0.2*xinc,-1*y+0.2,0,"Boom",3);
		draw_string(-1*x+1.2*xinc,-1*y+0.2,0,"Tok",3);
		draw_string(-1*x+2.2*xinc,-1*y+0.2,0,"Cha",3);
		draw_string(-1*x+3.2*xinc,-1*y+0.2,0,"Chi",3);

	glPopMatrix();
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
		
		if(key >= t - g_hitSize/2 && key <= t+g_hitSize/2) {
			if(find(hits.begin(), hits.end(), key) == hits.end()) {
				if(doesHit) {
					glColor3f(0.8,0.8,0.8 );
					draw_string(-1*x-1,0,0,"hit",5);
					hits.push_back(key);
					radius = 0.5;
					StkFloat note = g_instruments[beat];
					std::cout<<"Trying to strike! "<<note<<std::endl;				
					g_drummer->noteOn( note, 1.0);
				}
			}
			else {
				glColor3f(0.8,0.8,0.8 );
				draw_string(-1*x-1,0,0,"hit",5);
				radius = 0.5;
			}
		}

//		std::cout<<key<<" "<<t<<" "<<radius<<std::endl;
		glTranslatef(-1*x + (beat + 0.5)*xinc, y - (start-key)*2*y/2500 , 0);
		glutSolidSphere(radius,25,25);
		g_mapIter++;		
		glPopMatrix();
	}
	g_mapIter = g_beatMap.begin();
    // flush and swap
    glFlush( );
    glutSwapBuffers( );
}
