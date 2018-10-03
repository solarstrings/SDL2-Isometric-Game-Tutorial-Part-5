#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "isoEngine.h"
#include "isoMap.h"
#include "../texture.h"
#include "../logger.h"

static void isoGenerateMap(isoMapT *isoMap);

isoMapT* isoMapCreateEmptyMap(char *mapName,int width,int height,int numLayers,int tileSize)
{
    //Set failsafe values
    if(height<=0){
        height = 10;
    }

    if(width<=0){
        width = 10;
    }

    if(numLayers<=0){
        numLayers = 1;
    }

    //allocate memory for the map
    isoMapT *isoMap = malloc(sizeof(struct isoMapT));
    if(isoMap == NULL){
        writeToLog("Error in function: isoMapCreateEmptyMap() - Could not allocate memory for isometric map!","runlog.txt");
        return NULL;
    }
    //allocate memory for map data (the actual tiles)
    isoMap->mapData = malloc(1 + width * height * numLayers * sizeof(int));
    if(isoMap->mapData == NULL){
        writeToLog("Error in function: isoMapCreateEmptyMap() - Could not allocate memory for isometric map data!","runlog.txt");
        return NULL;
    }
    memset(isoMap->mapData,-1,width * height *numLayers *sizeof(int));

    //allocate memory for the tile set
    isoMap->tileSet = malloc(sizeof(struct isoTileSetT));
    if(isoMap->tileSet == NULL)
    {
        writeToLog("Error in function: isoMapCreateEmptyMap() - Could not allocate memory for tile set!","runlog.txt");
        return NULL;
    }

    isoMap->tileSet->numTileClipRects = 0;
    isoMap->tileSet->tilesTex = NULL;

    isoMap->tileSet->tileClipRects = NULL;
    isoMap->tileSet->tileSetLoaded = 0;

    isoMap->mapHeight = height;
    isoMap->mapWidth = width;
    isoMap->numLayers = numLayers;

    if(mapName == NULL){
        sprintf(isoMap->name,mapName,"Unnamed map");
    }
    else
    {
        strncpy(isoMap->name,mapName,MAP_NAME_LENGTH-1);
    }
    //Divide the tile size by two
    isoMap->tileSize = tileSize/2;
    isoGenerateMap(isoMap);
    return isoMap;
}

void isoMapFreeMap(isoMapT *isoMap)
{
    if(isoMap != NULL)
    {
        if(isoMap->mapData!=NULL)
        {
            free(isoMap->mapData);
        }
        if(isoMap->tileSet!=NULL)
        {
            if(isoMap->tileSet->tileClipRects!=NULL){
                free(isoMap->tileSet->tileClipRects);
            }
            //Freeing textures is handled by the texture pool, so we don't
            //free the memory the texture pointer is pointing too.
            free(isoMap->tileSet);
        }
        free(isoMap);
    }
}

int isoMapLoadTileSet(isoMapT *isoMap,textureT *texture,int tileWidth,int tileHeight)
{
    int x=0,y=0;
    int w,h;
    int numTilesX;
    int numTilesY;
    int i = 0;
    SDL_Rect tmpRect;

    if(isoMap == NULL)
    {
        writeToLog("Error in function: isoMapLoadTileSet() - Parameter: 'isoMapT *isoMap' is NULL!","runlog.txt");
        return -1;
    }
    //if the texture is NULL
    if(texture == NULL){
        writeToLog("Error in function: isoMapLoadTileSet() - Parameter: 'textureT *texture' is NULL!","runlog.txt");
        return -1;
    }
    //if a tile set already has been loaded
    if(isoMap->tileSet->tileClipRects!=NULL){
        free(isoMap->tileSet->tileClipRects);
    }

    isoMap->tileSet->tilesTex = texture;

    //get width and height
    w = isoMap->tileSet->tilesTex->width;
    h = isoMap->tileSet->tilesTex->height;

    if(w<tileWidth){
        writeToLog("Error in function: isoMapLoadTileSet() - Texture width is smaller than the tile width! Aborting!","runlog.txt");
        return -1;
    }
    if(h<tileHeight){
        writeToLog("Error in function: isoMapLoadTileSet() - Texture height is smaller than the tile height! Aborting!","runlog.txt");
        return -1;
    }
    //calculate the number of tiles that fit in the texture
    numTilesX = floor(w/tileWidth);
    numTilesY = floor(h/tileHeight);

    //set the number of clip rectangles
    isoMap->tileSet->numTileClipRects = numTilesX * numTilesY;

    //allocate memory for the tile clip rectangles
    isoMap->tileSet->tileClipRects = (SDL_Rect*)malloc(sizeof(SDL_Rect)*isoMap->tileSet->numTileClipRects);

    //loop through the texture
    while(1)
    {
        //setup the clip rectangle
        setupRect(&tmpRect,x,y,tileWidth,tileHeight);

        //copy the rectangle
        isoMap->tileSet->tileClipRects[i] = tmpRect;

        //go to the next tile tile the image
        x+=tileWidth;

        //if the x position passed the width of the texture
        if(x>=w){
            //reset the x position
            x = 0;
            //go to the next row in the image
            y += tileHeight;
            if(y>=h){
                //break out of the while loop. No more clip rectangles to create.
                break;
            }
        }
        i++;
    }
    return 1;
}

int isoMapGetTile(isoMapT *isoMap,int x,int y,int layer)
{
    if(x < 0 || x > isoMap->mapWidth-1 || y < 0 || y > isoMap->mapHeight-1 || layer < 0 || layer > isoMap->numLayers){
        return -1;
    }
    return isoMap->mapData[(y * isoMap->mapWidth + x) * isoMap->numLayers + layer];
}

void isoMapSetTile(isoMapT *isoMap,int x,int y,int layer,int value)
{
    if(isoMap == NULL)
    {
        return;
    }

    if(x < 0 || x > isoMap->mapWidth-1 || y < 0 || y > isoMap->mapHeight-1 || layer < 0 || layer > isoMap->numLayers){
        return;
    }
    isoMap->mapData[(y * isoMap->mapWidth + x) * isoMap->numLayers + layer] = value;
}

static void isoGenerateMap(isoMapT *isoMap)
{
    int x,y;
    int paintTile=0;
    //only loop y and x, we will only draw on the ground layer
    for(y=0;y<isoMap->mapHeight;y+=2)
    {
        for(x=0;x<isoMap->mapWidth;x+=2)
        {
            if(rand()%100 >90){
                isoMapSetTile(isoMap,x,y,1,2);
            }
        }
    }

    isoMapSetTile(isoMap,0,3,1,2);
    isoMapSetTile(isoMap,5,3,1,2);
    isoMapSetTile(isoMap,4,3,1,2);
    isoMapSetTile(isoMap,3,3,1,2);
    isoMapSetTile(isoMap,2,3,1,2);
    isoMapSetTile(isoMap,1,3,1,2);

    for(y=0;y<isoMap->mapHeight;y+=2)
    {
        for(x=0;x<isoMap->mapWidth;x+=2)
        {
            isoMapSetTile(isoMap,x,y,0,1);
            isoMapSetTile(isoMap,x,y+1,0,1);
            isoMapSetTile(isoMap,x+1,y,0,1);
            isoMapSetTile(isoMap,x+1,y+1,0,1);
            paintTile = rand()%10;
            if(paintTile>8)
            {
                if(y<isoMap->mapHeight-4 && x<isoMap->mapWidth-4){
                    isoMapSetTile(isoMap,x,y,0,4);
                    isoMapSetTile(isoMap,x,y+1,0,4);
                    isoMapSetTile(isoMap,x+1,y,0,4);
                    isoMapSetTile(isoMap,x+1,y+1,0,4);
                }
            }
            if(paintTile==7){
                if(y<isoMap->mapHeight-4 && x<isoMap->mapWidth-4){
                    isoMapSetTile(isoMap,x,y,0,3);
                    isoMapSetTile(isoMap,x,y+1,0,3);
                    isoMapSetTile(isoMap,x+1,y,0,3);
                    isoMapSetTile(isoMap,x+1,y+1,0,3);
                }
            }
        }
    }
}
