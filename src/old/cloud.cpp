// * This file is part of the COLOBOT source code
// * Copyright (C) 2001-2008, Daniel ROUX & EPSITEC SA, www.epsitec.ch
// *
// * This program is free software: you can redistribute it and/or modify
// * it under the terms of the GNU General Public License as published by
// * the Free Software Foundation, either version 3 of the License, or
// * (at your option) any later version.
// *
// * This program is distributed in the hope that it will be useful,
// * but WITHOUT ANY WARRANTY; without even the implied warranty of
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// * GNU General Public License for more details.
// *
// * You should have received a copy of the GNU General Public License
// * along with this program. If not, see  http://www.gnu.org/licenses/.


#include <windows.h>
#include <stdio.h>
#include <d3d.h>

#include "common/struct.h"
#include "math/geometry.h"
#include "math/conv.h"
#include "old/d3dengine.h"
#include "old/d3dmath.h"
#include "old/d3dutil.h"
#include "common/event.h"
#include "common/misc.h"
#include "common/iman.h"
#include "old/math3d.h"
#include "old/terrain.h"
#include "object/object.h"
#include "old/cloud.h"



const int DIMEXPAND = 4;	// extension of the dimensions



// Constructor of clouds.

CCloud::CCloud(CInstanceManager* iMan, CD3DEngine* engine)
{
	m_iMan = iMan;
	m_iMan->AddInstance(CLASS_CLOUD, this);

	m_engine = engine;
	m_terrain = 0;

	m_level = 0.0f;
	m_wind  = Math::Vector(0.0f, 0.0f, 0.0f);
	m_subdiv = 8;
	m_filename[0] = 0;
	m_bEnable = true;
}

// Destructor of clouds.

CCloud::~CCloud()
{
}


bool CCloud::EventProcess(const Event &event)
{
	if ( event.event == EVENT_FRAME )
	{
		return EventFrame(event);
	}

	return true;
}

// Makes the clouds evolve.

bool CCloud::EventFrame(const Event &event)
{
	if ( m_engine->RetPause() )  return true;

	m_time += event.rTime;

	if ( m_level == 0.0f )  return true;

	if ( m_time-m_lastTest < 0.2f )  return true;
	m_lastTest = m_time;

	return true;
}


// Adjusts the position to normal, to imitate the clouds
// at movement.

void CCloud::AdjustLevel(Math::Vector &pos, Math::Vector &eye, float deep,
						 Math::Point &uv1, Math::Point &uv2)
{
	float		dist, factor;

	uv1.x = (pos.x+20000.0f)/1280.0f;
	uv1.y = (pos.z+20000.0f)/1280.0f;
	uv1.x -= m_time*(m_wind.x/100.0f);
	uv1.y -= m_time*(m_wind.z/100.0f);

	uv2.x = 0.0f;
	uv2.y = 0.0f;

	dist = Math::DistanceProjected(pos, eye);
	factor = powf(dist/deep, 2.0f);
	pos.y -= m_level*factor*10.0f;
}

inline DWORD F2DW( FLOAT f )
{
	return *((DWORD*)&f);
}

// Draw the clouds.

void CCloud::Draw()
{
	LPDIRECT3DDEVICE7 device;
	D3DVERTEX2*		vertex;
	Math::Matrix*		matView;
	D3DMATERIAL7	material;
	Math::Matrix		matrix;
	Math::Vector		n, pos, p, eye;
	Math::Point			uv1, uv2;
	float			iDeep, deep, size, fogStart, fogEnd;
	int				i, j, u;

	if ( !m_bEnable )  return;
	if ( m_level == 0.0f )  return;
	if ( m_lineUsed == 0 )  return;

	vertex = (D3DVERTEX2*)malloc(sizeof(D3DVERTEX2)*(m_brick+2)*2);

	iDeep = m_engine->RetDeepView();
	deep = (m_brick*m_size)/2.0f;
	m_engine->SetDeepView(deep);
	m_engine->SetFocus(m_engine->RetFocus());
	m_engine->UpdateMatProj();  // increases the depth of view

//?	fogStart = deep*0.10f;
//?	fogEnd   = deep*0.16f;
	fogStart = deep*0.15f;
	fogEnd   = deep*0.24f;

	device = m_engine->RetD3DDevice();
	device->SetRenderState(D3DRENDERSTATE_AMBIENT, 0x00000000);
	device->SetRenderState(D3DRENDERSTATE_LIGHTING, false);
	device->SetRenderState(D3DRENDERSTATE_ZENABLE, false);
//?	device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, true);
	device->SetRenderState(D3DRENDERSTATE_FOGENABLE, true);
	device->SetRenderState(D3DRENDERSTATE_FOGSTART, F2DW(fogStart));
	device->SetRenderState(D3DRENDERSTATE_FOGEND,   F2DW(fogEnd));

	matView = m_engine->RetMatView();
	{
	  D3DMATRIX mat = MAT_TO_D3DMAT(*matView);
	  device->SetTransform(D3DTRANSFORMSTATE_VIEW, &mat);
	}

	ZeroMemory( &material, sizeof(D3DMATERIAL7) );
	material.diffuse = m_diffuse;
	material.ambient = m_ambient;
	m_engine->SetMaterial(material);

	m_engine->SetTexture(m_filename, 0);
	m_engine->SetTexture(m_filename, 1);

//?	m_engine->SetState(D3DSTATETTb|D3DSTATEDUALw|D3DSTATEWRAP);
	m_engine->SetState(D3DSTATETTb|D3DSTATEFOG|D3DSTATEWRAP);
//?	m_engine->SetState(D3DSTATEWRAP);

	matrix.LoadIdentity();
	{
	  D3DMATRIX mat = MAT_TO_D3DMAT(matrix);
	  device->SetTransform(D3DTRANSFORMSTATE_WORLD, &mat);
	}

	size = m_size/2.0f;
	eye = m_engine->RetEyePt();
	n = Math::Vector(0.0f, -1.0f, 0.0f);

	// Draws all the lines.
	for ( i=0 ; i<m_lineUsed ; i++ )
	{
		pos.y = m_level;
		pos.z = m_line[i].pz;
		pos.x = m_line[i].px1;

		u = 0;
		p.x = pos.x-size;
		p.z = pos.z+size;
		p.y = pos.y;
		AdjustLevel(p, eye, deep, uv1, uv2);
		vertex[u++] = D3DVERTEX2(p, n, uv1.x,uv1.y, uv2.x,uv2.y);

		p.x = pos.x-size;
		p.z = pos.z-size;
		p.y = pos.y;
		AdjustLevel(p, eye, deep, uv1, uv2);
		vertex[u++] = D3DVERTEX2(p, n, uv1.x,uv1.y, uv2.x,uv2.y);

		for ( j=0 ; j<m_line[i].len ; j++ )
		{
			p.x = pos.x+size;
			p.z = pos.z+size;
			p.y = pos.y;
			AdjustLevel(p, eye, deep, uv1, uv2);
			vertex[u++] = D3DVERTEX2(p, n, uv1.x,uv1.y, uv2.x,uv2.y);

			p.x = pos.x+size;
			p.z = pos.z-size;
			p.y = pos.y;
			AdjustLevel(p, eye, deep, uv1, uv2);
			vertex[u++] = D3DVERTEX2(p, n, uv1.x,uv1.y, uv2.x,uv2.y);

			pos.x += size*2.0f;
		}

		device->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_VERTEX2, vertex, u, NULL);
		m_engine->AddStatisticTriangle(u-2);
	}

	m_engine->SetDeepView(iDeep);
	m_engine->SetFocus(m_engine->RetFocus());
	m_engine->UpdateMatProj();  // gives depth to initial

	free(vertex);
}


// Updates the positions, relative to the ground.

bool CCloud::CreateLine(int x, int y, int len)
{
	float	offset;

	m_line[m_lineUsed].x   = x;
	m_line[m_lineUsed].y   = y;
	m_line[m_lineUsed].len = len;

	offset = m_brick*m_size/2.0f - m_size/2.0f;

	m_line[m_lineUsed].px1 = m_size* m_line[m_lineUsed].x - offset;
	m_line[m_lineUsed].px2 = m_size*(m_line[m_lineUsed].x+m_line[m_lineUsed].len) - offset;
	m_line[m_lineUsed].pz  = m_size* m_line[m_lineUsed].y - offset;

	m_lineUsed ++;

	return ( m_lineUsed < MAXCLOUDLINE );
}

// Creates all areas of cloud.

bool CCloud::Create(const char *filename,
					D3DCOLORVALUE diffuse, D3DCOLORVALUE ambient,
					float level)
{
	int			y;

	m_diffuse  = diffuse;
	m_ambient  = ambient;
	m_level    = level;
	m_time     = 0.0f;
	m_lastTest = 0.0f;
	strcpy(m_filename, filename);

	if ( m_filename[0] != 0 )
	{
		m_engine->LoadTexture(m_filename, 0);
		m_engine->LoadTexture(m_filename, 1);
	}

	if ( m_terrain == 0 )
	{
		m_terrain = (CTerrain*)m_iMan->SearchInstance(CLASS_TERRAIN);
	}

	m_wind = m_terrain->RetWind();

	m_brick = m_terrain->RetBrick()*m_terrain->RetMosaic()*DIMEXPAND;
	m_size  = m_terrain->RetSize();

	m_brick /= m_subdiv*DIMEXPAND;
	m_size  *= m_subdiv*DIMEXPAND;

	if ( m_level == 0.0f )  return true;

	m_lineUsed = 0;
	for ( y=0 ; y<m_brick ; y++ )
	{
		CreateLine(0, y, m_brick);
	}
	return true;
}

// Removes all the clouds.

void CCloud::Flush()
{
	m_level = 0.0f;
}


// Modifies the cloud level.

bool CCloud::SetLevel(float level)
{
	m_level = level;

	return Create(m_filename, m_diffuse, m_ambient,
				  m_level);
}

// Returns the current level of clouds.

float CCloud::RetLevel()
{
	return m_level;
}


// Activate management of clouds.

void CCloud::SetEnable(bool bEnable)
{
	m_bEnable = bEnable;
}

bool CCloud::RetEnable()
{
	return m_bEnable;
}

