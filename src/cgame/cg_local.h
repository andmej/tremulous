// Copyright (C) 1999-2000 Id Software, Inc.
//

/*
 *  Portions Copyright (C) 2000-2001 Tim Angus
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the OSML - Open Source Modification License v1.0 as
 *  described in the file COPYING which is distributed with this source
 *  code.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "../game/q_shared.h"
#include "tr_types.h"
#include "../game/bg_public.h"
#include "cg_public.h"
#include "../ui/ui_shared.h"

// The entire cgame module is unloaded and reloaded on each level change,
// so there is NO persistant data between levels on the client side.
// If you absolutely need something stored, it can either be kept
// by the server in the server stored userinfos, or stashed in a cvar.

#define CG_FONT_THRESHOLD 0.1

#define POWERUP_BLINKS      5

#define POWERUP_BLINK_TIME  1000
#define FADE_TIME           200
#define PULSE_TIME          200
#define DAMAGE_DEFLECT_TIME 100
#define DAMAGE_RETURN_TIME  400
#define DAMAGE_TIME         500
#define LAND_DEFLECT_TIME   150
#define LAND_RETURN_TIME    300
#define DUCK_TIME           100
#define PAIN_TWITCH_TIME    200
#define WEAPON_SELECT_TIME  1400
#define ITEM_SCALEUP_TIME   1000
#define ZOOM_TIME           150
#define ITEM_BLOB_TIME      200
#define MUZZLE_FLASH_TIME   20
#define SINK_TIME           1000    // time for fragments to sink into ground before going away
#define ATTACKER_HEAD_TIME  10000
#define REWARD_TIME         3000

#define PULSE_SCALE         1.5     // amount to scale up the icons when activating

#define MAX_STEP_CHANGE     32

#define MAX_VERTS_ON_POLY   10
#define MAX_MARK_POLYS      256

#define STAT_MINUS          10  // num frame for '-' stats digit

#define ICON_SIZE           48
#define CHAR_WIDTH          32
#define CHAR_HEIGHT         48
#define TEXT_ICON_SPACE     4

#define TEAMCHAT_WIDTH      80
#define TEAMCHAT_HEIGHT     8

// very large characters
#define GIANT_WIDTH         32
#define GIANT_HEIGHT        48

#define NUM_CROSSHAIRS      10

//TA: ripped from wolf source
// Ridah, trails
#define STYPE_STRETCH 0
#define STYPE_REPEAT  1

#define TJFL_FADEIN   (1<<0)
#define TJFL_CROSSOVER  (1<<1)
#define TJFL_NOCULL   (1<<2)
#define TJFL_FIXDISTORT (1<<3)
#define TJFL_SPARKHEADFLARE (1<<4)
#define TJFL_NOPOLYMERGE  (1<<5)
// done.

#define TEAM_OVERLAY_MAXNAME_WIDTH  12
#define TEAM_OVERLAY_MAXLOCATION_WIDTH  16

#define DEFAULT_MODEL       "sarge"
#define DEFAULT_TEAM_MODEL  "sarge"
#define DEFAULT_TEAM_HEAD   "sarge"

#define DEFAULT_REDTEAM_NAME    "Stroggs"
#define DEFAULT_BLUETEAM_NAME   "Pagans"

typedef enum
{
  FOOTSTEP_NORMAL,
  FOOTSTEP_BOOT,
  FOOTSTEP_FLESH,
  FOOTSTEP_MECH,
  FOOTSTEP_ENERGY,
  FOOTSTEP_METAL,
  FOOTSTEP_SPLASH,
  FOOTSTEP_CUSTOM,
  FOOTSTEP_NONE,

  FOOTSTEP_TOTAL
} footstep_t;

typedef enum
{
  IMPACTSOUND_DEFAULT,
  IMPACTSOUND_METAL,
  IMPACTSOUND_FLESH
} impactSound_t;

typedef enum
{
  JPS_OFF,
  JPS_DESCENDING,
  JPS_HOVERING,
  JPS_ASCENDING
} jetPackState_t;

//particle system stuff
#define MAX_SHADER_FRAMES         32
#define MAX_EJECTORS_PER_SYSTEM   4
#define MAX_PARTICLES_PER_EJECTOR 4

#define MAX_BASEPARTICLE_SYSTEMS  192
#define MAX_BASEPARTICLE_EJECTORS MAX_BASEPARTICLE_SYSTEMS*MAX_EJECTORS_PER_SYSTEM
#define MAX_BASEPARTICLES         MAX_BASEPARTICLE_EJECTORS*MAX_PARTICLES_PER_EJECTOR

#define MAX_PARTICLE_SYSTEMS      48
#define MAX_PARTICLE_EJECTORS     MAX_PARTICLE_SYSTEMS*MAX_EJECTORS_PER_SYSTEM
#define MAX_PARTICLES             MAX_PARTICLE_EJECTORS*5

#define PARTICLES_INFINITE        -1
#define PARTICLES_SAME_AS_INITIAL -2

/*
===============

COMPILE TIME STRUCTURES

===============
*/

typedef enum
{
  PMT_STATIC,
  PMT_TAG,
  PMT_CENT_ANGLES,
  PMT_NORMAL
} pMoveType_t;

typedef enum
{
  PMD_LINEAR,
  PMD_POINT
} pDirType_t;

typedef struct pMoveValues_u
{
  pDirType_t  dirType;
  
  //PMD_LINEAR
  vec3_t      dir; 
  float       dirRandAngle;
  
  //PMD_POINT
  vec3_t      point;
  float       pointRandAngle;
  
  float       mag;
  float       magRandFrac;
  
  float       parentVelFrac;
  float       parentVelFracRandFrac;
} pMoveValues_t;

typedef struct pLerpValues_s
{
  int   delay;
  float delayRandFrac;
  
  float initial;
  float initialRandFrac;
  
  float final;
  float finalRandFrac;

  float randFrac;
} pLerpValues_t;

//particle template
typedef struct baseParticle_s
{
  vec3_t          displacement;
  float           randDisplacement;
  float           normalDisplacement;
  
  pMoveType_t     velMoveType;
  pMoveValues_t   velMoveValues;
  
  pMoveType_t     accMoveType;
  pMoveValues_t   accMoveValues;

  int             lifeTime;
  float           lifeTimeRandFrac;

  float           bounceFrac;
  float           bounceFracRandFrac;
  qboolean        bounceCull;

  pLerpValues_t   radius;
  pLerpValues_t   alpha;
  pLerpValues_t   rotation;

  char            childSystemName[ MAX_QPATH ];
  qhandle_t       childSystemHandle;

  char            onDeathSystemName[ MAX_QPATH ];
  qhandle_t       onDeathSystemHandle;
  
  //particle invariant stuff
  char            shaderNames[ MAX_QPATH ][ MAX_SHADER_FRAMES ];
  qhandle_t       shaders[ MAX_SHADER_FRAMES ];
  int             numFrames;
  float           framerate;

  qboolean        overdrawProtection;
  qboolean        realLight;
  qboolean        cullOnStartSolid;
} baseParticle_t;


//ejector template
typedef struct baseParticleEjector_s
{
  baseParticle_t  *particles[ MAX_PARTICLES_PER_EJECTOR ];
  int             numParticles;
  
  pLerpValues_t   eject;          //zero period indicates creation of all particles at once

  int             totalParticles;         //can be infinite
  float           totalParticlesRandFrac;
} baseParticleEjector_t;


//particle system template
typedef struct baseParticleSystem_s
{
  char                  name[ MAX_QPATH ];
  baseParticleEjector_t *ejectors[ MAX_EJECTORS_PER_SYSTEM ];
  int                   numEjectors;

  qboolean              registered; //whether or not the assets for this particle have been loaded
} baseParticleSystem_t;


/*
===============

RUN TIME STRUCTURES

===============
*/

typedef enum
{
  PSA_STATIC,
  PSA_TAG,
  PSA_CENT_ORIGIN,
  PSA_PARTICLE
} psAttachmentType_t;


typedef struct psAttachment_s
{
  qboolean staticValid;
  qboolean tagValid;
  qboolean centValid;
  qboolean normalValid;
  qboolean particleValid;

  //PMT_STATIC
  vec3_t      origin;
  
  //PMT_TAG
  refEntity_t re;     //FIXME: should be pointers?
  refEntity_t parent; //
  qhandle_t   model;
  char        tagName[ MAX_STRING_CHARS ];
  
  //PMT_CENT_ANGLES
  int         centNum;

  //PMT_NORMAL
  vec3_t      normal;
} psAttachment_t;


typedef struct particleSystem_s
{
  baseParticleSystem_t  *class;
  
  psAttachmentType_t    attachType;
  psAttachment_t        attachment;
  qboolean              attached;   //is the particle system attached to anything

  qboolean              valid;
  qboolean              lazyRemove; //mark this system for later removal
  
} particleSystem_t;


typedef struct particleEjector_s
{
  baseParticleEjector_t *class;
  particleSystem_t      *parent;

  pLerpValues_t         ejectPeriod;

  int                   count;
  int                   totalParticles;
  
  int                   nextEjectionTime;
  
  qboolean              valid;
} particleEjector_t;


//used for actual particle evaluation
typedef struct particle_s
{
  baseParticle_t    *class;
  particleEjector_t *parent;
  
  int               birthTime;
  int               lifeTime;
  
  vec3_t            origin;
  vec3_t            velocity;
  
  pMoveType_t       accMoveType;
  pMoveValues_t     accMoveValues;

  int               lastEvalTime;

  int               nextChildTime;

  pLerpValues_t     radius;
  pLerpValues_t     alpha;
  pLerpValues_t     rotation;
  
  qboolean          valid;

  int               sortKey;

  particleSystem_t  *childSystem;
} particle_t;


//=================================================

// player entities need to track more information
// than any other type of entity.

// note that not every player entity is a client entity,
// because corpses after respawn are outside the normal
// client numbering range

// when changing animation, set animationTime to frameTime + lerping time
// The current lerp will finish out, then it will lerp to the new animation
typedef struct
{
  int         oldFrame;
  int         oldFrameTime;     // time when ->oldFrame was exactly on

  int         frame;
  int         frameTime;        // time when ->frame will be exactly on

  float       backlerp;

  float       yawAngle;
  qboolean    yawing;
  float       pitchAngle;
  qboolean    pitching;

  int         animationNumber;  // may include ANIM_TOGGLEBIT
  animation_t *animation;
  int         animationTime;    // time when the first frame of the animation will be exact
} lerpFrame_t;

//TA: smoothing of view and model for WW transitions
#define   MAXSMOOTHS          32

typedef struct
{
  float     time;
  float     timeMod;
  
  vec3_t    rotAxis;
  float     rotAngle;
} smooth_t;


typedef struct
{
  lerpFrame_t legs, torso, flag, nonseg;
  int         painTime;
  int         painDirection;  // flip from 0 to 1

  // machinegun spinning
  float       barrelAngle;
  int         barrelTime;
  qboolean    barrelSpinning;

  vec3_t      lastNormal;
  vec3_t      lastAxis[ 3 ];
  smooth_t    sList[ MAXSMOOTHS ];
} playerEntity_t;

typedef struct lightFlareStatus_s
{
  float     lastSrcRadius; //caching of likely flare source radius
  float     lastRadius;    //caching of likely flare radius
  float     lastRatio;     //caching of likely flare ratio
  int       lastTime;      //last time flare was visible/occluded
  qboolean  status;        //flare is visble?
} lightFlareStatus_t;

//=================================================

#define MAX_CENTITY_PARTICLE_SYSTEMS  8

// centity_t have a direct corespondence with gentity_t in the game, but
// only the entityState_t is directly communicated to the cgame
typedef struct centity_s
{
  entityState_t         currentState;     // from cg.frame
  entityState_t         nextState;        // from cg.nextFrame, if available
  qboolean              interpolate;      // true if next is valid to interpolate to
  qboolean              currentValid;     // true if cg.frame holds this entity

  int                   muzzleFlashTime;  // move to playerEntity?
  int                   muzzleFlashTime2; // move to playerEntity?
  int                   muzzleFlashTime3; // move to playerEntity?
  int                   previousEvent;
  int                   teleportFlag;

  int                   trailTime;        // so missile trails can handle dropped initial packets
  int                   dustTrailTime;
  int                   miscTime;
  int                   snapShotTime;     // last time this entity was found in a snapshot

  playerEntity_t        pe;

  int                   errorTime;        // decay the error from this time
  vec3_t                errorOrigin;
  vec3_t                errorAngles;

  qboolean              extrapolated;     // false if origin / angles is an interpolation
  vec3_t                rawOrigin;
  vec3_t                rawAngles;

  vec3_t                beamEnd;

  // exact interpolated position of entity on this frame
  vec3_t                lerpOrigin;
  vec3_t                lerpAngles;

  lerpFrame_t           lerpFrame;

  //TA:
  buildableAnimNumber_t buildableAnim;    //persistant anim number
  buildableAnimNumber_t oldBuildableAnim; //to detect when new anims are set
  particleSystem_t      *buildablePS;

  lightFlareStatus_t    lfs;

  qboolean              doorState;

  particleSystem_t      *muzzlePS;
  qboolean              muzzlePsTrigger;

  particleSystem_t      *jetPackPS;
  jetPackState_t        jetPackState;

  particleSystem_t      *entityPS;
  qboolean              entityPSMissing;
  
  qboolean              valid;
  qboolean              oldValid;
} centity_t;


//======================================================================

// local entities are created as a result of events or predicted actions,
// and live independantly from all server transmitted entities

typedef struct markPoly_s
{
  struct markPoly_s *prevMark, *nextMark;
  int               time;
  qhandle_t         markShader;
  qboolean          alphaFade;    // fade alpha instead of rgb
  float             color[ 4 ];
  poly_t            poly;
  polyVert_t        verts[ MAX_VERTS_ON_POLY ];
} markPoly_t;


typedef enum
{
  LE_MARK,
  LE_EXPLOSION,
  LE_LIGHTNING_BOLT, //wolf trail
  LE_SPRITE_EXPLOSION,
  LE_FRAGMENT,
  LE_MOVE_SCALE_FADE,
  LE_FALL_SCALE_FADE,
  LE_FADE_RGB,
  LE_SCALE_FADE
} leType_t;

typedef enum
{
  LEF_PUFF_DONT_SCALE   = 0x0001,      // do not scale size over time
  LEF_TUMBLE            = 0x0002     // tumble over time, used for ejecting shells
} leFlag_t;

typedef enum
{
  LEMT_NONE,
  LEMT_BURN,
  LEMT_BLOOD,
  LEMT_GREENBLOOD,  //TA: when aliens are injured
  LEMT_BANG         //TA: human item explosions
} leMarkType_t;     // fragment local entities can leave marks on walls

typedef enum
{
  LEBS_NONE,
  LEBS_BLOOD,
  LEBS_BANG,        //TA: human item explosions
  LEBS_BRASS
} leBounceSoundType_t;  // fragment local entities can make sounds on impacts

typedef struct localEntity_s
{
  struct localEntity_s  *prev, *next;
  leType_t              leType;
  int                   leFlags;

  int                   startTime;
  int                   endTime;
  int                   fadeInTime;

  float                 lifeRate;     // 1.0 / (endTime - startTime)

  trajectory_t          pos;
  trajectory_t          angles;

  float                 bounceFactor;   // 0.0 = no bounce, 1.0 = perfect

  float                 color[4];

  float                 radius;

  float                 light;
  vec3_t                lightColor;

  leMarkType_t          leMarkType;   // mark to leave on fragment impact
  leBounceSoundType_t   leBounceSoundType;

  refEntity_t           refEntity;

  unsigned int          sortKey;

  //TA: lightning bolt endpoint entities
  int                   srcENum, destENum;
  int                   vOffset;
  int                   maxRange;
} localEntity_t;

//======================================================================


typedef struct
{
  int client;
  int score;
  int ping;
  int time;
  int team;
} score_t;

// each client has an associated clientInfo_t
// that contains media references necessary to present the
// client model and other color coded effects
// this is regenerated each time a client's configstring changes,
// usually as a result of a userinfo (name, model, etc) change
#define MAX_CUSTOM_SOUNDS 32
typedef struct
{
  qboolean    infoValid;

  char        name[ MAX_QPATH ];
  team_t      team;

  int         botSkill;                   // 0 = not bot, 1-5 = bot

  vec3_t      color1;
  vec3_t      color2;

  int         score;                      // updated by score servercmds
  int         location;                   // location index for team mode
  int         health;                     // you only get this info about your teammates
  int         armor;
  int         curWeapon;

  int         handicap;
  int         wins, losses;               // in tourney mode

  int         teamTask;                   // task in teamplay (offence/defence)
  qboolean    teamLeader;                 // true when this is a team leader

  int         powerups;                   // so can display quad/flag status

  int         medkitUsageTime;
  int         invulnerabilityStartTime;
  int         invulnerabilityStopTime;

  int         breathPuffTime;

  // when clientinfo is changed, the loading of models/skins/sounds
  // can be deferred until you are dead, to prevent hitches in
  // gameplay
  char        modelName[ MAX_QPATH ];
  char        skinName[ MAX_QPATH ];
  char        headModelName[ MAX_QPATH ];
  char        headSkinName[ MAX_QPATH ];
  char        redTeam[ MAX_TEAMNAME ];
  char        blueTeam[ MAX_TEAMNAME ];
        
  qboolean    newAnims;                   // true if using the new mission pack animations
  qboolean    fixedlegs;                  // true if legs yaw is always the same as torso yaw
  qboolean    fixedtorso;                 // true if torso never changes yaw
  qboolean    nonsegmented;               // true if model is Q2 style nonsegmented
    
  vec3_t      headOffset;                 // move head in icon views
  footstep_t  footsteps;
  gender_t    gender;                     // from model

  qhandle_t   legsModel;
  qhandle_t   legsSkin;

  qhandle_t   torsoModel;
  qhandle_t   torsoSkin;

  qhandle_t   headModel;
  qhandle_t   headSkin;

  qhandle_t   nonSegModel;                //non-segmented model system
  qhandle_t   nonSegSkin;                 //non-segmented model system

  qhandle_t   modelIcon;

  animation_t animations[ MAX_PLAYER_TOTALANIMATIONS ];

  sfxHandle_t sounds[ MAX_CUSTOM_SOUNDS ];

  sfxHandle_t customFootsteps[ 4 ];
  sfxHandle_t customMetalFootsteps[ 4 ];
} clientInfo_t;


typedef struct weaponInfoMode_s
{
  float       flashDlight;
  vec3_t      flashDlightColor;
  sfxHandle_t flashSound[ 4 ];  // fast firing weapons randomly choose
  qboolean    continuousFlash;

  qhandle_t   missileModel;
  sfxHandle_t missileSound;
  float       missileDlight;
  vec3_t      missileDlightColor;
  int         missileRenderfx;
  qboolean    usesSpriteMissle;
  qhandle_t   missileSprite;
  int         missileSpriteSize;
  qhandle_t   missileParticleSystem;
  qboolean    missileRotates;
  qboolean    missileAnimates;
  int         missileAnimStartFrame;
  int         missileAnimNumFrames;
  int         missileAnimFrameRate;
  int         missileAnimLooping;

  sfxHandle_t firingSound;
  qboolean    loopFireSound;

  qhandle_t   muzzleParticleSystem;

  qboolean    alwaysImpact;
  qhandle_t   impactModel;
  qhandle_t   impactModelShader;
  qhandle_t   impactParticleSystem;
  qhandle_t   impactMark;
  qhandle_t   impactMarkSize;
  sfxHandle_t impactSound[ 4 ]; //random impact sound
  sfxHandle_t impactFleshSound[ 4 ]; //random impact sound
  float       impactDlight;
  vec3_t      impactDlightColor;
} weaponInfoMode_t;

// each WP_* weapon enum has an associated weaponInfo_t
// that contains media references necessary to present the
// weapon and its effects
typedef struct weaponInfo_s
{
  qboolean          registered;
  char              *humanName;

  qhandle_t         handsModel;       // the hands don't actually draw, they just position the weapon
  qhandle_t         weaponModel;
  qhandle_t         barrelModel;
  qhandle_t         flashModel;

  vec3_t            weaponMidpoint;   // so it will rotate centered instead of by tag

  qhandle_t         weaponIcon;
  qhandle_t         ammoIcon;

  qhandle_t         crossHair;
  int               crossHairSize;

  void              (*ejectBrassFunc)( centity_t * );

  sfxHandle_t       readySound;

  qboolean          disableIn3rdPerson;
  
  weaponInfoMode_t  wim[ WPM_NUM_WEAPONMODES ];
} weaponInfo_t;

typedef struct upgradeInfo_s
{
  qboolean    registered;
  char        *humanName;

  qhandle_t   upgradeIcon;
} upgradeInfo_t;

typedef struct
{
  qboolean    looped;
  qboolean    enabled;
  
  sfxHandle_t sound;
} sound_t;

typedef struct
{
  qhandle_t   models[ MAX_BUILDABLE_MODELS ];
  animation_t animations[ MAX_BUILDABLE_ANIMATIONS ];

  //same number of sounds as animations
  sound_t     sounds[ MAX_BUILDABLE_ANIMATIONS ];
} buildableInfo_t;

#define MAX_REWARDSTACK   10
#define MAX_SOUNDBUFFER   20

//======================================================================

//TA:
typedef struct
{
  vec3_t    alienBuildablePos[ MAX_GENTITIES ];
  int       alienBuildableTimes[ MAX_GENTITIES ];
  int       numAlienBuildables;
  
  vec3_t    humanBuildablePos[ MAX_GENTITIES ];
  int       numHumanBuildables;
  
  vec3_t    alienClientPos[ MAX_CLIENTS ];
  int       numAlienClients;
  
  vec3_t    humanClientPos[ MAX_CLIENTS ];
  int       numHumanClients;

  int       lastUpdateTime;
  vec3_t    origin;
  vec3_t    vangles;
} entityPos_t;

typedef struct
{
  int time;
  int length;
} consoleLine_t;

#define MAX_CONSOLE_TEXT  8192
#define MAX_CONSOLE_LINES 32

// all cg.stepTime, cg.duckTime, cg.landTime, etc are set to cg.time when the action
// occurs, and they will have visible effects for #define STEP_TIME or whatever msec after

#define MAX_PREDICTED_EVENTS  16

typedef struct
{
  int           clientFrame;                        // incremented each frame

  int           clientNum;

  qboolean      demoPlayback;
  qboolean      levelShot;                          // taking a level menu screenshot
  int           deferredPlayerLoading;              
  qboolean      loading;                            // don't defer players at initial startup
  qboolean      intermissionStarted;                // don't play voice rewards, because game will end shortly

  // there are only one or two snapshot_t that are relevent at a time
  int           latestSnapshotNum;                  // the number of snapshots the client system has received
  int           latestSnapshotTime;                 // the time from latestSnapshotNum, so we don't need to read the snapshot yet

  snapshot_t    *snap;                              // cg.snap->serverTime <= cg.time
  snapshot_t    *nextSnap;                          // cg.nextSnap->serverTime > cg.time, or NULL
  snapshot_t    activeSnapshots[ 2 ];

  float         frameInterpolation;                 // (float)( cg.time - cg.frame->serverTime ) /
                                                    // (cg.nextFrame->serverTime - cg.frame->serverTime)

  qboolean      thisFrameTeleport;
  qboolean      nextFrameTeleport;

  int           frametime;                          // cg.time - cg.oldTime

  int           time;                               // this is the time value that the client
                                                    // is rendering at.
  int           oldTime;                            // time at last frame, used for missile trails and prediction checking

  int           physicsTime;                        // either cg.snap->time or cg.nextSnap->time

  int           timelimitWarnings;                  // 5 min, 1 min, overtime
  int           fraglimitWarnings;

  qboolean      mapRestart;                         // set on a map restart to set back the weapon

  qboolean      renderingThirdPerson;               // during deaths, chasecams, etc

  // prediction state
  qboolean      hyperspace;                         // true if prediction has hit a trigger_teleport
  playerState_t predictedPlayerState;
  centity_t     predictedPlayerEntity;
  qboolean      validPPS;                           // clear until the first call to CG_PredictPlayerState
  int           predictedErrorTime;
  vec3_t        predictedError;

  int           eventSequence;
  int           predictableEvents[MAX_PREDICTED_EVENTS];

  float         stepChange;                         // for stair up smoothing
  int           stepTime;

  float         duckChange;                         // for duck viewheight smoothing
  int           duckTime;

  float         landChange;                         // for landing hard
  int           landTime;

  // input state sent to server
  int           weaponSelect;

  // auto rotating items
  vec3_t        autoAngles;
  vec3_t        autoAxis[ 3 ];
  vec3_t        autoAnglesFast;
  vec3_t        autoAxisFast[ 3 ];

  // view rendering
  refdef_t      refdef;
  vec3_t        refdefViewAngles;                   // will be converted to refdef.viewaxis

  // zoom key
  qboolean      zoomed;
  int           zoomTime;
  float         zoomSensitivity;

  // information screen text during loading
  char          infoScreenText[ MAX_STRING_CHARS ];

  // scoreboard
  int           scoresRequestTime;
  int           numScores;
  int           selectedScore;
  int           teamScores[ 2 ];
  score_t       scores[MAX_CLIENTS];
  qboolean      showScores;
  qboolean      scoreBoardShowing;
  int           scoreFadeTime;
  char          killerName[ MAX_NAME_LENGTH ];
  char          spectatorList[ MAX_STRING_CHARS ];  // list of names
  int           spectatorLen;                       // length of list
  float         spectatorWidth;                     // width in device units
  int           spectatorTime;                      // next time to offset
  int           spectatorPaintX;                    // current paint x
  int           spectatorPaintX2;                   // current paint x
  int           spectatorOffset;                    // current offset from start
  int           spectatorPaintLen;                  // current offset from start

  // centerprinting
  int           centerPrintTime;
  int           centerPrintCharWidth;
  int           centerPrintY;
  char          centerPrint[ 1024 ];
  int           centerPrintLines;

  // low ammo warning state
  int           lowAmmoWarning;   // 1 = low, 2 = empty

  // kill timers for carnage reward
  int           lastKillTime;

  // crosshair client ID
  int           crosshairClientNum;
  int           crosshairClientTime;

  // powerup active flashing
  int           powerupActive;
  int           powerupTime;

  // attacking player
  int           attackerTime;
  int           voiceTime;

  // reward medals
  int           rewardStack;
  int           rewardTime;
  int           rewardCount[ MAX_REWARDSTACK ];
  qhandle_t     rewardShader[ MAX_REWARDSTACK ];
  qhandle_t     rewardSound[ MAX_REWARDSTACK ];

  // sound buffer mainly for announcer sounds
  int           soundBufferIn;
  int           soundBufferOut;
  int           soundTime;
  qhandle_t     soundBuffer[ MAX_SOUNDBUFFER ];

  // for voice chat buffer
  int           voiceChatTime;
  int           voiceChatBufferIn;
  int           voiceChatBufferOut;
        
  // warmup countdown
  int           warmup;
  int           warmupCount;

  //==========================

  int           itemPickup;
  int           itemPickupTime;
  int           itemPickupBlendTime;                // the pulse around the crosshair is timed seperately

  int           weaponSelectTime;
  int           weaponAnimation;
  int           weaponAnimationTime;

  // blend blobs
  float         damageTime;
  float         damageX, damageY, damageValue;

  // status bar head
  float         headYaw;
  float         headEndPitch;
  float         headEndYaw;
  int           headEndTime;
  float         headStartPitch;
  float         headStartYaw;
  int           headStartTime;

  // view movement
  float         v_dmg_time;
  float         v_dmg_pitch;
  float         v_dmg_roll;

  vec3_t        kick_angles;                        // weapon kicks
  vec3_t        kick_origin;

  // temp working variables for player view
  float         bobfracsin;
  int           bobcycle;
  float         xyspeed;
  int           nextOrbitTime;

  // development tool
  refEntity_t   testModelEntity;
  char          testModelName[MAX_QPATH];
  qboolean      testGun;

  int           spawnTime;                          //TA: fovwarp
  int           weapon1Time;                        //TA: time when BUTTON_ATTACK went t->f f->t
  int           weapon2Time;                        //TA: time when BUTTON_ATTACK2 went t->f f->t
  int           weapon3Time;                        //TA: time when BUTTON_USE_HOLDABLE went t->f f->t
  qboolean      weapon1Firing;                      
  qboolean      weapon2Firing;                      
  qboolean      weapon3Firing;                      

  int           poisonedTime;

  vec3_t        lastNormal;                         //TA: view smoothage
  vec3_t        lastVangles;                        //TA: view smoothage
  smooth_t      sList[ MAXSMOOTHS ];                //TA: WW smoothing

  int           forwardMoveTime;                    //TA: for struggling
  int           rightMoveTime;
  int           upMoveTime;

  float         charModelFraction;                  //TA: loading percentages
  float         mediaFraction;
  float         buildablesFraction;

  int           lastBuildAttempt;
  int           lastEvolveAttempt;

  char          consoleText[ MAX_CONSOLE_TEXT ];
  consoleLine_t consoleLines[ MAX_CONSOLE_LINES ];
  int           numConsoleLines;
  qboolean      consoleValid;
  
  particleSystem_t *poisonCloudPS;
} cg_t;


// all of the model, shader, and sound references that are
// loaded at gamestate time are stored in cgMedia_t
// Other media that can be tied to clients, weapons, or items are
// stored in the clientInfo_t, itemInfo_t, weaponInfo_t, and powerupInfo_t
typedef struct
{
  qhandle_t   charsetShader;
  qhandle_t   whiteShader;
  qhandle_t   outlineShader;

  qhandle_t   deferShader;

  // gib explosions
  qhandle_t   gibAbdomen;
  qhandle_t   gibArm;
  qhandle_t   gibChest;
  qhandle_t   gibFist;
  qhandle_t   gibFoot;
  qhandle_t   gibForearm;
  qhandle_t   gibIntestine;
  qhandle_t   gibLeg;
  qhandle_t   gibSkull;
  qhandle_t   gibBrain;

  qhandle_t   metalGib1;
  qhandle_t   metalGib2;
  qhandle_t   metalGib3;
  qhandle_t   metalGib4;
  qhandle_t   metalGib5;
  qhandle_t   metalGib6;
  qhandle_t   metalGib7;
  qhandle_t   metalGib8;

  qhandle_t   alienGib1;
  qhandle_t   alienGib2;
  qhandle_t   alienGib3;
  qhandle_t   alienGib4;

  qhandle_t   gibSpark1;
  qhandle_t   gibSpark2;
  
  qhandle_t   smoke2;

  qhandle_t   machinegunBrassModel;
  qhandle_t   shotgunBrassModel;

  qhandle_t   lightningShader;

  qhandle_t   friendShader;

  qhandle_t   balloonShader;
  qhandle_t   connectionShader;

  qhandle_t   selectShader;
  qhandle_t   viewBloodShader;
  qhandle_t   tracerShader;
  qhandle_t   crosshairShader[ WP_NUM_WEAPONS ];
  qhandle_t   backTileShader;
  qhandle_t   noammoShader;

  qhandle_t   smokePuffShader;
  qhandle_t   smokePuffRageProShader;
  qhandle_t   shotgunSmokePuffShader;
  qhandle_t   plasmaBallShader;
  qhandle_t   blasterShader;
  qhandle_t   waterBubbleShader;
  qhandle_t   bloodTrailShader;

  //TA: extra stuff
  qhandle_t   explosionShader;
  qhandle_t   greenBloodTrailShader;
  qhandle_t   greenBloodMarkShader;
  qhandle_t   greenBloodExplosionShader;
  qhandle_t   explosionTrailShader;

  qhandle_t   flameExplShader;
  qhandle_t   creepShader;
  
  qhandle_t   scannerShader;
  qhandle_t   scannerBlipShader;
  qhandle_t   scannerLineShader;


  qhandle_t   numberShaders[ 11 ];

  qhandle_t   shadowMarkShader;

  // wall mark shaders
  qhandle_t   wakeMarkShader;
  qhandle_t   bloodMarkShader;
  qhandle_t   bulletMarkShader;
  qhandle_t   burnMarkShader;
  qhandle_t   holeMarkShader;
  qhandle_t   energyMarkShader;

  //TA: buildable shaders
  qhandle_t   greenBuildShader;
  qhandle_t   redBuildShader;
  qhandle_t   noPowerShader;
  qhandle_t   humanSpawningShader;

  // weapon effect models
  qhandle_t   bulletFlashModel;
  qhandle_t   ringFlashModel;
  qhandle_t   dishFlashModel;
  qhandle_t   lightningExplosionModel;

  // weapon effect shaders
  qhandle_t   bloodExplosionShader;

  // special effects models
  qhandle_t   teleportEffectModel;
  qhandle_t   teleportEffectShader;

  // sounds
  sfxHandle_t tracerSound;
  sfxHandle_t selectSound;
  sfxHandle_t useNothingSound;
  sfxHandle_t wearOffSound;
  sfxHandle_t footsteps[ FOOTSTEP_TOTAL ][ 4 ];
  sfxHandle_t gibSound;
  sfxHandle_t gibBounce1Sound;
  sfxHandle_t gibBounce2Sound;
  sfxHandle_t gibBounce3Sound;
  sfxHandle_t metalGibBounceSound;
  sfxHandle_t teleInSound;
  sfxHandle_t teleOutSound;
  sfxHandle_t noAmmoSound;
  sfxHandle_t respawnSound;
  sfxHandle_t talkSound;
  sfxHandle_t landSound;
  sfxHandle_t fallSound;
  sfxHandle_t jumpPadSound;

  sfxHandle_t hgrenb1aSound;
  sfxHandle_t hgrenb2aSound;

  sfxHandle_t voteNow;
  sfxHandle_t votePassed;
  sfxHandle_t voteFailed;

  sfxHandle_t watrInSound;
  sfxHandle_t watrOutSound;
  sfxHandle_t watrUnSound;

  sfxHandle_t jetpackDescendSound;
  sfxHandle_t jetpackIdleSound;
  sfxHandle_t jetpackAscendSound;
  
  qhandle_t   jetPackDescendPS;
  qhandle_t   jetPackHoverPS;
  qhandle_t   jetPackAscendPS;
  
  //TA:
  sfxHandle_t alienStageTransition;
  sfxHandle_t humanStageTransition;
  
  sfxHandle_t alienOvermindAttack;
  sfxHandle_t alienOvermindDying;
  sfxHandle_t alienOvermindSpawns;
  
  sfxHandle_t alienBuildableExplosion;
  sfxHandle_t alienBuildableDamage;
  sfxHandle_t alienBuildablePrebuild;
  sfxHandle_t humanBuildableExplosion;
  sfxHandle_t humanBuildablePrebuild;
  sfxHandle_t humanBuildableDamage[ 4 ];

  sfxHandle_t alienL1Grab;
  sfxHandle_t alienL4ChargePrepare;
  sfxHandle_t alienL4ChargeStart;
  
  qhandle_t   cursor;
  qhandle_t   selectCursor;
  qhandle_t   sizeCursor;

  //TA: for wolf trail effects
  qhandle_t   sparkFlareShader;

  //TA: media used for armour switching stuff

  //light armour
  qhandle_t larmourHeadSkin;
  qhandle_t larmourLegsSkin;
  qhandle_t larmourTorsoSkin;
  
  qhandle_t jetpackModel;
  qhandle_t jetpackFlashModel;
  qhandle_t battpackModel;
  
  sfxHandle_t repeaterUseSound;
  
  sfxHandle_t buildableRepairSound;
  sfxHandle_t buildableRepairedSound;

  qhandle_t   poisonCloudPS;
  qhandle_t   alienEvolvePS;
  qhandle_t   alienAcidTubePS;

  sfxHandle_t alienEvolveSound;
  
  qhandle_t   humanBuildableDamagedPS;
  qhandle_t   humanBuildableDestroyedPS;
  qhandle_t   alienBuildableDamagedPS;
  qhandle_t   alienBuildableDestroyedPS;

  sfxHandle_t lCannonWarningSound;

  qhandle_t   buildWeaponTimerPie[ 8 ];
} cgMedia_t;


// The client game static (cgs) structure hold everything
// loaded or calculated from the gamestate.  It will NOT
// be cleared when a tournement restart is done, allowing
// all clients to begin playing instantly
typedef struct
{
  gameState_t   gameState;              // gamestate from server
  glconfig_t    glconfig;               // rendering configuration
  float         screenXScale;           // derived from glconfig
  float         screenYScale;
  float         screenXBias;

  int           serverCommandSequence;  // reliable command stream counter
  int           processedSnapshotNum;   // the number of snapshots cgame has requested

  qboolean      localServer;            // detected on startup by checking sv_running

  // parsed from serverinfo
  int           dmflags;
  int           teamflags;
  int           timelimit;
  int           maxclients;
  char          mapname[ MAX_QPATH ];

  int           voteTime;
  int           voteYes;
  int           voteNo;
  qboolean      voteModified;           // beep whenever changed
  char          voteString[ MAX_STRING_TOKENS ];

  int           teamVoteTime[ 2 ];
  int           teamVoteYes[ 2 ];
  int           teamVoteNo[ 2 ];
  qboolean      teamVoteModified[ 2 ];  // beep whenever changed
  char          teamVoteString[ 2 ][ MAX_STRING_TOKENS ];

  int           levelStartTime;

  int           scores1, scores2;   // from configstrings

  qboolean      newHud;

  int           alienBuildPoints;
  int           alienBuildPointsTotal;
  int           humanBuildPoints;
  int           humanBuildPointsTotal;
  int           humanBuildPointsPowered;

  int           alienStage;
  int           humanStage;
  
  int           numAlienSpawns;
  int           numHumanSpawns;

  //
  // locally derived information from gamestate
  //
  qhandle_t     gameModels[ MAX_MODELS ];
  qhandle_t     gameShaders[ MAX_SHADERS ];
  qhandle_t     gameParticleSystems[ MAX_GAME_PARTICLE_SYSTEMS ];
  sfxHandle_t   gameSounds[ MAX_SOUNDS ];
  
  int           numInlineModels;
  qhandle_t     inlineDrawModel[ MAX_MODELS ];
  vec3_t        inlineModelMidpoints[ MAX_MODELS ];

  clientInfo_t  clientinfo[ MAX_CLIENTS ];
  
  //TA: corpse info
  clientInfo_t  corpseinfo[ MAX_CLIENTS ];

  // teamchat width is *3 because of embedded color codes
  char          teamChatMsgs[ TEAMCHAT_HEIGHT ][ TEAMCHAT_WIDTH * 3 + 1 ];
  int           teamChatMsgTimes[ TEAMCHAT_HEIGHT ];
  int           teamChatPos;
  int           teamLastChatPos;

  int           cursorX;
  int           cursorY;
  qboolean      eventHandling;
  qboolean      mouseCaptured;
  qboolean      sizingHud;
  void          *capturedItem;
  qhandle_t     activeCursor;

  // media
  cgMedia_t           media;
} cgs_t;

//==============================================================================

extern  cgs_t     cgs;
extern  cg_t      cg;
extern  centity_t cg_entities[ MAX_GENTITIES ];

//TA: weapon limit expanded:
//extern  weaponInfo_t  cg_weapons[MAX_WEAPONS];
extern  weaponInfo_t    cg_weapons[ 32 ];
//TA: upgrade infos:
extern  upgradeInfo_t   cg_upgrades[ 32 ];

//TA: buildable infos:
extern  buildableInfo_t cg_buildables[ BA_NUM_BUILDABLES ];

extern  markPoly_t      cg_markPolys[ MAX_MARK_POLYS ];

extern  vmCvar_t    cg_centertime;
extern  vmCvar_t    cg_runpitch;
extern  vmCvar_t    cg_runroll;
extern  vmCvar_t    cg_bobup;
extern  vmCvar_t    cg_bobpitch;
extern  vmCvar_t    cg_bobroll;
extern  vmCvar_t    cg_swingSpeed;
extern  vmCvar_t    cg_shadows;
extern  vmCvar_t    cg_gibs;
extern  vmCvar_t    cg_drawTimer;
extern  vmCvar_t    cg_drawFPS;
extern  vmCvar_t    cg_drawSnapshot;
extern  vmCvar_t    cg_draw3dIcons;
extern  vmCvar_t    cg_drawIcons;
extern  vmCvar_t    cg_drawAmmoWarning;
extern  vmCvar_t    cg_drawCrosshair;
extern  vmCvar_t    cg_drawCrosshairNames;
extern  vmCvar_t    cg_drawRewards;
extern  vmCvar_t    cg_drawTeamOverlay;
extern  vmCvar_t    cg_teamOverlayUserinfo;
extern  vmCvar_t    cg_crosshairX;
extern  vmCvar_t    cg_crosshairY;
extern  vmCvar_t    cg_drawStatus;
extern  vmCvar_t    cg_draw2D;
extern  vmCvar_t    cg_animSpeed;
extern  vmCvar_t    cg_debugAnim;
extern  vmCvar_t    cg_debugPosition;
extern  vmCvar_t    cg_debugEvents;
extern  vmCvar_t    cg_teslaTrailTime;
extern  vmCvar_t    cg_alienZapTime;
extern  vmCvar_t    cg_railTrailTime;
extern  vmCvar_t    cg_errorDecay;
extern  vmCvar_t    cg_nopredict;
extern  vmCvar_t    cg_debugMove;
extern  vmCvar_t    cg_noPlayerAnims;
extern  vmCvar_t    cg_showmiss;
extern  vmCvar_t    cg_footsteps;
extern  vmCvar_t    cg_addMarks;
extern  vmCvar_t    cg_brassTime;
extern  vmCvar_t    cg_gun_frame;
extern  vmCvar_t    cg_gun_x;
extern  vmCvar_t    cg_gun_y;
extern  vmCvar_t    cg_gun_z;
extern  vmCvar_t    cg_drawGun;
extern  vmCvar_t    cg_viewsize;
extern  vmCvar_t    cg_tracerChance;
extern  vmCvar_t    cg_tracerWidth;
extern  vmCvar_t    cg_tracerLength;
extern  vmCvar_t    cg_autoswitch;
extern  vmCvar_t    cg_ignore;
extern  vmCvar_t    cg_simpleItems;
extern  vmCvar_t    cg_fov;
extern  vmCvar_t    cg_zoomFov;
extern  vmCvar_t    cg_thirdPersonRange;
extern  vmCvar_t    cg_thirdPersonAngle;
extern  vmCvar_t    cg_thirdPerson;
extern  vmCvar_t    cg_stereoSeparation;
extern  vmCvar_t    cg_lagometer;
extern  vmCvar_t    cg_drawAttacker;
extern  vmCvar_t    cg_synchronousClients;
extern  vmCvar_t    cg_teamChatTime;
extern  vmCvar_t    cg_teamChatHeight;
extern  vmCvar_t    cg_stats;
extern  vmCvar_t    cg_forceModel;
extern  vmCvar_t    cg_buildScript;
extern  vmCvar_t    cg_paused;
extern  vmCvar_t    cg_blood;
extern  vmCvar_t    cg_predictItems;
extern  vmCvar_t    cg_deferPlayers;
extern  vmCvar_t    cg_drawFriend;
extern  vmCvar_t    cg_teamChatsOnly;
extern  vmCvar_t    cg_noVoiceChats;
extern  vmCvar_t    cg_noVoiceText;
extern  vmCvar_t    cg_scorePlum;
extern  vmCvar_t    cg_smoothClients;
extern  vmCvar_t    pmove_fixed;
extern  vmCvar_t    pmove_msec;
//extern  vmCvar_t    cg_pmove_fixed;
extern  vmCvar_t    cg_cameraOrbit;
extern  vmCvar_t    cg_cameraOrbitDelay;
extern  vmCvar_t    cg_timescaleFadeEnd;
extern  vmCvar_t    cg_timescaleFadeSpeed;
extern  vmCvar_t    cg_timescale;
extern  vmCvar_t    cg_cameraMode;
extern  vmCvar_t    cg_smallFont;
extern  vmCvar_t    cg_bigFont;
extern  vmCvar_t    cg_noTaunt;
extern  vmCvar_t    cg_noProjectileTrail;
extern  vmCvar_t    cg_oldRail;
extern  vmCvar_t    cg_oldRocket;
extern  vmCvar_t    cg_oldPlasma;
extern  vmCvar_t    cg_trueLightning;
extern  vmCvar_t    cg_creepRes;
extern  vmCvar_t    cg_drawSurfNormal;
extern  vmCvar_t    cg_drawBBOX;
extern  vmCvar_t    cg_debugAlloc;
extern  vmCvar_t    cg_wwSmoothTime;
extern  vmCvar_t    cg_wwFollow;
extern  vmCvar_t    cg_depthSortParticles;
extern  vmCvar_t    cg_consoleLatency;
extern  vmCvar_t    cg_lightFlare;
extern  vmCvar_t    cg_debugParticles;
extern  vmCvar_t    cg_debugPVS;
extern  vmCvar_t    cg_disableBuildWarnings;
extern  vmCvar_t    cg_disableScannerPlane;

//TA: hack to get class an carriage through to UI module
extern  vmCvar_t    ui_currentClass;
extern  vmCvar_t    ui_carriage;
extern  vmCvar_t    ui_stages;
extern  vmCvar_t    ui_dialog;
extern  vmCvar_t    ui_loading;
extern  vmCvar_t    ui_voteActive;
extern  vmCvar_t    ui_alienTeamVoteActive;
extern  vmCvar_t    ui_humanTeamVoteActive;

extern  vmCvar_t    cg_debugRandom;

//
// cg_main.c
//
const char  *CG_ConfigString( int index );
const char  *CG_Argv( int arg );

void        CG_TAUIConsole( const char *text );
void QDECL  CG_Printf( const char *msg, ... );
void QDECL  CG_Error( const char *msg, ... );

void        CG_StartMusic( void );

void        CG_UpdateCvars( void );

int         CG_CrosshairPlayer( void );
int         CG_LastAttacker( void );
void        CG_LoadMenus( const char *menuFile );
void        CG_KeyEvent( int key, qboolean down );
void        CG_MouseEvent( int x, int y );
void        CG_EventHandling( int type );
void        CG_SetScoreSelection( void *menu );
void        CG_BuildSpectatorString( );

qboolean    CG_FileExists( char *filename );
void        CG_RemoveConsoleLine( void );


//
// cg_view.c
//
void        CG_addSmoothOp( vec3_t rotAxis, float rotAngle, float timeMod ); //TA
void        CG_TestModel_f( void );
void        CG_TestGun_f( void );
void        CG_TestModelNextFrame_f( void );
void        CG_TestModelPrevFrame_f( void );
void        CG_TestModelNextSkin_f( void );
void        CG_TestModelPrevSkin_f( void );
void        CG_ZoomDown_f( void );
void        CG_ZoomUp_f( void );
void        CG_AddBufferedSound( sfxHandle_t sfx );
void        CG_DrawActiveFrame( int serverTime, stereoFrame_t stereoView, qboolean demoPlayback );


//
// cg_drawtools.c
//
void        CG_DrawPlane( vec3_t origin, vec3_t down, vec3_t right, qhandle_t shader );
void        CG_AdjustFrom640( float *x, float *y, float *w, float *h );
void        CG_FillRect( float x, float y, float width, float height, const float *color );
void        CG_DrawPic( float x, float y, float width, float height, qhandle_t hShader );
void        CG_DrawFadePic( float x, float y, float width, float height, vec4_t fcolor,
                            vec4_t tcolor, float amount, qhandle_t hShader );

int         CG_DrawStrlen( const char *str );

float       *CG_FadeColor( int startMsec, int totalMsec );
void        CG_TileClear( void );
void        CG_ColorForHealth( vec4_t hcolor );
void        CG_GetColorForHealth( int health, int armor, vec4_t hcolor );

void        CG_DrawRect( float x, float y, float width, float height, float size, const float *color );
void        CG_DrawSides(float x, float y, float w, float h, float size);
void        CG_DrawTopBottom(float x, float y, float w, float h, float size);


//
// cg_draw.c
//
extern  int sortedTeamPlayers[ TEAM_MAXOVERLAY ];
extern  int numSortedTeamPlayers;
extern  char systemChat[ 256 ];
extern  char teamChat1[ 256 ];
extern  char teamChat2[ 256 ];

void        CG_AddLagometerFrameInfo( void );
void        CG_AddLagometerSnapshotInfo( snapshot_t *snap );
void        CG_CenterPrint( const char *str, int y, int charWidth );
void        CG_DrawActive( stereoFrame_t stereoView );
void        CG_OwnerDraw( float x, float y, float w, float h, float text_x, float text_y,
                          int ownerDraw, int ownerDrawFlags, int align, float special,
                          float scale, vec4_t color, qhandle_t shader, int textStyle);
void        CG_Text_Paint( float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit, int style );
int         CG_Text_Width( const char *text, float scale, int limit );
int         CG_Text_Height( const char *text, float scale, int limit );
float       CG_GetValue(int ownerDraw);
void        CG_RunMenuScript(char **args);
void        CG_SetPrintString( int type, const char *p );
void        CG_InitTeamChat( );
void        CG_GetTeamColor( vec4_t *color );
const char  *CG_GetKillerText();
void        CG_Text_PaintChar( float x, float y, float width, float height, float scale,
                               float s, float t, float s2, float t2, qhandle_t hShader );
void        CG_DrawLoadingScreen( void );
void        CG_UpdateMediaFraction( float newFract );

//
// cg_player.c
//
void        CG_Player( centity_t *cent );
void        CG_Corpse( centity_t *cent );
void        CG_ResetPlayerEntity( centity_t *cent );
void        CG_AddRefEntityWithPowerups( refEntity_t *ent, int powerups, int team );
void        CG_NewClientInfo( int clientNum );
void        CG_PrecacheClientInfo( pClass_t class, char *model, char *skin );
sfxHandle_t CG_CustomSound( int clientNum, const char *soundName );

//
// cg_buildable.c
//
void        CG_GhostBuildable( buildable_t buildable );
void        CG_Buildable( centity_t *cent );
void        CG_InitBuildables( );
void        CG_HumanBuildableExplosion( vec3_t origin, vec3_t dir );
void        CG_AlienBuildableExplosion( vec3_t origin, vec3_t dir );

//
// cg_animmapobj.c
//
void        CG_animMapObj( centity_t *cent );
void        CG_ModelDoor( centity_t *cent );

//
// cg_predict.c
//

#define MAGIC_TRACE_HACK -2

void        CG_BuildSolidList( void );
int         CG_PointContents( const vec3_t point, int passEntityNum );
void        CG_Trace( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, 
                      int skipNumber, int mask );
void        CG_CapTrace( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, 
                         int skipNumber, int mask );
void        CG_PredictPlayerState( void );


//
// cg_events.c
//
void        CG_CheckEvents( centity_t *cent );
void        CG_EntityEvent( centity_t *cent, vec3_t position );
void        CG_PainEvent( centity_t *cent, int health );


//
// cg_ents.c
//
void        CG_DrawBoundingBox( vec3_t origin, vec3_t mins, vec3_t maxs );
void        CG_SetEntitySoundPosition( centity_t *cent );
void        CG_AddPacketEntities( void );
void        CG_Beam( centity_t *cent );
void        CG_AdjustPositionForMover( const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out );

void        CG_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
                                    qhandle_t parentModel, char *tagName );
void        CG_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
                                           qhandle_t parentModel, char *tagName );




//
// cg_weapons.c
//
void        CG_NextWeapon_f( void );
void        CG_PrevWeapon_f( void );
void        CG_Weapon_f( void );

void        CG_InitUpgrades( );
void        CG_RegisterUpgrade( int upgradeNum );
void        CG_InitWeapons( );
void        CG_RegisterWeapon( int weaponNum );

void        CG_FireWeapon( centity_t *cent, weaponMode_t weaponMode );
void        CG_MissileHitWall( weapon_t weapon, weaponMode_t weaponMode, int clientNum,
                               vec3_t origin, vec3_t dir, impactSound_t soundType );
void        CG_MissileHitPlayer( weapon_t weapon, weaponMode_t weaponMode, vec3_t origin, vec3_t dir, int entityNum );
void        CG_Bullet( vec3_t origin, int sourceEntityNum, vec3_t normal, qboolean flesh, int fleshEntityNum );
void        CG_ShotgunFire( entityState_t *es );

void        CG_TeslaTrail( vec3_t start, vec3_t end, int srcENum, int destENum );
void        CG_AlienZap( vec3_t start, vec3_t end, int srcENum, int destENum );
void        CG_AddViewWeapon (playerState_t *ps);
void        CG_AddPlayerWeapon( refEntity_t *parent, playerState_t *ps, centity_t *cent );
void        CG_DrawItemSelect( rectDef_t *rect, vec4_t color );
void        CG_DrawItemSelectText( rectDef_t *rect, float scale, int textStyle );


//
// cg_scanner.c
//
void        CG_UpdateEntityPositions( void );
void        CG_Scanner( rectDef_t *rect, qhandle_t shader, vec4_t color );
void        CG_AlienSense( rectDef_t *rect );

//
// cg_marks.c
//
void        CG_InitMarkPolys( void );
void        CG_AddMarks( void );
void        CG_ImpactMark( qhandle_t markShader,
                           const vec3_t origin, const vec3_t dir,
                           float orientation,
                           float r, float g, float b, float a,
                           qboolean alphaFade,
                           float radius, qboolean temporary );

//
// cg_localents.c
//
void          CG_InitLocalEntities( void );
localEntity_t *CG_AllocLocalEntity( void );
void          CG_AddLocalEntities( void );

//
// cg_effects.c
//
localEntity_t *CG_SmokePuff( const vec3_t p,
                             const vec3_t vel,
                             float radius,
                             float r, float g, float b, float a,
                             float duration,
                             int startTime,
                             int fadeInTime,
                             int leFlags,
                             qhandle_t hShader );
void          CG_BubbleTrail( vec3_t start, vec3_t end, float spacing );
void          CG_SpawnEffect( vec3_t org );
void          CG_GibPlayer( vec3_t playerOrigin );
void          CG_BigExplode( vec3_t playerOrigin );

void          CG_Bleed( vec3_t origin, int entityNum );

localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir,
                                 qhandle_t hModel, qhandle_t shader, int msec,
                                 qboolean isSprite );

//TA: wolf tesla effect
void          CG_DynamicLightningBolt( qhandle_t shader, vec3_t start, vec3_t pend,
                                       int numBolts, float maxWidth, qboolean fade,
                                       float startAlpha, int recursion, int randseed );

// Ridah, trails
//
// cg_trails.c
//
int           CG_AddTrailJunc( int headJuncIndex, qhandle_t shader, int spawnTime,
                             int sType, vec3_t pos, int trailLife, float alphaStart,
                             float alphaEnd, float startWidth, float endWidth, int flags,
                             vec3_t colorStart, vec3_t colorEnd, float sRatio, float animSpeed );
int           CG_AddSparkJunc( int headJuncIndex, qhandle_t shader, vec3_t pos, int trailLife,
                             float alphaStart, float alphaEnd, float startWidth, float endWidth );
int           CG_AddSmokeJunc( int headJuncIndex, qhandle_t shader, vec3_t pos, int trailLife,
                             float alpha, float startWidth, float endWidth ); 
int           CG_AddFireJunc( int headJuncIndex, qhandle_t shader, vec3_t pos, int trailLife,
                            float alpha, float startWidth, float endWidth );
void          CG_AddTrails( void );
void          CG_ClearTrails( void );
// done.


//
// cg_snapshot.c
//
void          CG_ProcessSnapshots( void );

//
// cg_consolecmds.c
//
qboolean      CG_ConsoleCommand( void );
void          CG_InitConsoleCommands( void );

//
// cg_servercmds.c
//
void          CG_ExecuteNewServerCommands( int latestSequence );
void          CG_ParseServerinfo( void );
void          CG_SetConfigValues( void );
void          CG_ShaderStateChanged(void);

//
// cg_playerstate.c
//
void          CG_Respawn( void );
void          CG_TransitionPlayerState( playerState_t *ps, playerState_t *ops );
void          CG_CheckChangedPredictableEvents( playerState_t *ps );

//
// cg_mem.c
//
void          CG_InitMemory( void );
void          *CG_Alloc( int size );
void          CG_Free( void *ptr );
void          CG_DefragmentMemory( void );

//
// cg_mp3decoder.c
//
qboolean      S_decodeMP3( char *mp3File, char *wavFile );

//
// cg_particles.c
//
void                CG_LoadParticleSystems( void );
qhandle_t           CG_RegisterParticleSystem( char *name );

particleSystem_t    *CG_SpawnNewParticleSystem( qhandle_t psHandle );
void                CG_DestroyParticleSystem( particleSystem_t **ps );

qboolean            CG_IsParticleSystemInfinite( particleSystem_t *ps );
qboolean            CG_IsParticleSystemValid( particleSystem_t **ps );
  
void                CG_SetParticleSystemCent( particleSystem_t *ps, centity_t *cent );
void                CG_AttachParticleSystemToCent( particleSystem_t *ps );
void                CG_SetParticleSystemTag( particleSystem_t *ps, refEntity_t parent, qhandle_t model, char *tagName );
void                CG_AttachParticleSystemToTag( particleSystem_t *ps );
void                CG_SetParticleSystemOrigin( particleSystem_t *ps, vec3_t origin );
void                CG_AttachParticleSystemToOrigin( particleSystem_t *ps );
void                CG_SetParticleSystemNormal( particleSystem_t *ps, vec3_t normal );
void                CG_AttachParticleSystemToParticle( particleSystem_t *ps );
void                CG_SetParticleSystemParentParticle( particleSystem_t *ps, particle_t *p );

void                CG_AddParticles( void );

void                CG_ParticleSystemEntity( centity_t *cent );

//
// cg_ptr.c
// 
int   CG_ReadPTRCode( void );
void  CG_WritePTRCode( int code );
  
//
//===============================================

//
// system traps
// These functions are how the cgame communicates with the main game system
//


// print message on the local console
void          trap_Print( const char *fmt );

// abort the game
void          trap_Error( const char *fmt );

// milliseconds should only be used for performance tuning, never
// for anything game related.  Get time from the CG_DrawActiveFrame parameter
int           trap_Milliseconds( void );

// console variable interaction
void          trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
void          trap_Cvar_Update( vmCvar_t *vmCvar );
void          trap_Cvar_Set( const char *var_name, const char *value );
void          trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

// ServerCommand and ConsoleCommand parameter access
int           trap_Argc( void );
void          trap_Argv( int n, char *buffer, int bufferLength );
void          trap_Args( char *buffer, int bufferLength );

// filesystem access
// returns length of file
int           trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void          trap_FS_Read( void *buffer, int len, fileHandle_t f );
void          trap_FS_Write( const void *buffer, int len, fileHandle_t f );
void          trap_FS_FCloseFile( fileHandle_t f );
void          trap_FS_Seek( fileHandle_t f, long offset, fsOrigin_t origin ); // fsOrigin_t

// add commands to the local console as if they were typed in
// for map changing, etc.  The command is not executed immediately,
// but will be executed in order the next time console commands
// are processed
void          trap_SendConsoleCommand( const char *text );

// register a command name so the console can perform command completion.
// FIXME: replace this with a normal console command "defineCommand"?
void          trap_AddCommand( const char *cmdName );

// send a string to the server over the network
void          trap_SendClientCommand( const char *s );

// force a screen update, only used during gamestate load
void          trap_UpdateScreen( void );

// model collision
void          trap_CM_LoadMap( const char *mapname );
int           trap_CM_NumInlineModels( void );
clipHandle_t  trap_CM_InlineModel( int index );    // 0 = world, 1+ = bmodels
clipHandle_t  trap_CM_TempBoxModel( const vec3_t mins, const vec3_t maxs );
int           trap_CM_PointContents( const vec3_t p, clipHandle_t model );
int           trap_CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles );
void          trap_CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
                                const vec3_t mins, const vec3_t maxs,
                                clipHandle_t model, int brushmask );
void          trap_CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
                                           const vec3_t mins, const vec3_t maxs,
                                           clipHandle_t model, int brushmask,
                                           const vec3_t origin, const vec3_t angles );
void          trap_CM_CapsuleTrace( trace_t *results, const vec3_t start, const vec3_t end,
                const vec3_t mins, const vec3_t maxs,
                clipHandle_t model, int brushmask );
void          trap_CM_TransformedCapsuleTrace( trace_t *results, const vec3_t start, const vec3_t end,
                const vec3_t mins, const vec3_t maxs,
                clipHandle_t model, int brushmask,
                const vec3_t origin, const vec3_t angles );

// Returns the projection of a polygon onto the solid brushes in the world
int           trap_CM_MarkFragments( int numPoints, const vec3_t *points,
                                     const vec3_t projection,
                                     int maxPoints, vec3_t pointBuffer,
                                     int maxFragments, markFragment_t *fragmentBuffer );

// normal sounds will have their volume dynamically changed as their entity
// moves and the listener moves
void          trap_S_StartSound( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx );
void          trap_S_StopLoopingSound( int entnum );

// a local sound is always played full volume
void          trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum );
void          trap_S_ClearLoopingSounds( qboolean killall );
void          trap_S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void          trap_S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void          trap_S_UpdateEntityPosition( int entityNum, const vec3_t origin );

// respatialize recalculates the volumes of sound as they should be heard by the
// given entityNum and position
void          trap_S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );
sfxHandle_t   trap_S_RegisterSound( const char *sample, qboolean compressed );    // returns buzz if not found
void          trap_S_StartBackgroundTrack( const char *intro, const char *loop ); // empty name stops music
void          trap_S_StopBackgroundTrack( void );


void          trap_R_LoadWorldMap( const char *mapname );

// all media should be registered during level startup to prevent
// hitches during gameplay
qhandle_t     trap_R_RegisterModel( const char *name );     // returns rgb axis if not found
qhandle_t     trap_R_RegisterSkin( const char *name );      // returns all white if not found
qhandle_t     trap_R_RegisterShader( const char *name );      // returns all white if not found
qhandle_t     trap_R_RegisterShaderNoMip( const char *name );     // returns all white if not found

// a scene is built up by calls to R_ClearScene and the various R_Add functions.
// Nothing is drawn until R_RenderScene is called.
void          trap_R_ClearScene( void );
void          trap_R_AddRefEntityToScene( const refEntity_t *re );

// polys are intended for simple wall marks, not really for doing
// significant construction
void          trap_R_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts );
void          trap_R_AddPolysToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts, int numPolys );
void          trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void          trap_R_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b );
int           trap_R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
void          trap_R_RenderScene( const refdef_t *fd );
void          trap_R_SetColor( const float *rgba ); // NULL = 1,1,1,1
void          trap_R_DrawStretchPic( float x, float y, float w, float h,
                                     float s1, float t1, float s2, float t2, qhandle_t hShader );
void          trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs );
int           trap_R_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, 
                              float frac, const char *tagName );
void          trap_R_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset );

// The glconfig_t will not change during the life of a cgame.
// If it needs to change, the entire cgame will be restarted, because
// all the qhandle_t are then invalid.
void          trap_GetGlconfig( glconfig_t *glconfig );

// the gamestate should be grabbed at startup, and whenever a
// configstring changes
void          trap_GetGameState( gameState_t *gamestate );

// cgame will poll each frame to see if a newer snapshot has arrived
// that it is interested in.  The time is returned seperately so that
// snapshot latency can be calculated.
void          trap_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime );

// a snapshot get can fail if the snapshot (or the entties it holds) is so
// old that it has fallen out of the client system queue
qboolean      trap_GetSnapshot( int snapshotNumber, snapshot_t *snapshot );

// retrieve a text command from the server stream
// the current snapshot will hold the number of the most recent command
// qfalse can be returned if the client system handled the command
// argc() / argv() can be used to examine the parameters of the command
qboolean      trap_GetServerCommand( int serverCommandNumber );

// returns the most recent command number that can be passed to GetUserCmd
// this will always be at least one higher than the number in the current
// snapshot, and it may be quite a few higher if it is a fast computer on
// a lagged connection
int           trap_GetCurrentCmdNumber( void );

qboolean      trap_GetUserCmd( int cmdNumber, usercmd_t *ucmd );

// used for the weapon select and zoom
void          trap_SetUserCmdValue( int stateValue, float sensitivityScale );

// aids for VM testing
void          testPrintInt( char *string, int i );
void          testPrintFloat( char *string, float f );

int           trap_MemoryRemaining( void );
void          trap_R_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
qboolean      trap_Key_IsDown( int keynum );
int           trap_Key_GetCatcher( void );
void          trap_Key_SetCatcher( int catcher );
int           trap_Key_GetKey( const char *binding );

typedef enum
{
  SYSTEM_PRINT,
  CHAT_PRINT,
  TEAMCHAT_PRINT
} q3print_t;


int           trap_CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits );
e_status      trap_CIN_StopCinematic( int handle );
e_status      trap_CIN_RunCinematic( int handle );
void          trap_CIN_DrawCinematic( int handle );
void          trap_CIN_SetExtents( int handle, int x, int y, int w, int h );

void          trap_SnapVector( float *v );

qboolean      trap_loadCamera( const char *name );
void          trap_startCamera( int time );
qboolean      trap_getCameraInfo( int time, vec3_t *origin, vec3_t *angles );

qboolean      trap_GetEntityToken( char *buffer, int bufferSize );
