// mpUIWindowFrame.h
////////////////////////////////////////////////////////

#ifndef mpUIWindowFrame_h
#define mpUIWindowFrame_h

#include "omxplayer/guilib/Geometry.h" // CRect


#ifndef OMX
//---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
#include <SDL2/SDL.h>  // 
#include "mpSettings.h" // AspectMode
//---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
#else
//---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
enum AspectMode { ASPECT_STRETCH, ASPECT_LETTERBOX, ASPECT_NATURAL };
typedef struct SDL_Rect
{
    int x, y;
    int w, h;
} SDL_Rect;
struct VideoWallConfig
{
  bool enabled = false;
  int gridWidth = 2;
  int gridHeight = 2;
  int gridColumn = 1;
  int gridRow = 1;
  int bezelPercentage = 2;
};
//---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 
#endif

class WindowFrame
{
  public:
    WindowFrame();
    WindowFrame( int x, int y, int width, int height );
    WindowFrame( SDL_Rect initSDLRect );
    WindowFrame( CRect initRect );
    ~WindowFrame();
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    SDL_Rect getSDLRect();
    CRect getCRect();
    WindowFrame inset( int insetAmountPixelsX, int insetAmountPixelsY );
    bool operator == ( const WindowFrame &p2);
    bool operator != ( const WindowFrame &p2);
};


// STRUCTURE FOR STORING MARGIN VALUES.
// MARGIN CALCULATION FUNCTIONS SHOULD PROBABLY BE MOVED HERE
struct FrameMargin
{
  float leftMargin;
  float rightMargin;
  float topMargin;
  float bottomMargin;
};


// FUNCTIONS TO CALCULATE COPY WINDOW RECTS BASED ON ASPECT MODE AND VIDEO WALL MODE
enum SourceOrDest { DEST_RECT, SOURCE_RECT };
WindowFrame getCopyRect( WindowFrame sourceFrame, WindowFrame targetFrame, AspectMode aspectMode, float scaleFactor, SourceOrDest selectSourceRect );
WindowFrame getDestCopyRect( WindowFrame sourceFrame, WindowFrame targetFrame, AspectMode aspectMode, float scaleFactor );
WindowFrame getSourceCopyRect( WindowFrame sourceFrame, WindowFrame targetFrame, AspectMode aspectMode, float scaleFactor );
WindowFrame getVideoWallSourceRect(  WindowFrame sourceRect,  WindowFrame destRect,  WindowFrame targetFrame, VideoWallConfig videoWallConfig );
WindowFrame getVideoWallDestRect(  WindowFrame sourceRect,  WindowFrame destRect,  WindowFrame targetFrame, VideoWallConfig videoWallConfig );


#endif