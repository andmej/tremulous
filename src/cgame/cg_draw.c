// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_draw.c -- draw all of the graphical elements during
// active (after loading) gameplay

/*
 *  Portions Copyright (C) 2000-2001 Tim Angus
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*  To assertain which portions are licensed under the LGPL and which are
 *  licensed by Id Software, Inc. please run a diff between the equivalent
 *  versions of the "Tremulous" modification and the unmodified "Quake3"
 *  game source code.
 */

#include "cg_local.h"

int drawTeamOverlayModificationCount = -1;
int sortedTeamPlayers[TEAM_MAXOVERLAY];
int numSortedTeamPlayers;

char systemChat[256];
char teamChat1[256];
char teamChat2[256];

/*
==============
CG_DrawField

Draws large numbers for status bar and powerups
==============
*/
static void CG_DrawField (int x, int y, int width, int value) {
  char  num[16], *ptr;
  int   l;
  int   frame;

  if ( width < 1 ) {
    return;
  }

  // draw number string
  if ( width > 5 ) {
    width = 5;
  }

  switch ( width ) {
  case 1:
    value = value > 9 ? 9 : value;
    value = value < 0 ? 0 : value;
    break;
  case 2:
    value = value > 99 ? 99 : value;
    value = value < -9 ? -9 : value;
    break;
  case 3:
    value = value > 999 ? 999 : value;
    value = value < -99 ? -99 : value;
    break;
  case 4:
    value = value > 9999 ? 9999 : value;
    value = value < -999 ? -999 : value;
    break;
  }

  Com_sprintf (num, sizeof(num), "%i", value);
  l = strlen(num);
  if (l > width)
    l = width;
  x += 2 + CHAR_WIDTH*(width - l);

  ptr = num;
  while (*ptr && l)
  {
    if (*ptr == '-')
      frame = STAT_MINUS;
    else
      frame = *ptr -'0';

    CG_DrawPic( x,y, CHAR_WIDTH, CHAR_HEIGHT, cgs.media.numberShaders[frame] );
    x += CHAR_WIDTH;
    ptr++;
    l--;
  }
}


/*
================
CG_Draw3DModel

================
*/
void CG_Draw3DModel( float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles ) {
  refdef_t    refdef;
  refEntity_t   ent;

  if ( !cg_draw3dIcons.integer || !cg_drawIcons.integer ) {
    return;
  }

  CG_AdjustFrom640( &x, &y, &w, &h );

  memset( &refdef, 0, sizeof( refdef ) );

  memset( &ent, 0, sizeof( ent ) );
  AnglesToAxis( angles, ent.axis );
  VectorCopy( origin, ent.origin );
  ent.hModel = model;
  ent.customSkin = skin;
  ent.renderfx = RF_NOSHADOW;   // no stencil shadows

  refdef.rdflags = RDF_NOWORLDMODEL;

  AxisClear( refdef.viewaxis );

  refdef.fov_x = 30;
  refdef.fov_y = 30;

  refdef.x = x;
  refdef.y = y;
  refdef.width = w;
  refdef.height = h;

  refdef.time = cg.time;

  trap_R_ClearScene();
  trap_R_AddRefEntityToScene( &ent );
  trap_R_RenderScene( &refdef );
}

/*
================
CG_DrawHead

Used for both the status bar and the scoreboard
================
*/
void CG_DrawHead( float x, float y, float w, float h, int clientNum, vec3_t headAngles ) {
  clipHandle_t  cm;
  clientInfo_t  *ci;
  float     len;
  vec3_t      origin;
  vec3_t      mins, maxs;

  ci = &cgs.clientinfo[ clientNum ];

  if ( cg_draw3dIcons.integer ) {
    cm = ci->headModel;
    if ( !cm ) {
      return;
    }

    // offset the origin y and z to center the head
    trap_R_ModelBounds( cm, mins, maxs );

    origin[2] = -0.5 * ( mins[2] + maxs[2] );
    origin[1] = 0.5 * ( mins[1] + maxs[1] );

    // calculate distance so the head nearly fills the box
    // assume heads are taller than wide
    len = 0.7 * ( maxs[2] - mins[2] );
    origin[0] = len / 0.268;  // len / tan( fov/2 )

    // allow per-model tweaking
    VectorAdd( origin, ci->headOffset, origin );

    CG_Draw3DModel( x, y, w, h, ci->headModel, ci->headSkin, origin, headAngles );
  } else if ( cg_drawIcons.integer ) {
    CG_DrawPic( x, y, w, h, ci->modelIcon );
  }

  // if they are deferred, draw a cross out
  if ( ci->deferred ) {
    CG_DrawPic( x, y, w, h, cgs.media.deferShader );
  }
}

/*
================
CG_DrawFlagModel

Used for both the status bar and the scoreboard
================
*/
void CG_DrawFlagModel( float x, float y, float w, float h, int team, qboolean force2D ) {
  qhandle_t   cm;
  float     len;
  vec3_t      origin, angles;
  vec3_t      mins, maxs;
  qhandle_t   handle;

  if ( !force2D && cg_draw3dIcons.integer ) {

    VectorClear( angles );

    cm = cgs.media.redFlagModel;

    // offset the origin y and z to center the flag
    trap_R_ModelBounds( cm, mins, maxs );

    origin[2] = -0.5 * ( mins[2] + maxs[2] );
    origin[1] = 0.5 * ( mins[1] + maxs[1] );

    // calculate distance so the flag nearly fills the box
    // assume heads are taller than wide
    len = 0.5 * ( maxs[2] - mins[2] );
    origin[0] = len / 0.268;  // len / tan( fov/2 )

    angles[YAW] = 60 * sin( cg.time / 2000.0 );;

    CG_Draw3DModel( x, y, w, h,
      team == TEAM_HUMANS ? cgs.media.redFlagModel : cgs.media.blueFlagModel,
      0, origin, angles );
  } else if ( cg_drawIcons.integer ) {
    gitem_t *item = BG_FindItemForPowerup( team == TEAM_HUMANS ? PW_REDFLAG : PW_BLUEFLAG );

    CG_DrawPic( x, y, w, h, cg_items[ ITEM_INDEX(item) ].icon );
  }
}

/*
================
CG_DrawStatusBarHead

================
*/
static void CG_DrawStatusBarHead( float x ) {
  vec3_t    angles;
  float   size, stretch;
  float   frac;

  VectorClear( angles );

  if ( cg.damageTime && cg.time - cg.damageTime < DAMAGE_TIME ) {
    frac = (float)(cg.time - cg.damageTime ) / DAMAGE_TIME;
    size = ICON_SIZE * 1.25 * ( 1.5 - frac * 0.5 );

    stretch = size - ICON_SIZE * 1.25;
    // kick in the direction of damage
    x -= stretch * 0.5 + cg.damageX * stretch * 0.5;

    cg.headStartYaw = 180 + cg.damageX * 45;

    cg.headEndYaw = 180 + 20 * cos( crandom()*M_PI );
    cg.headEndPitch = 5 * cos( crandom()*M_PI );

    cg.headStartTime = cg.time;
    cg.headEndTime = cg.time + 100 + random() * 2000;
  } else {
    if ( cg.time >= cg.headEndTime ) {
      // select a new head angle
      cg.headStartYaw = cg.headEndYaw;
      cg.headStartPitch = cg.headEndPitch;
      cg.headStartTime = cg.headEndTime;
      cg.headEndTime = cg.time + 100 + random() * 2000;

      cg.headEndYaw = 180 + 20 * cos( crandom()*M_PI );
      cg.headEndPitch = 5 * cos( crandom()*M_PI );
    }

    size = ICON_SIZE * 1.25;
  }

  // if the server was frozen for a while we may have a bad head start time
  if ( cg.headStartTime > cg.time ) {
    cg.headStartTime = cg.time;
  }

  frac = ( cg.time - cg.headStartTime ) / (float)( cg.headEndTime - cg.headStartTime );
  frac = frac * frac * ( 3 - 2 * frac );
  angles[YAW] = cg.headStartYaw + ( cg.headEndYaw - cg.headStartYaw ) * frac;
  angles[PITCH] = cg.headStartPitch + ( cg.headEndPitch - cg.headStartPitch ) * frac;

  CG_DrawHead( x, 480 - size, size, size,
        cg.snap->ps.clientNum, angles );
}

/*
================
CG_DrawStatusBarFlag

================
*/
static void CG_DrawStatusBarFlag( float x, int team ) {
  CG_DrawFlagModel( x, 480 - ICON_SIZE, ICON_SIZE, ICON_SIZE, team, qfalse );
}


/*
================
CG_DrawTeamBackground

================
*/
void CG_DrawTeamBackground( int x, int y, int w, int h, float alpha, int team )
{
  vec4_t    hcolor;

  hcolor[3] = alpha;
  if ( team == TEAM_HUMANS ) {
    hcolor[0] = 1;
    hcolor[1] = 0;
    hcolor[2] = 0;
  } else if ( team == TEAM_DROIDS ) {
    hcolor[0] = 0;
    hcolor[1] = 0;
    hcolor[2] = 1;
  } else {
    return; 
  }
  trap_R_SetColor( hcolor );
  CG_DrawPic( x, y, w, h, cgs.media.teamStatusBar );
  trap_R_SetColor( NULL );
}

/*
================
CG_DrawLighting

================
*/
static void CG_DrawLighting( void )
{
  centity_t   *cent;
  int         currentLum;
  static int  lum;
  vec3_t      point, direction;
  
  cent = &cg_entities[cg.snap->ps.clientNum];

  VectorCopy( cent->lerpOrigin, point );

  AngleVectors( cg.predictedPlayerState.viewangles, direction, NULL, NULL );

  currentLum = CG_LightFromDirection( point, direction );
  //CG_Printf( "%d\n", lum );
  if( abs( lum - currentLum ) > 4 )
    lum = currentLum;

  switch( cg.snap->ps.stats[ STAT_PCLASS ] )
  {
    case PCL_D_B_BASE:
    case PCL_D_O_BASE:
    case PCL_D_D_BASE:
      if( lum < 10 )
        CG_DrawPic( -4, -4, 648, 488, cgs.media.droidNav9 );
      else if( lum >= 10 && lum < 16 )
        CG_DrawPic( -4, -4, 648, 488, cgs.media.droidNav8 );
      else if( lum >= 16 && lum < 22 )
        CG_DrawPic( -4, -4, 648, 488, cgs.media.droidNav7 );
      else if( lum >= 22 && lum < 28 )
        CG_DrawPic( -4, -4, 648, 488, cgs.media.droidNav6 );
      else if( lum >= 28 && lum < 34 )
        CG_DrawPic( -4, -4, 648, 488, cgs.media.droidNav5 );
      else if( lum >= 34 && lum < 40 )
        CG_DrawPic( -4, -4, 648, 488, cgs.media.droidNav4 );
      else if( lum >= 40 && lum < 46 )
        CG_DrawPic( -4, -4, 648, 488, cgs.media.droidNav3 );
      else if( lum >= 46 && lum < 53 )
        CG_DrawPic( -4, -4, 648, 488, cgs.media.droidNav2 );
      else if( lum >= 53 )
        CG_DrawPic( -4, -4, 648, 488, cgs.media.droidNav1 );

      break;

    case PCL_H_BASE:
      if( BG_activated( UP_NVG, cg.snap->ps.stats ) )
        CG_DrawPic( 0, 0, 640, 480, cgs.media.humanNV );
      break;
  }

  //fade to black if stamina is low
  if( ( cg.snap->ps.stats[ STAT_STAMINA ] < -800  ) &&
      ( cg.snap->ps.stats[ STAT_PTEAM ] == PTE_HUMANS ) )
  {
    vec4_t black = { 0, 0, 0, 0 };
    black[ 3 ] = 1.0 - ( (float)( cg.snap->ps.stats[ STAT_STAMINA ] + 1000 ) / 200.0f );
    trap_R_SetColor( black );
    CG_DrawPic( 0, 0, 640, 480, cgs.media.whiteShader );
    trap_R_SetColor( NULL );
  }
}

/*
================
CG_DrawStatusBar

================
*/
static void CG_DrawStatusBar( void ) {
  int     color;
  centity_t *cent;
  playerState_t *ps;
  int     value;
  int     ammo, clips, maxclips;
  vec4_t    hcolor;
  vec3_t    angles;
  vec3_t    origin;
  static float colors[4][4] = {
//    { 0.2, 1.0, 0.2, 1.0 } , { 1.0, 0.2, 0.2, 1.0 }, {0.5, 0.5, 0.5, 1} };
    { 0.3f, 0.4f, 0.3f, 1.0f } ,   // normal
    { 1.0f, 0.2f, 0.2f, 1.0f },   // low health
    {0.2f, 0.3f, 0.2f, 1.0f},     // weapon firing
    { 1.0f, 1.0f, 1.0f, 1.0f } };     // health > 100


  if ( cg_drawStatus.integer == 0 ) {
    return;
  }

  // draw the team background
  CG_DrawTeamBackground( 0, 420, 640, 60, 0.33f, cg.snap->ps.persistant[PERS_TEAM] );

  cent = &cg_entities[cg.snap->ps.clientNum];
  ps = &cg.snap->ps;

  VectorClear( angles );

  //TA: stop drawing all these silly 3d models on the hud. Saves space and is more realistic.
  
  // draw any 3D icons first, so the changes back to 2D are minimized
  /*if ( cent->currentState.weapon && cg_weapons[ cent->currentState.weapon ].ammoModel ) {
    origin[0] = 70;
    origin[1] = 0;
    origin[2] = 0;
    angles[YAW] = 90 + 20 * sin( cg.time / 1000.0 );
    CG_Draw3DModel( CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE,
             cg_weapons[ cent->currentState.weapon ].ammoModel, 0, origin, angles );
  }*/

  //CG_DrawStatusBarHead( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE );

  /*if (cg.predictedPlayerState.powerups[PW_REDFLAG])
    CG_DrawStatusBarFlag( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE + ICON_SIZE, TEAM_HUMANS);
  else if (cg.predictedPlayerState.powerups[PW_BLUEFLAG])
    CG_DrawStatusBarFlag( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE + ICON_SIZE, TEAM_DROIDS);*/

  /*if ( ps->stats[ STAT_ARMOR ] ) {
    origin[0] = 90;
    origin[1] = 0;
    origin[2] = -10;
    angles[YAW] = ( cg.time & 2047 ) * 360 / 2048.0;
    CG_Draw3DModel( 370 + CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE,
             cgs.media.armorModel, 0, origin, angles );
  }*/

  //
  // ammo
  //
  if ( cent->currentState.weapon ) {
    //TA: must mask off clips and maxClips
    if( !BG_infiniteAmmo( cent->currentState.weapon ) )
      BG_unpackAmmoArray( cent->currentState.weapon, ps->ammo, ps->powerups, &ammo, &clips, &maxclips );
    else
      ammo = -1;

    if ( ammo > -1 ) {
      if ( cg.predictedPlayerState.weaponstate == WEAPON_FIRING
        && cg.predictedPlayerState.weaponTime > 100 ) {
        // draw as dark grey when reloading
        color = 2;  // dark grey
      } else {
        if ( ammo >= 0 ) {
          color = 0;  // green
        } else {
          color = 1;  // red
        }
      }
      trap_R_SetColor( colors[color] );

      CG_DrawField( 85, 432, 3, ammo);

      if( maxclips )
        CG_DrawField( 20, 432, 1, clips );
      
      trap_R_SetColor( NULL );

      // if we didn't draw a 3D icon, draw a 2D icon for ammo
      /*if ( !cg_draw3dIcons.integer && cg_drawIcons.integer ) {
        qhandle_t icon;

        icon = cg_weapons[ cg.predictedPlayerState.weapon ].ammoIcon;
        if ( icon ) {
          CG_DrawPic( CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE, icon );
        }
      }*/
    }
  }

  //
  // stamina
  //
  #define STAM_HEIGHT 20
  #define STAM_WIDTH  10
  #define STAM_X      5
  #define STAM_Y      435
  if( ps->stats[ STAT_PTEAM ] == PTE_HUMANS )
  {
    int stamina = ps->stats[ STAT_STAMINA ];
    int height = (int)( (float)stamina / ( 1000 / STAM_HEIGHT ) );
    vec4_t bcolor = { 0.5, 0.5, 0.5, 0.5 };

    trap_R_SetColor( bcolor );   // white
    CG_DrawPic( STAM_X, STAM_Y, STAM_WIDTH, STAM_HEIGHT * 2, cgs.media.whiteShader );

    if( stamina > 0 )
    {
      trap_R_SetColor( colors[0] ); // green
      CG_DrawPic( STAM_X, STAM_Y + ( STAM_HEIGHT - height ),
                  STAM_WIDTH, height, cgs.media.whiteShader );
    }
    
    if( stamina < 0 )
    {
      trap_R_SetColor( colors[1] ); // red
      CG_DrawPic( STAM_X, STAM_Y + STAM_HEIGHT , STAM_WIDTH, -height, cgs.media.whiteShader );
    }
  }

  //
  // health+armor
  //
  if( ps->stats[ STAT_PTEAM ] == PTE_DROIDS )
  {
    vec4_t fcolor = { 1, 0, 0, 0.5 }; //red half alpha
    vec4_t tcolor = { 0.3, 0.8, 1, 1 }; //cyan no alpha

    value = (int)( (float)( (float)ps->stats[STAT_HEALTH] / ps->stats[STAT_MAX_HEALTH] ) * 100 );

    CG_DrawFadePic( 20, 0, 30, 440, fcolor, tcolor, value, cgs.media.droidHealth );

    value = (int)( (float)( (float)ps->stats[STAT_ARMOR] / ps->stats[STAT_MAX_HEALTH] ) * 100 );

    if( value > 0 )
      CG_DrawFadePic( 580, 0, 30, 440, fcolor, tcolor, value, cgs.media.droidHealth );
  }
  else
  {
    value = ps->stats[STAT_HEALTH];
    if ( value > 100 ) {
      trap_R_SetColor( colors[0] );   // white
    } else if (value > 25) {
      trap_R_SetColor( colors[0] ); // green
    } else if (value > 0) {
      color = (cg.time >> 8) & 1; // flash
      trap_R_SetColor( colors[color] );
    } else {
      trap_R_SetColor( colors[1] ); // red
    }
    
    // stretch the health up when taking damage
    CG_DrawField ( 300, 432, 3, value);
    CG_ColorForHealth( hcolor );
    trap_R_SetColor( hcolor );

    value = ps->stats[STAT_ARMOR];
    if (value > 0 )
    {
      trap_R_SetColor( colors[0] );
      CG_DrawField (541, 432, 3, value);
      trap_R_SetColor( NULL );
      // if we didn't draw a 3D icon, draw a 2D icon for armor
      /*if ( !cg_draw3dIcons.integer && cg_drawIcons.integer ) {
        CG_DrawPic( 370 + CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE, cgs.media.armorIcon );
      }*/
    }
  }
  
}

/*
===========================================================================================

  UPPER RIGHT CORNER

===========================================================================================
*/

/*
================
CG_DrawAttacker

================
*/
static float CG_DrawAttacker( float y ) {
  int     t;
  float   size;
  vec3_t    angles;
  const char  *info;
  const char  *name;
  int     clientNum;

  if ( cg.predictedPlayerState.stats[STAT_HEALTH] <= 0 ) {
    return y;
  }

  if ( !cg.attackerTime ) {
    return y;
  }

  clientNum = cg.predictedPlayerState.persistant[PERS_ATTACKER];
  if ( clientNum < 0 || clientNum >= MAX_CLIENTS || clientNum == cg.snap->ps.clientNum ) {
    return y;
  }

  t = cg.time - cg.attackerTime;
  if ( t > ATTACKER_HEAD_TIME ) {
    cg.attackerTime = 0;
    return y;
  }

  size = ICON_SIZE * 1.25;

  angles[PITCH] = 0;
  angles[YAW] = 180;
  angles[ROLL] = 0;
  CG_DrawHead( 640 - size, y, size, size, clientNum, angles );

  info = CG_ConfigString( CS_PLAYERS + clientNum );
  name = Info_ValueForKey(  info, "n" );
  y += size;
  CG_DrawBigString( 640 - ( Q_PrintStrlen( name ) * BIGCHAR_WIDTH), y, name, 0.5 );

  return y + BIGCHAR_HEIGHT + 2;
}

/*
==================
CG_DrawSnapshot
==================
*/
static float CG_DrawSnapshot( float y ) {
  char    *s;
  int     w;

  s = va( "time:%i snap:%i cmd:%i", cg.snap->serverTime,
    cg.latestSnapshotNum, cgs.serverCommandSequence );
  w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

  CG_DrawBigString( 635 - w, y + 2, s, 1.0F);

  return y + BIGCHAR_HEIGHT + 4;
}

/*
==================
CG_DrawFPS
==================
*/
#define FPS_FRAMES  4
static float CG_DrawFPS( float y ) {
  char    *s;
  int     w;
  static int  previousTimes[FPS_FRAMES];
  static int  index;
  int   i, total;
  int   fps;
  static  int previous;
  int   t, frameTime;

  // don't use serverTime, because that will be drifting to
  // correct for internet lag changes, timescales, timedemos, etc
  t = trap_Milliseconds();
  frameTime = t - previous;
  previous = t;

  previousTimes[index % FPS_FRAMES] = frameTime;
  index++;
  if ( index > FPS_FRAMES ) {
    // average multiple frames together to smooth changes out a bit
    total = 0;
    for ( i = 0 ; i < FPS_FRAMES ; i++ ) {
      total += previousTimes[i];
    }
    if ( !total ) {
      total = 1;
    }
    fps = 1000 * FPS_FRAMES / total;

    s = va( "%ifps", fps );
    w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

    CG_DrawBigString( 635 - w, y + 2, s, 1.0F);
  }

  return y + BIGCHAR_HEIGHT + 4;
}

/*
=================
CG_DrawTimer
=================
*/
static float CG_DrawTimer( float y ) {
  char    *s;
  int     w;
  int     mins, seconds, tens;
  int     msec;

  msec = cg.time - cgs.levelStartTime;

  seconds = msec / 1000;
  mins = seconds / 60;
  seconds -= mins * 60;
  tens = seconds / 10;
  seconds -= tens * 10;

  s = va( "%i:%i%i", mins, tens, seconds );
  w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

  CG_DrawBigString( 635 - w, y + 2, s, 1.0F);

  return y + BIGCHAR_HEIGHT + 4;
}


/*
=================
CG_DrawTeamOverlay
=================
*/

static float CG_DrawTeamOverlay( float y, qboolean right, qboolean upper ) {
  int x, w, h, xx;
  int i, j, len;
  const char *p;
  vec4_t    hcolor;
  int pwidth, lwidth;
  int plyrs;
  char st[16];
  clientInfo_t *ci;
  gitem_t *item;
  int ret_y, count;

  if ( !cg_drawTeamOverlay.integer ) {
    return y;
  }

  if ( cg.snap->ps.persistant[PERS_TEAM] != TEAM_HUMANS &&
    cg.snap->ps.persistant[PERS_TEAM] != TEAM_DROIDS ) {
    return y; // Not on any team
  }

  plyrs = 0;

  // max player name width
  pwidth = 0;
  count = (numSortedTeamPlayers > 8) ? 8 : numSortedTeamPlayers;
  for (i = 0; i < count; i++) {
    ci = cgs.clientinfo + sortedTeamPlayers[i];
    if ( ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
      plyrs++;
      len = CG_DrawStrlen(ci->name);
      if (len > pwidth)
        pwidth = len;
    }
  }

  if (!plyrs)
    return y;

  if (pwidth > TEAM_OVERLAY_MAXNAME_WIDTH)
    pwidth = TEAM_OVERLAY_MAXNAME_WIDTH;

  // max location name width
  lwidth = 0;
  for (i = 1; i < MAX_LOCATIONS; i++) {
    p = CG_ConfigString(CS_LOCATIONS + i);
    if (p && *p) {
      len = CG_DrawStrlen(p);
      if (len > lwidth)
        lwidth = len;
    }
  }

  if (lwidth > TEAM_OVERLAY_MAXLOCATION_WIDTH)
    lwidth = TEAM_OVERLAY_MAXLOCATION_WIDTH;

  w = (pwidth + lwidth + 4 + 7) * TINYCHAR_WIDTH;

  if ( right )
    x = 640 - w;
  else
    x = 0;

  h = plyrs * TINYCHAR_HEIGHT;

  if ( upper ) {
    ret_y = y + h;
  } else {
    y -= h;
    ret_y = y;
  }

  if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_HUMANS ) {
    hcolor[0] = 1;
    hcolor[1] = 0;
    hcolor[2] = 0;
    hcolor[3] = 0.33f;
  } else { // if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_DROIDS )
    hcolor[0] = 0;
    hcolor[1] = 0;
    hcolor[2] = 1;
    hcolor[3] = 0.33f;
  }
  trap_R_SetColor( hcolor );
  CG_DrawPic( x, y, w, h, cgs.media.teamStatusBar );
  trap_R_SetColor( NULL );

  for (i = 0; i < count; i++) {
    ci = cgs.clientinfo + sortedTeamPlayers[i];
    if ( ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {

      hcolor[0] = hcolor[1] = hcolor[2] = hcolor[3] = 1.0;

      xx = x + TINYCHAR_WIDTH;

      CG_DrawStringExt( xx, y,
        ci->name, hcolor, qfalse, qfalse,
        TINYCHAR_WIDTH, TINYCHAR_HEIGHT, TEAM_OVERLAY_MAXNAME_WIDTH);

      if (lwidth) {
        p = CG_ConfigString(CS_LOCATIONS + ci->location);
        if (!p || !*p)
          p = "unknown";
        len = CG_DrawStrlen(p);
        if (len > lwidth)
          len = lwidth;

//        xx = x + TINYCHAR_WIDTH * 2 + TINYCHAR_WIDTH * pwidth +
//          ((lwidth/2 - len/2) * TINYCHAR_WIDTH);
        xx = x + TINYCHAR_WIDTH * 2 + TINYCHAR_WIDTH * pwidth;
        CG_DrawStringExt( xx, y,
          p, hcolor, qfalse, qfalse, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
          TEAM_OVERLAY_MAXLOCATION_WIDTH);
      }

      CG_GetColorForHealth( ci->health, ci->armor, hcolor );

      Com_sprintf (st, sizeof(st), "%3i %3i", ci->health, ci->armor);

      xx = x + TINYCHAR_WIDTH * 3 +
        TINYCHAR_WIDTH * pwidth + TINYCHAR_WIDTH * lwidth;

      CG_DrawStringExt( xx, y,
        st, hcolor, qfalse, qfalse,
        TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );

      // draw weapon icon
      xx += TINYCHAR_WIDTH * 3;

      if ( cg_weapons[ci->curWeapon].weaponIcon ) {
        CG_DrawPic( xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
          cg_weapons[ci->curWeapon].weaponIcon );
      } else {
        CG_DrawPic( xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
          cgs.media.deferShader );
      }

      // Draw powerup icons
      if (right) {
        xx = x;
      } else {
        xx = x + w - TINYCHAR_WIDTH;
      }
      /*for (j = 0; j < PW_NUM_POWERUPS; j++) {
        if (ci->powerups & (1 << j)) {
          gitem_t *item;

          item = BG_FindItemForPowerup( j );

          CG_DrawPic( xx, y, TINYCHAR_WIDTH, TINYCHAR_HEIGHT,
            trap_R_RegisterShader( item->icon ) );
          if (right) {
            xx -= TINYCHAR_WIDTH;
          } else {
            xx += TINYCHAR_WIDTH;
          }
        }
      }*/

      y += TINYCHAR_HEIGHT;
    }
  }

  return ret_y;
}


/*
=====================
CG_DrawUpperRight

=====================
*/
static void CG_DrawUpperRight( void ) {
  float y;

  y = 0;

  if ( cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 1 ) {
    y = CG_DrawTeamOverlay( y, qtrue, qtrue );
  }
  if ( cg_drawSnapshot.integer ) {
    y = CG_DrawSnapshot( y );
  }
  if ( cg_drawFPS.integer ) {
    y = CG_DrawFPS( y );
  }
  if ( cg_drawTimer.integer ) {
    y = CG_DrawTimer( y );
  }
  if ( cg_drawAttacker.integer ) {
    y = CG_DrawAttacker( y );
  }

}

/*
===========================================================================================

  LOWER RIGHT CORNER

===========================================================================================
*/


/*
=================
CG_DrawPoints

Draw the small two score display
=================
*/
static float CG_DrawPoints( float y )
{
  const char  *s;
  int     points, totalpoints, buildpoints;
  int     team;
  int     x, w;
  float   y1;
  qboolean  spectator;

  y -= BIGCHAR_HEIGHT + 8;

  y1 = y;
 

  x = 640;
  points = cg.snap->ps.persistant[PERS_POINTS];
  totalpoints = cg.snap->ps.persistant[PERS_TOTALPOINTS];

  team = cg.snap->ps.stats[ STAT_PTEAM ];
  
  if( team == PTE_DROIDS )
    buildpoints = cgs.aBuildPoints;
  else if( team == PTE_HUMANS )
    buildpoints = cgs.hBuildPoints;

  spectator = ( team == PTE_NONE );

  if( !spectator )
  {
    s = va( "%2i", points );
    w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
    x -= w;

    CG_DrawBigString( x + 2, y, s, 1.0F );

    s = va( "%2i", totalpoints );
    w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
    x -= w;

    CG_DrawBigString( x + 2, y, s, 1.0F );

    s = va( "%2i", buildpoints );
    w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
    x -= w;

    CG_DrawBigString( x + 2, y, s, 1.0F );
  }

  return y1 - 8;
}


/*
=================
CG_DrawScores

Draw the small two score display
=================
*/
static float CG_DrawScores( float y ) {
  const char  *s;
  int     s1, s2, score;
  int     x, w;
  int     v;
  vec4_t    color;
  float   y1;
  gitem_t   *item;

  s1 = cgs.scores1;
  s2 = cgs.scores2;

  y -=  BIGCHAR_HEIGHT + 8;

  y1 = y;

  // draw from the right side to left
  if ( cgs.gametype >= GT_TEAM ) {
    x = 640;

    color[0] = 0;
    color[1] = 0;
    color[2] = 1;
    color[3] = 0.33f;
    s = va( "%2i", s2 );
    w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
    x -= w;
    CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
    if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_DROIDS ) {
      CG_DrawPic( x, y-4, w, BIGCHAR_HEIGHT+8, cgs.media.selectShader );
    }
    CG_DrawBigString( x + 4, y, s, 1.0F);

    if ( cgs.gametype == GT_CTF ) {
      // Display flag status
      item = BG_FindItemForPowerup( PW_BLUEFLAG );

      if (item) {
        y1 = y - BIGCHAR_HEIGHT - 8;
        if( cgs.blueflag >= 0 && cgs.blueflag <= 2 ) {
          CG_DrawPic( x, y1-4, w, BIGCHAR_HEIGHT+8, cgs.media.blueFlagShader[cgs.blueflag] );
        }
      }
    }

    color[0] = 1;
    color[1] = 0;
    color[2] = 0;
    color[3] = 0.33f;
    s = va( "%2i", s1 );
    w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
    x -= w;
    CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
    if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_HUMANS ) {
      CG_DrawPic( x, y-4, w, BIGCHAR_HEIGHT+8, cgs.media.selectShader );
    }
    CG_DrawBigString( x + 4, y, s, 1.0F);

    if ( cgs.gametype == GT_CTF ) {
      // Display flag status
      item = BG_FindItemForPowerup( PW_REDFLAG );

      if (item) {
        y1 = y - BIGCHAR_HEIGHT - 8;
        if( cgs.redflag >= 0 && cgs.redflag <= 2 ) {
          CG_DrawPic( x, y1-4, w, BIGCHAR_HEIGHT+8, cgs.media.redFlagShader[cgs.redflag] );
        }
      }
    }


    if ( cgs.gametype >= GT_CTF ) {
      v = cgs.capturelimit;
    } else {
      v = cgs.fraglimit;
    }
    if ( v ) {
      s = va( "%2i", v );
      w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
      x -= w;
      CG_DrawBigString( x + 4, y, s, 1.0F);
    }

  } else {
    qboolean  spectator;

    x = 640;
    score = cg.snap->ps.persistant[PERS_SCORE];
    spectator = ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR );

    // always show your score in the second box if not in first place
    if ( s1 != score ) {
      s2 = score;
    }
    if ( s2 != SCORE_NOT_PRESENT ) {
      s = va( "%2i", s2 );
      w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
      x -= w;
      if ( !spectator && score == s2 && score != s1 ) {
        color[0] = 1;
        color[1] = 0;
        color[2] = 0;
        color[3] = 0.33f;
        CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
        CG_DrawPic( x, y-4, w, BIGCHAR_HEIGHT+8, cgs.media.selectShader );
      } else {
        color[0] = 0.5f;
        color[1] = 0.5f;
        color[2] = 0.5f;
        color[3] = 0.33f;
        CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
      }
      CG_DrawBigString( x + 4, y, s, 1.0F);
    }

    // first place
    if ( s1 != SCORE_NOT_PRESENT ) {
      s = va( "%2i", s1 );
      w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
      x -= w;
      if ( !spectator && score == s1 ) {
        color[0] = 0;
        color[1] = 0;
        color[2] = 1;
        color[3] = 0.33f;
        CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
        CG_DrawPic( x, y-4, w, BIGCHAR_HEIGHT+8, cgs.media.selectShader );
      } else {
        color[0] = 0.5f;
        color[1] = 0.5f;
        color[2] = 0.5f;
        color[3] = 0.33f;
        CG_FillRect( x, y-4,  w, BIGCHAR_HEIGHT+8, color );
      }
      CG_DrawBigString( x + 4, y, s, 1.0F);
    }

    if ( cgs.fraglimit ) {
      s = va( "%2i", cgs.fraglimit );
      w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
      x -= w;
      CG_DrawBigString( x + 4, y, s, 1.0F);
    }

  }

  return y1 - 8;
}

/*
================
CG_DrawPowerups
================
*/
static float CG_DrawPowerups( float y ) {
  int   sorted[MAX_POWERUPS];
  int   sortedTime[MAX_POWERUPS];
  int   i, j, k;
  int   active;
  playerState_t *ps;
  int   t;
  gitem_t *item;
  int   x;
  int   color;
  float size;
  float f;
  static float colors[2][4] = {
    { 0.2f, 1.0f, 0.2f, 1.0f } , { 1.0f, 0.2f, 0.2f, 1.0f } };

  ps = &cg.snap->ps;

  if ( ps->stats[STAT_HEALTH] <= 0 ) {
    return y;
  }

  // sort the list by time remaining
  active = 0;
  for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
    if ( !ps->powerups[ i ] ) {
      continue;
    }
    t = ps->powerups[ i ] - cg.time;
    // ZOID--don't draw if the power up has unlimited time (999 seconds)
    // This is true of the CTF flags
    if ( t < 0 || t > 999000) {
      continue;
    }

    // insert into the list
    for ( j = 0 ; j < active ; j++ ) {
      if ( sortedTime[j] >= t ) {
        for ( k = active - 1 ; k >= j ; k-- ) {
          sorted[k+1] = sorted[k];
          sortedTime[k+1] = sortedTime[k];
        }
        break;
      }
    }
    sorted[j] = i;
    sortedTime[j] = t;
    active++;
  }

  // draw the icons and timers
  x = 640 - ICON_SIZE - CHAR_WIDTH * 2;
  for ( i = 0 ; i < active ; i++ ) {
    item = BG_FindItemForPowerup( sorted[i] );

    if (item) {

      color = 1;

      y -= ICON_SIZE;

      trap_R_SetColor( colors[color] );
      CG_DrawField( x, y, 2, sortedTime[ i ] / 1000 );

      t = ps->powerups[ sorted[i] ];
      if ( t - cg.time >= POWERUP_BLINKS * POWERUP_BLINK_TIME ) {
        trap_R_SetColor( NULL );
      } else {
        vec4_t  modulate;

        f = (float)( t - cg.time ) / POWERUP_BLINK_TIME;
        f -= (int)f;
        modulate[0] = modulate[1] = modulate[2] = modulate[3] = f;
        trap_R_SetColor( modulate );
      }

      if ( cg.powerupActive == sorted[i] && 
        cg.time - cg.powerupTime < PULSE_TIME ) {
        f = 1.0 - ( ( (float)cg.time - cg.powerupTime ) / PULSE_TIME );
        size = ICON_SIZE * ( 1.0 + ( PULSE_SCALE - 1.0 ) * f );
      } else {
        size = ICON_SIZE;
      }

      CG_DrawPic( 640 - size, y + ICON_SIZE / 2 - size / 2, 
        size, size, trap_R_RegisterShader( item->icon ) );
    }
  }
  trap_R_SetColor( NULL );

  return y;
}


/*
=====================
CG_DrawLowerRight

=====================
*/
static void CG_DrawLowerRight( void ) {
  float y;

  y = 480 - ICON_SIZE;

  if ( cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 2 ) {
    y = CG_DrawTeamOverlay( y, qtrue, qfalse );
  }

  //y = CG_DrawScores( y );
  //y = CG_DrawPoints( y );
  y = CG_DrawPowerups( y );
}

/*
===================
CG_DrawPickupItem
===================
*/
static int CG_DrawPickupItem( int y ) {
  int   value;
  float *fadeColor;

  if ( cg.snap->ps.stats[STAT_HEALTH] <= 0 ) {
    return y;
  }

  y -= ICON_SIZE;

  value = cg.itemPickup;
  if ( value ) {
    fadeColor = CG_FadeColor( cg.itemPickupTime, 3000 );
    if ( fadeColor ) {
      CG_RegisterItemVisuals( value );
      trap_R_SetColor( fadeColor );
      CG_DrawPic( 8, y, ICON_SIZE, ICON_SIZE, cg_items[ value ].icon );
      CG_DrawBigString( ICON_SIZE + 16, y + (ICON_SIZE/2 - BIGCHAR_HEIGHT/2), bg_itemlist[ value ].pickup_name, fadeColor[0] );
      trap_R_SetColor( NULL );
    }
  }

  return y;
}

/*
=====================
CG_DrawLowerLeft

=====================
*/
static void CG_DrawLowerLeft( void ) {
  float y;

  y = 480 - ICON_SIZE;

  if ( cgs.gametype >= GT_TEAM && cg_drawTeamOverlay.integer == 3 ) {
    y = CG_DrawTeamOverlay( y, qfalse, qfalse );
  }


  y = CG_DrawPickupItem( y );
}



//===========================================================================================

/*
=================
CG_DrawTeamInfo
=================
*/
static void CG_DrawTeamInfo( void ) {
  int w, h;
  int i, len;
  vec4_t    hcolor;
  int   chatHeight;

#define CHATLOC_Y 420 // bottom end
#define CHATLOC_X 0

  if (cg_teamChatHeight.integer < TEAMCHAT_HEIGHT)
    chatHeight = cg_teamChatHeight.integer;
  else
    chatHeight = TEAMCHAT_HEIGHT;
  if (chatHeight <= 0)
    return; // disabled

  if (cgs.teamLastChatPos != cgs.teamChatPos) {
    if (cg.time - cgs.teamChatMsgTimes[cgs.teamLastChatPos % chatHeight] > cg_teamChatTime.integer) {
      cgs.teamLastChatPos++;
    }

    h = (cgs.teamChatPos - cgs.teamLastChatPos) * TINYCHAR_HEIGHT;

    w = 0;

    for (i = cgs.teamLastChatPos; i < cgs.teamChatPos; i++) {
      len = CG_DrawStrlen(cgs.teamChatMsgs[i % chatHeight]);
      if (len > w)
        w = len;
    }
    w *= TINYCHAR_WIDTH;
    w += TINYCHAR_WIDTH * 2;

    if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_HUMANS ) {
      hcolor[0] = 1;
      hcolor[1] = 0;
      hcolor[2] = 0;
      hcolor[3] = 0.33f;
    } else if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_DROIDS ) {
      hcolor[0] = 0;
      hcolor[1] = 0;
      hcolor[2] = 1;
      hcolor[3] = 0.33f;
    } else {
      hcolor[0] = 0;
      hcolor[1] = 1;
      hcolor[2] = 0;
      hcolor[3] = 0.33f;
    }

    trap_R_SetColor( hcolor );
    CG_DrawPic( CHATLOC_X, CHATLOC_Y - h, 640, h, cgs.media.teamStatusBar );
    trap_R_SetColor( NULL );

    hcolor[0] = hcolor[1] = hcolor[2] = 1.0;
    hcolor[3] = 1.0;

    for (i = cgs.teamChatPos - 1; i >= cgs.teamLastChatPos; i--) {
      CG_DrawStringExt( CHATLOC_X + TINYCHAR_WIDTH,
        CHATLOC_Y - (cgs.teamChatPos - i)*TINYCHAR_HEIGHT,
        cgs.teamChatMsgs[i % chatHeight], hcolor, qfalse, qfalse,
        TINYCHAR_WIDTH, TINYCHAR_HEIGHT, 0 );
    }
  }
}

/*
===================
CG_DrawHoldableItem
===================
*/
static void CG_DrawHoldableItem( void ) {
  int   value;

  //TA: not using q3 holdable item code
  /*value = cg.snap->ps.stats[STAT_HOLDABLE_ITEM];
  if ( value ) {
    CG_RegisterItemVisuals( value );
    CG_DrawPic( 640-ICON_SIZE, (SCREEN_HEIGHT-ICON_SIZE)/2, ICON_SIZE, ICON_SIZE, cg_items[ value ].icon );
  }*/

}


/*
===================
CG_DrawReward
===================
*/
static void CG_DrawReward( void ) {
  float *color;
  int   i, count;
  float x, y;
  char  buf[32];

  if ( !cg_drawRewards.integer ) {
    return;
  }
  color = CG_FadeColor( cg.rewardTime, REWARD_TIME );
  if ( !color ) {
    if (cg.rewardStack > 0) {
      for(i = 0; i < cg.rewardStack; i++) {
        cg.rewardSound[i] = cg.rewardSound[i+1];
        cg.rewardShader[i] = cg.rewardShader[i+1];
        cg.rewardCount[i] = cg.rewardCount[i+1];
      }
      cg.rewardTime = cg.time;
      cg.rewardStack--;
      color = CG_FadeColor( cg.rewardTime, REWARD_TIME );
      trap_S_StartLocalSound(cg.rewardSound[0], CHAN_ANNOUNCER);
    } else {
      return;
    }
  }

  trap_R_SetColor( color );

  if ( cg.rewardCount[0] >= 10 ) {
    y = 56;
    x = 320 - ICON_SIZE/2;
    CG_DrawPic( x, y, ICON_SIZE-4, ICON_SIZE-4, cg.rewardShader[0] );
    Com_sprintf(buf, sizeof(buf), "%d", cg.rewardCount[0]);
    x = ( SCREEN_WIDTH - SMALLCHAR_WIDTH * CG_DrawStrlen( buf ) ) / 2;
    CG_DrawStringExt( x, y+ICON_SIZE, buf, color, qfalse, qtrue,
                SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
  }
  else {

    count = cg.rewardCount[0];

    y = 56;
    x = 320 - count * ICON_SIZE/2;
    for ( i = 0 ; i < count ; i++ ) {
      CG_DrawPic( x, y, ICON_SIZE-4, ICON_SIZE-4, cg.rewardShader[0] );
      x += ICON_SIZE;
    }
  }
  trap_R_SetColor( NULL );
}


/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define LAG_SAMPLES   128


typedef struct {
  int   frameSamples[LAG_SAMPLES];
  int   frameCount;
  int   snapshotFlags[LAG_SAMPLES];
  int   snapshotSamples[LAG_SAMPLES];
  int   snapshotCount;
} lagometer_t;

lagometer_t   lagometer;

/*
==============
CG_AddLagometerFrameInfo

Adds the current interpolate / extrapolate bar for this frame
==============
*/
void CG_AddLagometerFrameInfo( void ) {
  int     offset;

  offset = cg.time - cg.latestSnapshotTime;
  lagometer.frameSamples[ lagometer.frameCount & ( LAG_SAMPLES - 1) ] = offset;
  lagometer.frameCount++;
}

/*
==============
CG_AddLagometerSnapshotInfo

Each time a snapshot is received, log its ping time and
the number of snapshots that were dropped before it.

Pass NULL for a dropped packet.
==============
*/
void CG_AddLagometerSnapshotInfo( snapshot_t *snap ) {
  // dropped packet
  if ( !snap ) {
    lagometer.snapshotSamples[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = -1;
    lagometer.snapshotCount++;
    return;
  }

  // add this snapshot's info
  lagometer.snapshotSamples[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = snap->ping;
  lagometer.snapshotFlags[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = snap->snapFlags;
  lagometer.snapshotCount++;
}

/*
==============
CG_DrawDisconnect

Should we draw something differnet for long lag vs no packets?
==============
*/
static void CG_DrawDisconnect( void ) {
  float   x, y;
  int     cmdNum;
  usercmd_t cmd;
  const char    *s;
  int     w;

  // draw the phone jack if we are completely past our buffers
  cmdNum = trap_GetCurrentCmdNumber() - CMD_BACKUP + 1;
  trap_GetUserCmd( cmdNum, &cmd );
  if ( cmd.serverTime <= cg.snap->ps.commandTime
    || cmd.serverTime > cg.time ) { // special check for map_restart
    return;
  }

  // also add text in center of screen
  s = "Connection Interrupted";
  w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
  CG_DrawBigString( 320 - w/2, 100, s, 1.0F);

  // blink the icon
  if ( ( cg.time >> 9 ) & 1 ) {
    return;
  }

  x = 640 - 48;
  y = 480 - 48;

  CG_DrawPic( x, y, 48, 48, trap_R_RegisterShader("gfx/2d/net.tga" ) );
}


#define MAX_LAGOMETER_PING  900
#define MAX_LAGOMETER_RANGE 300

/*
==============
CG_DrawLagometer
==============
*/
static void CG_DrawLagometer( void ) {
  int   a, x, y, i;
  float v;
  float ax, ay, aw, ah, mid, range;
  int   color;
  float vscale;

  if ( !cg_lagometer.integer || cgs.localServer ) {
    CG_DrawDisconnect();
    return;
  }

  //
  // draw the graph
  //
  x = 640 - 48;
  y = 480 - 48;

  trap_R_SetColor( NULL );
  CG_DrawPic( x, y, 48, 48, cgs.media.lagometerShader );

  ax = x;
  ay = y;
  aw = 48;
  ah = 48;
  CG_AdjustFrom640( &ax, &ay, &aw, &ah );

  color = -1;
  range = ah / 3;
  mid = ay + range;

  vscale = range / MAX_LAGOMETER_RANGE;

  // draw the frame interpoalte / extrapolate graph
  for ( a = 0 ; a < aw ; a++ ) {
    i = ( lagometer.frameCount - 1 - a ) & (LAG_SAMPLES - 1);
    v = lagometer.frameSamples[i];
    v *= vscale;
    if ( v > 0 ) {
      if ( color != 1 ) {
        color = 1;
        trap_R_SetColor( g_color_table[ColorIndex(COLOR_YELLOW)] );
      }
      if ( v > range ) {
        v = range;
      }
      trap_R_DrawStretchPic ( ax + aw - a, mid - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
    } else if ( v < 0 ) {
      if ( color != 2 ) {
        color = 2;
        trap_R_SetColor( g_color_table[ColorIndex(COLOR_BLUE)] );
      }
      v = -v;
      if ( v > range ) {
        v = range;
      }
      trap_R_DrawStretchPic( ax + aw - a, mid, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
    }
  }

  // draw the snapshot latency / drop graph
  range = ah / 2;
  vscale = range / MAX_LAGOMETER_PING;

  for ( a = 0 ; a < aw ; a++ ) {
    i = ( lagometer.snapshotCount - 1 - a ) & (LAG_SAMPLES - 1);
    v = lagometer.snapshotSamples[i];
    if ( v > 0 ) {
      if ( lagometer.snapshotFlags[i] & SNAPFLAG_RATE_DELAYED ) {
        if ( color != 5 ) {
          color = 5;  // YELLOW for rate delay
          trap_R_SetColor( g_color_table[ColorIndex(COLOR_YELLOW)] );
        }
      } else {
        if ( color != 3 ) {
          color = 3;
          trap_R_SetColor( g_color_table[ColorIndex(COLOR_GREEN)] );
        }
      }
      v = v * vscale;
      if ( v > range ) {
        v = range;
      }
      trap_R_DrawStretchPic( ax + aw - a, ay + ah - v, 1, v, 0, 0, 0, 0, cgs.media.whiteShader );
    } else if ( v < 0 ) {
      if ( color != 4 ) {
        color = 4;    // RED for dropped snapshots
        trap_R_SetColor( g_color_table[ColorIndex(COLOR_RED)] );
      }
      trap_R_DrawStretchPic( ax + aw - a, ay + ah - range, 1, range, 0, 0, 0, 0, cgs.media.whiteShader );
    }
  }

  trap_R_SetColor( NULL );

  if ( cg_nopredict.integer || cg_synchronousClients.integer ) {
    CG_DrawBigString( ax, ay, "snc", 1.0 );
  }

  CG_DrawDisconnect();
}



/*
===============================================================================

CENTER PRINTING

===============================================================================
*/


/*
==============
CG_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_CenterPrint( const char *str, int y, int charWidth ) {
  char  *s;

  Q_strncpyz( cg.centerPrint, str, sizeof(cg.centerPrint) );

  cg.centerPrintTime = cg.time;
  cg.centerPrintY = y;
  cg.centerPrintCharWidth = charWidth;

  // count the number of lines for centering
  cg.centerPrintLines = 1;
  s = cg.centerPrint;
  while( *s ) {
    if (*s == '\n')
      cg.centerPrintLines++;
    s++;
  }
}


/*
===================
CG_DrawCenterString
===================
*/
static void CG_DrawCenterString( void ) {
  char  *start;
  int   l;
  int   x, y, w;
  float *color;

  if ( !cg.centerPrintTime ) {
    return;
  }

  color = CG_FadeColor( cg.centerPrintTime, 1000 * cg_centertime.value );
  if ( !color ) {
    return;
  }

  trap_R_SetColor( color );

  start = cg.centerPrint;

  y = cg.centerPrintY - cg.centerPrintLines * BIGCHAR_HEIGHT / 2;

  while ( 1 ) {
    char linebuffer[1024];

    for ( l = 0; l < 50; l++ ) {
      if ( !start[l] || start[l] == '\n' ) {
        break;
      }
      linebuffer[l] = start[l];
    }
    linebuffer[l] = 0;

    w = cg.centerPrintCharWidth * CG_DrawStrlen( linebuffer );

    x = ( SCREEN_WIDTH - w ) / 2;

    CG_DrawStringExt( x, y, linebuffer, color, qfalse, qtrue,
      cg.centerPrintCharWidth, (int)(cg.centerPrintCharWidth * 1.5), 0 );

    y += cg.centerPrintCharWidth * 1.5;

    while ( *start && ( *start != '\n' ) ) {
      start++;
    }
    if ( !*start ) {
      break;
    }
    start++;
  }

  trap_R_SetColor( NULL );
}



/*
================================================================================

CROSSHAIR

================================================================================
*/


/*
=================
CG_DrawCrosshair
=================
*/
static void CG_DrawCrosshair(void) {
  float   w, h;
  qhandle_t hShader;
  float   f;
  float   x, y;
  int     ca;

  if ( !cg_drawCrosshair.integer ) {
    return;
  }

  if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
    return;
  }

  if ( cg.renderingThirdPerson ) {
    return;
  }

  // set color based on health
  if ( cg_crosshairHealth.integer ) {
    vec4_t    hcolor;

    CG_ColorForHealth( hcolor );
    trap_R_SetColor( hcolor );
  } else {
    trap_R_SetColor( NULL );
  }

  w = h = cg_crosshairSize.value;

  // pulse the size of the crosshair when picking up items
  f = cg.time - cg.itemPickupBlendTime;
  if ( f > 0 && f < ITEM_BLOB_TIME ) {
    f /= ITEM_BLOB_TIME;
    w *= ( 1 + f );
    h *= ( 1 + f );
  }

  x = cg_crosshairX.integer;
  y = cg_crosshairY.integer;
  CG_AdjustFrom640( &x, &y, &w, &h );

  ca = cg_drawCrosshair.integer;
  if (ca < 0) {
    ca = 0;
  }
  hShader = cgs.media.crosshairShader[ ca % NUM_CROSSHAIRS ];

  trap_R_DrawStretchPic( x + cg.refdef.x + 0.5 * (cg.refdef.width - w),
    y + cg.refdef.y + 0.5 * (cg.refdef.height - h),
    w, h, 0, 0, 1, 1, hShader );
}



/*
=================
CG_ScanForCrosshairEntity
=================
*/
static void CG_ScanForCrosshairEntity( void ) {
  trace_t   trace;
  vec3_t    start, end;
  int     content;

  VectorCopy( cg.refdef.vieworg, start );
  VectorMA( start, 131072, cg.refdef.viewaxis[0], end );

  CG_Trace( &trace, start, vec3_origin, vec3_origin, end,
    cg.snap->ps.clientNum, CONTENTS_SOLID|CONTENTS_BODY );
  if ( trace.entityNum >= MAX_CLIENTS ) {
    return;
  }

  // if the player is in fog, don't show it
  content = trap_CM_PointContents( trace.endpos, 0 );
  if ( content & CONTENTS_FOG ) {
    return;
  }

  // if the player is invisible, don't show it
  /*if ( cg_entities[ trace.entityNum ].currentState.powerups & ( 1 << PW_INVIS ) ) {
    return;
  }*/

  // update the fade timer
  cg.crosshairClientNum = trace.entityNum;
  cg.crosshairClientTime = cg.time;
}


/*
=====================
CG_DrawCrosshairNames
=====================
*/
static void CG_DrawCrosshairNames( void ) {
  float   *color;
  char    *name;
  float   w;

  if ( !cg_drawCrosshair.integer ) {
    return;
  }
  if ( !cg_drawCrosshairNames.integer ) {
    return;
  }
  if ( cg.renderingThirdPerson ) {
    return;
  }

  // scan the known entities to see if the crosshair is sighted on one
  CG_ScanForCrosshairEntity();

  // draw the name of the player being looked at
  color = CG_FadeColor( cg.crosshairClientTime, 1000 );
  if ( !color ) {
    trap_R_SetColor( NULL );
    return;
  }

  name = cgs.clientinfo[ cg.crosshairClientNum ].name;
  w = CG_DrawStrlen( name ) * BIGCHAR_WIDTH;
  CG_DrawBigString( 320 - w / 2, 170, name, color[3] * 0.5f );

  trap_R_SetColor( NULL );
}



//==============================================================================

/*
=================
CG_DrawSpectator
=================
*/
static void CG_DrawSpectator(void) {
  CG_DrawBigString(320 - 9 * 8, 440, "SPECTATOR", 1.0F);
  if ( cgs.gametype == GT_TOURNAMENT ) {
    CG_DrawBigString(320 - 15 * 8, 460, "waiting to play", 1.0F);
  }
  if ( cgs.gametype >= GT_TEAM ) {
    CG_DrawBigString(320 - 39 * 8, 460, "press ESC and use the JOIN menu to play", 1.0F);
  }
}

/*
=================
CG_DrawVote
=================
*/
static void CG_DrawVote(void) {
  char  *s;
  int   sec;

  if ( !cgs.voteTime ) {
    return;
  }

  // play a talk beep whenever it is modified
  if ( cgs.voteModified ) {
    cgs.voteModified = qfalse;
    trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
  }

  sec = ( VOTE_TIME - ( cg.time - cgs.voteTime ) ) / 1000;
  if ( sec < 0 ) {
    sec = 0;
  }
  s = va("VOTE(%i):%s yes:%i no:%i", sec, cgs.voteString, cgs.voteYes, cgs.voteNo );
  CG_DrawSmallString( 0, 58, s, 1.0F );
}

/*
=================
CG_DrawTeamVote
=================
*/
static void CG_DrawTeamVote(void) {
  char  *s;
  int   sec, cs_offset;

  if ( cgs.clientinfo->team == TEAM_HUMANS )
    cs_offset = 0;
  else if ( cgs.clientinfo->team == TEAM_DROIDS )
    cs_offset = 1;
  else
    return;

  if ( !cgs.teamVoteTime[cs_offset] ) {
    return;
  }

  // play a talk beep whenever it is modified
  if ( cgs.teamVoteModified[cs_offset] ) {
    cgs.teamVoteModified[cs_offset] = qfalse;
    trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
  }

  sec = ( VOTE_TIME - ( cg.time - cgs.teamVoteTime[cs_offset] ) ) / 1000;
  if ( sec < 0 ) {
    sec = 0;
  }
  s = va("TEAMVOTE(%i):%s yes:%i no:%i", sec, cgs.teamVoteString[cs_offset],
              cgs.teamVoteYes[cs_offset], cgs.teamVoteNo[cs_offset] );
  CG_DrawSmallString( 0, 90, s, 1.0F );
}


static qboolean CG_DrawScoreboard() {
  return CG_DrawOldScoreboard();
}

/*
=================
CG_DrawIntermission
=================
*/
static void CG_DrawIntermission( void ) {
  if ( cgs.gametype == GT_SINGLE_PLAYER ) {
    CG_DrawCenterString();
    return;
  }

  cg.scoreFadeTime = cg.time;
  cg.scoreBoardShowing = CG_DrawScoreboard();
}

/*
=================
CG_DrawFollow
=================
*/
static qboolean CG_DrawFollow( void ) {
  float   x;
  vec4_t    color;
  const char  *name;

  if ( !(cg.snap->ps.pm_flags & PMF_FOLLOW) ) {
    return qfalse;
  }
  color[0] = 1;
  color[1] = 1;
  color[2] = 1;
  color[3] = 1;


  CG_DrawBigString( 320 - 9 * 8, 24, "following", 1.0F );

  name = cgs.clientinfo[ cg.snap->ps.clientNum ].name;

  x = 0.5 * ( 640 - GIANT_WIDTH * CG_DrawStrlen( name ) );

  CG_DrawStringExt( x, 40, name, color, qtrue, qtrue, GIANT_WIDTH, GIANT_HEIGHT, 0 );

  return qtrue;
}



/*
=================
CG_DrawAmmoWarning
=================
*/
static void CG_DrawAmmoWarning( void ) {
  const char  *s;
  int     w;

  if ( cg_drawAmmoWarning.integer == 0 ) {
    return;
  }

  if ( !cg.lowAmmoWarning ) {
    return;
  }

  if ( cg.lowAmmoWarning == 2 ) {
    s = "OUT OF AMMO";
  } else {
    s = "LOW AMMO WARNING";
  }
  w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
  CG_DrawBigString(320 - w / 2, 64, s, 1.0F);
}

/*
=================
CG_DrawWarmup
=================
*/
static void CG_DrawWarmup( void ) {
  int     w;
  int     sec;
  int     i;
  float   scale;
  clientInfo_t  *ci1, *ci2;
  int     cw;
  const char  *s;

  sec = cg.warmup;
  if ( !sec ) {
    return;
  }

  if ( sec < 0 ) {
    s = "Waiting for players";
    w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
    CG_DrawBigString(320 - w / 2, 24, s, 1.0F);
    cg.warmupCount = 0;
    return;
  }

  if (cgs.gametype == GT_TOURNAMENT) {
    // find the two active players
    ci1 = NULL;
    ci2 = NULL;
    for ( i = 0 ; i < cgs.maxclients ; i++ ) {
      if ( cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_FREE ) {
        if ( !ci1 ) {
          ci1 = &cgs.clientinfo[i];
        } else {
          ci2 = &cgs.clientinfo[i];
        }
      }
    }

    if ( ci1 && ci2 ) {
      s = va( "%s vs %s", ci1->name, ci2->name );
      w = CG_DrawStrlen( s );
      if ( w > 640 / GIANT_WIDTH ) {
        cw = 640 / w;
      } else {
        cw = GIANT_WIDTH;
      }
      CG_DrawStringExt( 320 - w * cw/2, 20,s, colorWhite,
          qfalse, qtrue, cw, (int)(cw * 1.5f), 0 );
    }
  } else {
    if ( cgs.gametype == GT_FFA ) {
      s = "Free For All";
    } else if ( cgs.gametype == GT_TEAM ) {
      s = "Team Deathmatch";
    } else if ( cgs.gametype == GT_CTF ) {
      s = "Capture the Flag";
    } else {
      s = "";
    }
    w = CG_DrawStrlen( s );
    if ( w > 640 / GIANT_WIDTH ) {
      cw = 640 / w;
    } else {
      cw = GIANT_WIDTH;
    }
    CG_DrawStringExt( 320 - w * cw/2, 25,s, colorWhite,
        qfalse, qtrue, cw, (int)(cw * 1.1f), 0 );
  }

  sec = ( sec - cg.time ) / 1000;
  if ( sec < 0 ) {
    cg.warmup = 0;
    sec = 0;
  }
  s = va( "Starts in: %i", sec + 1 );
  if ( sec != cg.warmupCount ) {
    cg.warmupCount = sec;
    switch ( sec ) {
    case 0:
      trap_S_StartLocalSound( cgs.media.count1Sound, CHAN_ANNOUNCER );
      break;
    case 1:
      trap_S_StartLocalSound( cgs.media.count2Sound, CHAN_ANNOUNCER );
      break;
    case 2:
      trap_S_StartLocalSound( cgs.media.count3Sound, CHAN_ANNOUNCER );
      break;
    default:
      break;
    }
  }
  scale = 0.45f;
  switch ( cg.warmupCount ) {
  case 0:
    cw = 28;
    scale = 0.54f;
    break;
  case 1:
    cw = 24;
    scale = 0.51f;
    break;
  case 2:
    cw = 20;
    scale = 0.48f;
    break;
  default:
    cw = 16;
    scale = 0.45f;
    break;
  }

  w = CG_DrawStrlen( s );
  CG_DrawStringExt( 320 - w * cw/2, 70, s, colorWhite,
      qfalse, qtrue, cw, (int)(cw * 1.5), 0 );
}

//==================================================================================

/*
=================
CG_Draw2D
=================
*/
static void CG_Draw2D( void ) {

  // if we are taking a levelshot for the menu, don't draw anything
  if ( cg.levelShot ) {
    return;
  }

  if ( cg_draw2D.integer == 0 ) {
    return;
  }

  if ( cg.snap->ps.pm_type == PM_INTERMISSION ) {
    CG_DrawIntermission();
    return;
  }

  if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
    CG_DrawSpectator();
    CG_DrawCrosshair();
    CG_DrawCrosshairNames();
    CG_DrawLighting();
  } else {
    // don't draw any status if dead or the scoreboard is being explicitly shown
    if ( !cg.showScores && cg.snap->ps.stats[STAT_HEALTH] > 0 ) {
      CG_DrawLighting();
      CG_DrawStatusBar();
      CG_DrawAmmoWarning();
      CG_DrawCrosshair();
      CG_DrawCrosshairNames();
      CG_DrawWeaponSelect();
      CG_DrawHoldableItem();
      CG_DrawReward();
    }
    if ( cgs.gametype >= GT_TEAM ) {
      CG_DrawTeamInfo();
    }
    if( cg.snap->ps.weapon == WP_SCANNER )
      CG_Scanner();
  }

  CG_DrawVote();
  CG_DrawTeamVote();

  CG_DrawLagometer();

  CG_DrawUpperRight();

  CG_DrawLowerRight();

  CG_DrawLowerLeft();

  if ( !CG_DrawFollow() ) {
    CG_DrawWarmup();
  }

  // don't draw center string if scoreboard is up
  cg.scoreBoardShowing = CG_DrawScoreboard();
  if ( !cg.scoreBoardShowing) {
    CG_DrawCenterString();
  }
}


static void CG_DrawTourneyScoreboard() {
  CG_DrawOldTourneyScoreboard();
}


/*
=====================
CG_DrawActive

Perform all drawing needed to completely fill the screen
=====================
*/
void CG_DrawActive( stereoFrame_t stereoView ) {
  float   separation;
  vec3_t    baseOrg;

  // optionally draw the info screen instead
  if ( !cg.snap ) {
    CG_DrawInformation();
    return;
  }

  // optionally draw the tournement scoreboard instead
  if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR &&
    ( cg.snap->ps.pm_flags & PMF_SCOREBOARD ) ) {
    CG_DrawTourneyScoreboard();
    return;
  }

  switch ( stereoView ) {
  case STEREO_CENTER:
    separation = 0;
    break;
  case STEREO_LEFT:
    separation = -cg_stereoSeparation.value / 2;
    break;
  case STEREO_RIGHT:
    separation = cg_stereoSeparation.value / 2;
    break;
  default:
    separation = 0;
    CG_Error( "CG_DrawActive: Undefined stereoView" );
  }


  // clear around the rendered view if sized down
  CG_TileClear();

  // offset vieworg appropriately if we're doing stereo separation
  VectorCopy( cg.refdef.vieworg, baseOrg );
  if ( separation != 0 ) {
    VectorMA( cg.refdef.vieworg, -separation, cg.refdef.viewaxis[1], cg.refdef.vieworg );
  }

  // draw 3D view
  trap_R_RenderScene( &cg.refdef );

  // restore original viewpoint if running stereo
  if ( separation != 0 ) {
    VectorCopy( baseOrg, cg.refdef.vieworg );
  }

  // draw status bar and other floating elements
  CG_Draw2D();
}



