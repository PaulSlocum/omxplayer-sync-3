//  mpUIWindowFrame.cpp
/////////////////////////////////////////////////////////////////////////

#include "mpUIWindowFrame.h"
#include <stdio.h>


/////////////////////////////////////////////////////////////////////////
WindowFrame::WindowFrame( int newX, int newY, int newWidth, int newHeight )
{
  x = newX;
  y = newY;
  width = newWidth;
  height = newHeight;
} 

/////////////////////////////////////////////////////////////////////////
WindowFrame::WindowFrame()
{
}

/////////////////////////////////////////////////////////////////////////
WindowFrame::WindowFrame( SDL_Rect initSDLRect )
{
  x = initSDLRect.x;
  y = initSDLRect.y;
  width = initSDLRect.w;
  height = initSDLRect.h;
}


/////////////////////////////////////////////////////////////////////////
WindowFrame::WindowFrame( CRect initRect )
{
  x = initRect.x1;
  y = initRect.y1;
  width = initRect.x2 - initRect.x1;
  height = initRect.y2 - initRect.y1;
  //printf( "(WINDOWFRAME) Init WITH CRECT: x:%d width:%d y:%d height:%d\n",  x, width, y, height );
}


//////////////////////////////////////////////////////////////////////////
WindowFrame::~WindowFrame()
{
}


////////////////////////////////////////////////////////////////////////
SDL_Rect WindowFrame::getSDLRect()
{
 	SDL_Rect resultSDLRect; //create a rect
	resultSDLRect.x = x;  //controls the rect's x coordinate 
	resultSDLRect.y = y; // controls the rect's y coordinte
	resultSDLRect.w = width; // controls the width of the rect
	resultSDLRect.h = height; // controls the height of the rect
	return( resultSDLRect );
}

///////////////////////////////////////////////////////////////////////////
CRect WindowFrame::getCRect()
{
  CRect resultCRect;
  resultCRect.x1 = x;
  resultCRect.y1 = y;
  resultCRect.x2 = x + width;
  resultCRect.y2 = y + height;
  //printf( "(WINDOWFRAME) getCRect(): x:%d width:%d y:%d height:%d\n",  x, width, y, height );
  return resultCRect;
}



////////////////////////////////////////////////////////////////////////
WindowFrame WindowFrame::inset( int insetAmountPixelsX, int insetAmountPixelsY )
{
  WindowFrame returnFrame;
  returnFrame.x = x + insetAmountPixelsX;
  returnFrame.width = width - insetAmountPixelsX*2; 
  returnFrame.y = y + insetAmountPixelsY;
  returnFrame.height = height - insetAmountPixelsY*2; 
  return( returnFrame );
}

///////////////////////////////////////////////////////////////////////////////////////////////
bool WindowFrame::operator == (const WindowFrame &p2)
{
   if( x==p2.x && y==p2.y && width==p2.width && height==p2.height )
     return true;
   else
     return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////
bool WindowFrame::operator != (const WindowFrame &p2)
{
  return( !( *this == p2 ) );
}
 


// ---  --  ---  --  ---  --  ---  --  ---  --  ---  --  ---  --  ---  --  ---  --  ---  --  ---  --  ---  --  



//==================================================================
WindowFrame getCopyRect( WindowFrame sourceFrame, WindowFrame targetFrame, AspectMode aspectMode, float scaleFactor, SourceOrDest selectSourceRect )
{
    WindowFrame adjustedSourceRect = sourceFrame;
    WindowFrame adjustedDestRect = targetFrame;

    switch( aspectMode )
    {
      case ASPECT_STRETCH:
      {
        // IT'S IN STRETCH MODE BY DEFAULT        
        break;
      }
      
      case ASPECT_LETTERBOX:
      { 
        int widthIfTopBottomAreLocked = sourceFrame.width * ((float)targetFrame.height / (float)sourceFrame.height);
        int heightIfSidesAreLocked = sourceFrame.height * ((float)targetFrame.width / (float)sourceFrame.width);
        if( widthIfTopBottomAreLocked <= targetFrame.width )
        {
          adjustedDestRect.width = widthIfTopBottomAreLocked;
          adjustedDestRect.x = targetFrame.x + (targetFrame.width - adjustedDestRect.width)/2;
        }
        else
        {
          adjustedDestRect.height = heightIfSidesAreLocked;
          adjustedDestRect.y = targetFrame.y + (targetFrame.height - adjustedDestRect.height)/2;
        }
        break;
      }
      
      case ASPECT_NATURAL:
      {
        adjustedSourceRect = { 0, 0, sourceFrame.width, sourceFrame.height };
      
        //printf( "IMAGE HEIGHT: %d   IMAGE WIDTH: %d \n", sourceFrame.height, sourceFrame.width );
      
        // IF IMAGE IS WIDER THAN WINDOW...
        if( targetFrame.width < (sourceFrame.width * scaleFactor) )
        {
          adjustedSourceRect.x = (sourceFrame.width - targetFrame.width / scaleFactor)/2;
          adjustedSourceRect.width = targetFrame.width / scaleFactor;
          //printf( "--(WINDOWFRAME) TG FRAME WIDTH: %d  SRC FRAME WIDTH: %d \n", targetFrame.width, sourceFrame.width );
          //printf( "--(WINDOWFRAME) X: %d  WIDTH: %d \n", adjustedSourceRect.x, adjustedSourceRect.width );
        }
        else // ELSE - IMAGE IS NARROWER THAN WINDOW...
        {
          adjustedDestRect.x = targetFrame.x + (targetFrame.width - sourceFrame.width * scaleFactor)/2;
          adjustedDestRect.width = sourceFrame.width * scaleFactor;
        }
      
        // IF IMAGE IS TALLER THAN WINDOW...
        if( targetFrame.height < sourceFrame.height * scaleFactor )
        {
          adjustedSourceRect.y = (sourceFrame.height - targetFrame.height / scaleFactor)/2;
          adjustedSourceRect.height = targetFrame.height / scaleFactor;
        }
        else // ELSE - IMAGE IS SHORTER THAN WINDOW...
        {
          adjustedDestRect.y = targetFrame.y + (targetFrame.height - sourceFrame.height * scaleFactor)/2;
          adjustedDestRect.height = sourceFrame.height * scaleFactor;
        } //*/
        break;
      }
    } // SWITCH //*/
    
    if( selectSourceRect == SOURCE_RECT )
      return adjustedSourceRect;
    else
      return adjustedDestRect;
}


//==================================================================
WindowFrame getDestCopyRect( WindowFrame sourceFrame, WindowFrame targetFrame, AspectMode aspectMode, float scaleFactor )
{
  return getCopyRect( sourceFrame, targetFrame, aspectMode, scaleFactor, DEST_RECT );
}

//==================================================================
WindowFrame getSourceCopyRect( WindowFrame sourceFrame, WindowFrame targetFrame, AspectMode aspectMode, float scaleFactor )
{
  return getCopyRect( sourceFrame, targetFrame, aspectMode, scaleFactor, SOURCE_RECT );
}


//===================================================================
//WindowFrame getVideoWallSourceRect( WindowFrame sourceRect, WindowFrame destRect, WindowFrame targetFrame, VideoWallConfig videoWallConfig )
WindowFrame getVideoWallRect( WindowFrame sourceRect, WindowFrame destRect, WindowFrame targetFrame, VideoWallConfig videoWallConfig, SourceOrDest selectSourceRect )
{
  WindowFrame sourceReturnRect;
  WindowFrame destReturnRect;
  
  if( videoWallConfig.enabled == false )
  {
    // VIDEO WALL DISABLED
    sourceReturnRect = sourceRect;
    destReturnRect = destRect;
  }
  else // ELSE - VIDEO WALL ENABLED
  {
    // IF IMAGE WIDTH FILLS FRAME...
    if( destRect.width >= targetFrame.width )
    {
      sourceReturnRect.width = sourceRect.width / videoWallConfig.gridWidth;
      sourceReturnRect.x = sourceRect.x + (videoWallConfig.gridColumn - 1) * sourceRect.width / videoWallConfig.gridWidth;
      //printf( "(WINDOWFRAME) getVideoWallDestRect: IMAGE FILLS FRAME WIDTH" ); // DEBUG
      destReturnRect.width = destRect.width;
      destReturnRect.x = destRect.x;
    }
    else // ELSE -- IMAGE NARROWER THAN FRAME...
    {
      sourceReturnRect.width = sourceRect.width;
      sourceReturnRect.x = sourceRect.x;
      //printf( "(WINDOWFRAME) getVideoWallDestRect: IMAGE NARROWER THAN FRAME WIDTH" ); // DEBUG
      destReturnRect.width = destRect.width * videoWallConfig.gridWidth;
      destReturnRect.x = targetFrame.x + (destRect.x - targetFrame.x)*videoWallConfig.gridWidth - (videoWallConfig.gridColumn - 1) * targetFrame.width;
    }

    // IF IMAGE HEIGHT FILLS FRAME...
    if( destRect.height >= targetFrame.height )
    {
      sourceReturnRect.height = sourceRect.height / videoWallConfig.gridHeight;
      sourceReturnRect.y = sourceRect.y + (videoWallConfig.gridRow - 1) * sourceRect.height / videoWallConfig.gridHeight;
      //printf( "(WINDOWFRAME) getVideoWallDestRect: IMAGE FILLS FRAME HEIGHT" );
      destReturnRect.height = destRect.height;
      destReturnRect.y = destRect.y;
    }
    else // ELSE -- IMAGE NARROWER THAN FRAME...
    {
      sourceReturnRect.height = sourceRect.height;
      sourceReturnRect.y = sourceRect.y;
      //printf( "(WINDOWFRAME) getVideoWallDestRect: IMAGE NARROWER THAN FRAME HEIGHT" ); // DEBUG
      destReturnRect.height = destRect.height * videoWallConfig.gridHeight;
      destReturnRect.y = targetFrame.y + (destRect.y - targetFrame.y)*videoWallConfig.gridHeight - (videoWallConfig.gridRow - 1) * targetFrame.height;
    }
    
    // ADD MARGIN FOR BEZEL COMPENSATION (EXCEPT FOR EDGES)
    int leftBezelInset = 0;
    int rightBezelInset = 0;
    int topBezelInset = 0;
    int bottomBezelInset = 0;
    int bezelInsetAmountHorz = sourceReturnRect.width * videoWallConfig.bezelPercentage / 100.0 / 2.0 ;
    int bezelInsetAmountVert = sourceReturnRect.height * videoWallConfig.bezelPercentage / 100.0 / 2.0 ;
    //int bezelInsetAmount = targetFrame.width * videoWallConfig.bezelPercentage / 100.0; // <-------------------- ORIGINAL
    //int bezelInsetAmount = targetFrame.width * videoWallConfig.bezelPercentage / 100.0 / 4.0;
    if( videoWallConfig.gridColumn > 1 )
      leftBezelInset = bezelInsetAmountHorz;
    if( videoWallConfig.gridRow > 1 )
      topBezelInset = bezelInsetAmountVert;
    if( videoWallConfig.gridColumn < videoWallConfig.gridWidth )
      rightBezelInset = bezelInsetAmountHorz;
    if( videoWallConfig.gridRow < videoWallConfig.gridHeight )
      bottomBezelInset = bezelInsetAmountVert;
    sourceReturnRect.x += leftBezelInset;
    sourceReturnRect.y += topBezelInset;
    sourceReturnRect.width -= leftBezelInset + rightBezelInset;
    sourceReturnRect.height -= topBezelInset + bottomBezelInset;
    
    // IF DEST RECT IS NOT VISIBLE...
    if( destReturnRect.x + destReturnRect.width < targetFrame.x  ||  destReturnRect.x > targetFrame.x + targetFrame.width || 
        destReturnRect.y + destReturnRect.height < targetFrame.y  ||  destReturnRect.y > targetFrame.y + targetFrame.height )
    {
      // MAKE NULL IMAGE WINDOW...
      destReturnRect.x = 0;
      destReturnRect.y = 0;
      destReturnRect.width = 0;
      destReturnRect.height = 0;//*/
    }
    else // ELSE - IT IS VISIBLE, SO CHECK TO SEE IF IMAGE NEEDS TO BE CROPPED TO TARGET WINDOW...
    {
      // CROP TO LEFT OF FRAME IF NECESSARY...
      if( destReturnRect.x < targetFrame.x )
      {
        WindowFrame destReturnRectPreCrop = destReturnRect;
        WindowFrame sourceReturnRectPreCrop = sourceReturnRect;
        destReturnRect.x = targetFrame.x;
        destReturnRect.width = destReturnRectPreCrop.width - (targetFrame.x - destReturnRectPreCrop.x);
        sourceReturnRect.x = sourceReturnRectPreCrop.x + sourceReturnRectPreCrop.width * 0.5;
        sourceReturnRect.width = (float)sourceReturnRectPreCrop.width * ( ((float)targetFrame.x - (float)destReturnRectPreCrop.x) / (float)destReturnRectPreCrop.width );
	    }

      // CROP TO RIGHT OF FRAME IF NECESSARY...
      if( (destReturnRect.x + destReturnRect.width) > (targetFrame.x + targetFrame.width) )
      {
        WindowFrame destReturnRectPreCrop = destReturnRect;
        WindowFrame sourceReturnRectPreCrop = sourceReturnRect;
        destReturnRect.x = destReturnRectPreCrop.x;
        destReturnRect.width = destReturnRectPreCrop.width - ( (destReturnRect.x + destReturnRect.width) - (targetFrame.x + targetFrame.width) );
        sourceReturnRect.width = (float)sourceReturnRectPreCrop.width * 
                                      ( (destReturnRectPreCrop.x + destReturnRectPreCrop.width) - (targetFrame.x + targetFrame.width) ) / (float)destReturnRectPreCrop.width;
	    } 

      // CROP ABOVE FRAME IF NECESSARY...
      if( destReturnRect.y < targetFrame.y )
      {
        WindowFrame destReturnRectPreCrop = destReturnRect;
        WindowFrame sourceReturnRectPreCrop = sourceReturnRect;
        destReturnRect.y = targetFrame.y;
        destReturnRect.height = destReturnRectPreCrop.height - (targetFrame.y - destReturnRectPreCrop.y);
        sourceReturnRect.y = sourceReturnRectPreCrop.y + sourceReturnRectPreCrop.height * 0.5;
        sourceReturnRect.height = (float)sourceReturnRectPreCrop.height * ( ((float)targetFrame.y - (float)destReturnRectPreCrop.y) / (float)destReturnRectPreCrop.height );
	    }

      // CROP BELOW FRAME IF NECESSARY...
      if( destReturnRect.y + destReturnRect.height > targetFrame.y + targetFrame.height )
      {
        WindowFrame destReturnRectPreCrop = destReturnRect;
        WindowFrame sourceReturnRectPreCrop = sourceReturnRect;
        destReturnRect.y = destReturnRectPreCrop.y;
        destReturnRect.height = destReturnRectPreCrop.height - ( (destReturnRect.y + destReturnRect.height) - (targetFrame.y + targetFrame.height) );
        sourceReturnRect.height = (float)sourceReturnRectPreCrop.height * 
                                      ( (destReturnRectPreCrop.y + destReturnRectPreCrop.height) - (targetFrame.y + targetFrame.height) ) / (float)destReturnRectPreCrop.height;
	    } //*/

    }
  }
  
  if( selectSourceRect == SOURCE_RECT )
    return sourceReturnRect; 
  else
    return destReturnRect;
}


//===================================================================
WindowFrame getVideoWallSourceRect( __attribute__((unused)) WindowFrame sourceRect, WindowFrame destRect, WindowFrame targetFrame, VideoWallConfig videoWallConfig )
{
  return getVideoWallRect( sourceRect, destRect, targetFrame, videoWallConfig, SOURCE_RECT );
}


//===================================================================
WindowFrame getVideoWallDestRect( __attribute__((unused)) WindowFrame sourceRect, WindowFrame destRect, WindowFrame targetFrame, VideoWallConfig videoWallConfig )
{
  return getVideoWallRect( sourceRect, destRect, targetFrame, videoWallConfig, DEST_RECT );
}















