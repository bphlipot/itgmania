#include "global.h"
/*
-----------------------------------------------------------------------------
 Class: GrooveRadar

 Desc: See header.

 Copyright (c) 2001-2002 by the person(s) listed below.  All rights reserved.
	Chris Danford
-----------------------------------------------------------------------------
*/

#include "GrooveRadar.h"
#include "RageUtil.h"
#include "PrefsManager.h"
#include "RageBitmapTexture.h"
#include "GameConstantsAndTypes.h"
#include "ThemeManager.h"
#include "Notes.h"
#include "RageDisplay.h"
#include "RageMath.h"
#include <math.h>


#define LABEL_OFFSET_X( i )			THEME->GetMetricF("GrooveRadar",ssprintf("Label%dOffsetX",i+1))
#define LABEL_OFFSET_Y( i )			THEME->GetMetricF("GrooveRadar",ssprintf("Label%dOffsetY",i+1))
#define LABEL_ON_COMMAND			THEME->GetMetric ("GrooveRadar","LabelOnCommand")
#define LABEL_ON_DELAY				THEME->GetMetricF("GrooveRadar","LabelOnDelay")
#define LABEL_ON_COMMAND_POST_DELAY THEME->GetMetric ("GrooveRadar","LabelOnCommandPostDelay")
#define DISABLE_RADAR				THEME->GetMetricI("GrooveRadar","DisableRadar")


float RADAR_VALUE_ROTATION( int iValueIndex ) {	return PI/2 + PI*2 / 5.0f * iValueIndex; }

const float RADAR_EDGE_WIDTH	= 2;

GrooveRadar::GrooveRadar()
{
	this->AddChild( &m_GrooveRadarValueMap );

	for( int c=0; c<NUM_RADAR_CATEGORIES; c++ )
	{
		m_sprRadarLabels[c].Load( THEME->GetPathToG("GrooveRadar labels 1x5") );
		m_sprRadarLabels[c].StopAnimating();
		m_sprRadarLabels[c].SetState( c );
		m_sprRadarLabels[c].SetXY( LABEL_OFFSET_X(c), LABEL_OFFSET_Y(c) );
		this->AddChild( &m_sprRadarLabels[c] );
	}
}

void GrooveRadar::TweenOnScreen()
{
	for( int c=0; c<NUM_RADAR_CATEGORIES; c++ )
	{
		m_sprRadarLabels[c].SetX( LABEL_OFFSET_X(c) );
		m_sprRadarLabels[c].Command( LABEL_ON_COMMAND );
		m_sprRadarLabels[c].BeginTweening( LABEL_ON_DELAY*c );	// sleep
		m_sprRadarLabels[c].Command( LABEL_ON_COMMAND_POST_DELAY );
	}
	m_GrooveRadarValueMap.TweenOnScreen();
}

void GrooveRadar::TweenOffScreen()
{
	for( int c=0; c<NUM_RADAR_CATEGORIES; c++ )
	{
		m_sprRadarLabels[c].StopTweening();
		m_sprRadarLabels[c].BeginTweening( 0.2f );
		/* Make sure we undo glow.  We do this at the end of TweenIn,
		 * but we might tween off before we complete tweening in, and
		 * the glow can remain. */
		m_sprRadarLabels[c].SetGlow( RageColor(1,1,1,0) );
		m_sprRadarLabels[c].SetDiffuse( RageColor(1,1,1,0) );
	}
	m_GrooveRadarValueMap.TweenOffScreen();
}

GrooveRadar::GrooveRadarValueMap::GrooveRadarValueMap()
{
	m_sprRadarBase.Load( THEME->GetPathToG("GrooveRadar base") );
	this->AddChild( &m_sprRadarBase );

	for( int p=0; p<NUM_PLAYERS; p++ )
	{
		m_bValuesVisible[p] = false;
		m_PercentTowardNew[p] = 0;

		for( int c=0; c<NUM_RADAR_CATEGORIES; c++ )
		{
			m_fValuesNew[p][c] = 0;
			m_fValuesOld[p][c] = 0;
		}
	}
}

void GrooveRadar::GrooveRadarValueMap::SetFromNotes( PlayerNumber pn, Notes* pNotes )		// NULL means no song
{
	if(DISABLE_RADAR == 1) // if the theme says not to disable it
		return;
	if( pNotes != NULL )
	{
		for( int c=0; c<NUM_RADAR_CATEGORIES; c++ )
		{
			const float fValueCurrent = m_fValuesOld[pn][c] * (1-m_PercentTowardNew[pn]) + m_fValuesNew[pn][c] * m_PercentTowardNew[pn];
			m_fValuesOld[pn][c] = fValueCurrent;
			m_fValuesNew[pn][c] = pNotes->GetRadarValues()[c];
		}	

		if( m_bValuesVisible[pn] == false )	// the values WERE invisible
			m_PercentTowardNew[pn] = 1;
		else
			m_PercentTowardNew[pn] = 0;	

		m_bValuesVisible[pn] = true;
	}
	else	// pNotes == NULL
	{
		m_bValuesVisible[pn] = false;
	}
}

void GrooveRadar::GrooveRadarValueMap::Update( float fDeltaTime )
{
	ActorFrame::Update( fDeltaTime );

	if(DISABLE_RADAR != 1)
	{
		for( int p=0; p<NUM_PLAYERS; p++ )
		{
			m_PercentTowardNew[p] = min( m_PercentTowardNew[p]+4.0f*fDeltaTime, 1 );
		}
	}
}

void GrooveRadar::GrooveRadarValueMap::DrawPrimitives()
{
	ActorFrame::DrawPrimitives();

	if(DISABLE_RADAR == 1) // if they disabled the radar
		return; // get out of here...
	// draw radar filling
	const float fRadius = m_sprRadarBase.GetZoomedHeight()/2.0f*1.1f;

	DISPLAY->SetTexture( NULL );
	DISPLAY->SetTextureModeModulate();
	RageSpriteVertex v[12];	// needed to draw 5 fan primitives and 10 strip primitives

	for( int p=0; p<NUM_PLAYERS; p++ )
	{
		if( !m_bValuesVisible[p] )
			continue;

		//
		// use a fan to draw the volume
		//
		RageColor color = PlayerToColor( (PlayerNumber)p );
		color.a = 0.5f;
		v[0].p = RageVector3( 0, 0, 0 );
		v[0].c = color;

		int i;

		for( i=0; i<NUM_RADAR_CATEGORIES+1; i++ )	// do one extra to close the fan
		{
			const int c = i%NUM_RADAR_CATEGORIES;
			const float fDistFromCenter = 
				( m_fValuesOld[p][c] * (1-m_PercentTowardNew[p]) + m_fValuesNew[p][c] * m_PercentTowardNew[p] + 0.07f ) * fRadius;
			const float fRotation = RADAR_VALUE_ROTATION(i);
			const float fX = cosf(fRotation) * fDistFromCenter;
			const float fY = -sinf(fRotation) * fDistFromCenter;

			v[1+i].p = RageVector3( fX, fY,	0 );
			v[1+i].c = v[0].c;
		}

		DISPLAY->DrawFan( v, NUM_RADAR_CATEGORIES+2 );

		//
		// use a line loop to draw the thick line
		//
		for( i=0; i<=NUM_RADAR_CATEGORIES; i++ )
		{
			const int c = i%NUM_RADAR_CATEGORIES;
			const float fDistFromCenter = 
				( m_fValuesOld[p][c] * (1-m_PercentTowardNew[p]) + m_fValuesNew[p][c] * m_PercentTowardNew[p] + 0.07f ) * fRadius;
			const float fRotation = RADAR_VALUE_ROTATION(i);
			const float fX = cosf(fRotation) * fDistFromCenter;
			const float fY = -sinf(fRotation) * fDistFromCenter;

			v[i].p = RageVector3( fX, fY, 0 );
			v[i].c = PlayerToColor( (PlayerNumber)p );
		}

		// TODO: Add this back in.  -Chris
//		switch( PREFSMAN->m_iPolygonRadar )
//		{
//		case 0:		DISPLAY->DrawLoop_LinesAndPoints( v, NUM_RADAR_CATEGORIES, RADAR_EDGE_WIDTH );	break;
//		case 1:		DISPLAY->DrawLoop_Polys( v, NUM_RADAR_CATEGORIES, RADAR_EDGE_WIDTH );			break;
//		default:
//		case -1:
		DISPLAY->DrawLineStrip( v, NUM_RADAR_CATEGORIES+1, RADAR_EDGE_WIDTH );
//		break;
//		}
	}
}

void GrooveRadar::GrooveRadarValueMap::TweenOnScreen()
{
	SetZoom( 0.5f );
	SetRotationZ( 720 );
	BeginTweening( 0.6f );
	SetZoom( 1 );
	SetRotationZ( 0 );
}

void GrooveRadar::GrooveRadarValueMap::TweenOffScreen()
{
	BeginTweening( 0.6f );
	SetRotationZ( 180*4 );
	SetZoom( 0 );
}
