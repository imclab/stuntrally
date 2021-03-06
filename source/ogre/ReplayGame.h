#pragma once
#include "../vdrift/mathvector.h"
#include "../vdrift/quaternion.h"
#include <OgreVector3.h>

const static int ciRplHdrSize = 1024;
const static int cDefSize = 8*1024;
const static int ciTrkHdrSize = 32;


// note: add new vars always at end of
// ReplayHeader or ReplayFrame structs (backward compatibility)

/// size of ReplayFrame
//= 384 Bytes per frame
//  384 * 160 fps = 61.4 kB/s
//  1 min = 3.68 MB, 10 min = 36.8 MB


//  whole replay/ghost data - max 4 players
//--------------------------------------------
struct ReplayHeader
{
	char head[5];  // "SR\_ "
	typedef char ChName[32];

	char track[63];   // track name  (hmap crc? diff-)
	char track_user;  // user/original
	ChName car;       // car name  (eg. ES, .car file crc?, settings diff-)

	int ver, frameSize;  // bin data format - sizeof(ReplayFrame)
	float whR[4][4];  // cars wheels radius
	
	int numPlayers;
	float hue[4],sat[4],val[4];  // cars colors
	ChName cars[3];   // car names (when numPlayers > 1)

	ChName nicks[4];  // multiplayer nicks
	char descr[128];  // description - user text
	float trees;      // trees multipler
	char num_laps;
	char networked;   // if 1, was networked, so use nicks when playing
	char sim_mode[32];// easy, normal, etc

	ReplayHeader();
	void Default(), SafeEnd0();
};


//  car data, for each simulation frame
//--------------------------------------------
struct ReplayFrame
{
	//  time  since game start
	double time;
	//  car & wheels
	MATHVECTOR<float,3> pos, whPos[4];
	QUATERNION<float> rot, whRot[4];

	//  hud
	float rpm,vel, clutch;  int gear;  //char

	//  sound, input
	float throttle, steer;
	MATHVECTOR<float,3> posEngn;  //snd engine pos --
	float speed, dynVel;

	//  wheel trails, particles, snd
	char surfType[4], whTerMtr[4];  //TRACKSURFACE::TYPE
	float squeal[4], slide[4], whVel[4];
	float suspVel[4], suspDisp[4];
	
	float fboost;  // input, particles
	char whRoadMtr[4];
	
	//  fluids
	float whH[4], whAngVel[4];  // submerge height
	char whP[4];  //particle type
	float whSteerAng[2];
	
	float percent;  // track % val
	char braking;  // for rear car lights (bool)

	//  hit sparks
	float fHitTime, fParIntens,fParVel;//, fSndForce, fNormVel;
	Ogre::Vector3 vHitPos,vHitNorm;  // world hit data
	float whMudSpin, fHitForce, fCarScrap, fCarScreech;

	ReplayFrame();
};


//  reduced data, for track's ghost
//--------------------------------------------
/*struct TrackFramePacked  // in file
{
	short rot[4];
};/**/
struct TrackFrame  // for game
{
	//  time  since game start
	float time;
	//  car,  no wheels
	MATHVECTOR<float,3> pos;
	QUATERNION<float> rot;

	//  info
	char brake, steer;
	//short vel;  char gear;
	
	TrackFrame();
};


//  only data for car sim, to rewind back
//--------------------------------------------
struct RewindFrame
{
	//  time  since game start
	double time;
	//  car
	MATHVECTOR<float,3> pos, vel, angvel;
	QUATERNION<float> rot;
	float fDamage;
	//engine rpm?..
};


///  Replay
//--------------------------------------------
class Replay
{
public:
	Replay();
	
	bool LoadFile(std::string file, bool onlyHdr=false);
	bool SaveFile(std::string file);

	void AddFrame(const ReplayFrame& frame, int carNum);    // record
	bool GetFrame(double time, ReplayFrame* fr, int carNum);  // play
	const ReplayFrame& GetFrame0(int id){  return frames[0][id];  }

	const double GetTimeLength(int carNum=0) const;  // total time in seconds
	const int GetNumFrames() const {  return frames[0].size();  }

	//  inits only basic header data, fill the rest after
	void InitHeader(const char* track, bool trk_user, const char* car, bool bClear);
	void Clear();
	void CopyFrom(const Replay& rpl);
	void DeleteFrames(int carNum, double fromTime);

	ReplayHeader header;
private:
	std::vector<ReplayFrame> frames[4];  // 4 players max
};


///  Rewind
//  to move car back in time, not saved in file, can have less frames than replay
//--------------------------------------------
class Rewind
{
public:
	Rewind();

	void AddFrame(const RewindFrame& frame, int carNum);
	bool GetFrame(double time, RewindFrame* fr, int carNum);

	const double GetTimeLength(int carNum=0) const;  // total time in seconds

	void Clear();
private:
	std::vector<RewindFrame> frames[4];  // 4 players max
	int idLast[4];  // last index from GetFrame (optym)
};


//  Track's ghost header
//--------------------------------------------
struct TrackHeader
{
	int ver;
	int frameSize;

	TrackHeader();
	void Default();
};

///  Track's ghost
//--------------------------------------------
class TrackGhost
{
public:
	TrackGhost();

	bool LoadFile(std::string file);
	bool SaveFile(std::string file);

	void AddFrame(const TrackFrame& frame);
	bool GetFrame(float time, TrackFrame* fr);

	const float GetTimeLength() const;
	void Clear();

	TrackHeader header;
//private:
	std::vector<TrackFrame> frames;
	int idLast;  // last index from GetFrame
};
