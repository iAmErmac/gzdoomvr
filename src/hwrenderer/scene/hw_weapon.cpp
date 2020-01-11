// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** hw_weapon.cpp
** Weapon sprite utilities
**
*/

#include "sbar.h"
#include "r_utility.h"
#include "v_video.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "hw_weapon.h"
#include "hw_fakeflat.h"
#include "hwrenderer/textures/hw_material.h"
#include "hwrenderer/utility/hw_lighting.h"
#include "hwrenderer/utility/hw_cvars.h"

EXTERN_CVAR(Float, transsouls)
EXTERN_CVAR(Int, gl_fuzztype)


//==========================================================================
//
//
//
//==========================================================================

bool isBright(DPSprite *psp)
{
	if (psp != nullptr && psp->GetState() != nullptr)
	{
		bool disablefullbright = false;
		FTextureID lump = sprites[psp->GetSprite()].GetSpriteFrame(psp->GetFrame(), 0, 0., nullptr);
		if (lump.isValid())
		{
			FTexture * tex = TexMan(lump);
			if (tex) disablefullbright = tex->bDisableFullbright;
		}
		return psp->GetState()->GetFullbright() && !disablefullbright;
	}
	return false;
}

//==========================================================================
//
// Weapon position
//
//==========================================================================

WeaponPosition GetWeaponPosition(player_t *player)
{
	WeaponPosition w;
	P_BobWeapon(player, &w.bobx, &w.boby, r_viewpoint.TicFrac);

	// Interpolate the main weapon layer once so as to be able to add it to other layers.
	if ((w.weapon = player->FindPSprite(PSP_WEAPON)) != nullptr)
	{
		if (w.weapon->firstTic)
		{
			w.wx = (float)w.weapon->x;
			w.wy = (float)w.weapon->y;
		}
		else
		{
			w.wx = (float)(w.weapon->oldx + (w.weapon->x - w.weapon->oldx) * r_viewpoint.TicFrac);
			w.wy = (float)(w.weapon->oldy + (w.weapon->y - w.weapon->oldy) * r_viewpoint.TicFrac);
		}
	}
	else
	{
		w.wx = 0;
		w.wy = 0;
	}
	return w;
}

//==========================================================================
//
// Bobbing
//
//==========================================================================

FVector2 BobWeapon(WeaponPosition &weap, DPSprite *psp)
{
	if (psp->firstTic)
	{ // Can't interpolate the first tic.
		psp->firstTic = false;
		psp->oldx = psp->x;
		psp->oldy = psp->y;
	}

	float sx = float(psp->oldx + (psp->x - psp->oldx) * r_viewpoint.TicFrac);
	float sy = float(psp->oldy + (psp->y - psp->oldy) * r_viewpoint.TicFrac);

	if (psp->Flags & PSPF_ADDBOB)
	{
		sx += (psp->Flags & PSPF_MIRROR) ? -weap.bobx : weap.bobx;
		sy += weap.boby;
	}

	if (psp->Flags & PSPF_ADDWEAPON && psp->GetID() != PSP_WEAPON)
	{
		sx += weap.wx;
		sy += weap.wy;
	}
	return { sx, sy };
}

//==========================================================================
//
// Lighting
//
//==========================================================================

WeaponLighting GetWeaponLighting(sector_t *viewsector, const DVector3 &pos, int FixedColormap, area_t in_area, const DVector3 &playerpos )
{
	WeaponLighting l;

	if (FixedColormap)
	{
		l.lightlevel = 255;
		l.cm.Clear();
		l.isbelow = false;
	}
	else
	{
		sector_t fs;
		auto fakesec = hw_FakeFlat(viewsector, &fs, in_area, false);

		// calculate light level for weapon sprites
		l.lightlevel = hw_ClampLight(fakesec->lightlevel);

		// calculate colormap for weapon sprites
		if (viewsector->e->XFloor.ffloors.Size() && !(level.flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING))
		{
			TArray<lightlist_t> & lightlist = viewsector->e->XFloor.lightlist;
			for (unsigned i = 0; i<lightlist.Size(); i++)
			{
				double lightbottom;

				if (i<lightlist.Size() - 1)
				{
					lightbottom = lightlist[i + 1].plane.ZatPoint(r_viewpoint.Pos);
				}
				else
				{
					lightbottom = viewsector->floorplane.ZatPoint(r_viewpoint.Pos);
				}

				if (lightbottom<r_viewpoint.Pos.Z)
				{
					l.cm = lightlist[i].extra_colormap;
					l.lightlevel = hw_ClampLight(*lightlist[i].p_lightlevel);
					break;
				}
			}
		}
		else
		{
			l.cm = fakesec->Colormap;
			if (level.flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING) l.cm.ClearColor();
		}

		l.lightlevel = hw_CalcLightLevel(l.lightlevel, getExtraLight(), true);

		if (level.lightmode == 8 || l.lightlevel < 92)
		{
			// Korshun: the way based on max possible light level for sector like in software renderer.
			double min_L = 36.0 / 31.0 - ((l.lightlevel / 255.0) * (63.0 / 31.0)); // Lightlevel in range 0-63
			if (min_L < 0)
				min_L = 0;
			else if (min_L > 1.0)
				min_L = 1.0;

			l.lightlevel = int((1.0 - min_L) * 255);
		}
		else
		{
			l.lightlevel = (2 * l.lightlevel + 255) / 3;
		}
		l.lightlevel = viewsector->CheckSpriteGlow(l.lightlevel, playerpos);
		l.isbelow = fakesec != viewsector && in_area == area_below;
	}

	// Korshun: fullbright fog in opengl, render weapon sprites fullbright (but don't cancel out the light color!)
	if (level.brightfog && ((level.flags&LEVEL_HASFADETABLE) || l.cm.FadeColor != 0))
	{
		l.lightlevel = 255;
	}
	return l;
}

//==========================================================================
//
// Render Style
//
//==========================================================================

WeaponRenderStyle GetWeaponRenderStyle(DPSprite *psp, AActor *playermo)
{
	WeaponRenderStyle r;
	auto rs = psp->GetRenderStyle(playermo->RenderStyle, playermo->Alpha);

	visstyle_t vis;
	float trans = 0.f;

	vis.RenderStyle = STYLE_Count;
	vis.Alpha = rs.second;
	vis.Invert = false;
	playermo->AlterWeaponSprite(&vis);

	if (!(psp->Flags & PSPF_FORCEALPHA)) trans = vis.Alpha;

	if (vis.RenderStyle != STYLE_Count && !(psp->Flags & PSPF_FORCESTYLE))
	{
		r.RenderStyle = vis.RenderStyle;
	}
	else
	{
		r.RenderStyle = rs.first;
	}
	if (r.RenderStyle.BlendOp == STYLEOP_None) return r;

	if (vis.Invert)
	{
		// this only happens for Strife's inverted weapon sprite
		r.RenderStyle.Flags |= STYLEF_InvertSource;
	}

	// Set the render parameters

	r.OverrideShader = -1;
	if (r.RenderStyle.BlendOp == STYLEOP_Fuzz)
	{
		if (gl_fuzztype != 0)
		{
			// Todo: implement shader selection here
			r.RenderStyle = LegacyRenderStyles[STYLE_Translucent];
			r.OverrideShader = SHADER_NoTexture + gl_fuzztype;
			r.alpha = 0.99f;	// trans may not be 1 here
		}
		else
		{
			r.RenderStyle.BlendOp = STYLEOP_Shadow;
		}
	}


	if (r.RenderStyle.Flags & STYLEF_TransSoulsAlpha)
	{
		r.alpha	= transsouls;
	}
	else if (r.RenderStyle.Flags & STYLEF_Alpha1)
	{
		r.alpha = 1.f;
	}
	else if (trans == 0.f)
	{
		r.alpha = vis.Alpha;
	}
	return r;
}

//==========================================================================
//
// Coordinates
//
//==========================================================================

bool GetWeaponRect(DPSprite *psp, float sx, float sy, player_t *player, WeaponRect &rc)
{
	float			tx;
	float			x1, x2;
	float			scale;
	float			scalex;
	float			ftexturemid;

	// decide which patch to use
	bool mirror;
	FTextureID lump = sprites[psp->GetSprite()].GetSpriteFrame(psp->GetFrame(), 0, 0., &mirror);
	if (!lump.isValid()) return false;

	FMaterial * tex = FMaterial::ValidateTexture(lump, true, false);
	if (!tex) return false;

	float vw = (float)viewwidth;
	float vh = (float)viewheight;

	FloatRect r;
	tex->GetSpriteRect(&r);

	// calculate edges of the shape
	scalex = (320.0f / (240.0f * r_viewwindow.WidescreenRatio)) * vw / 320;

	tx = (psp->Flags & PSPF_MIRROR) ? ((160 - r.width) - (sx + r.left)) : (sx - (160 - r.left));
	x1 = tx * scalex + vw / 2;
	if (x1 > vw)	return false; // off the right side
	rc.x1 = x1 + viewwindowx;


	tx += r.width;
	x2 = tx * scalex + vw / 2;
	if (x2 < 0) return false; // off the left side
	rc.x2 = x2 + viewwindowx;

	// killough 12/98: fix psprite positioning problem
	ftexturemid = 100.f - sy - r.top;

	AWeapon * wi = player->ReadyWeapon;
	if (wi && wi->YAdjust != 0)
	{
		float fYAd = wi->YAdjust;
		if (screenblocks >= 11)
		{
			ftexturemid -= fYAd;
		}
		else
		{
			ftexturemid -= float(StatusBar->GetDisplacement()) * fYAd;
		}
	}

	scale = (SCREENHEIGHT*vw) / (SCREENWIDTH * 200.0f);
	rc.y1 = viewwindowy + vh / 2 - (ftexturemid * scale);
	rc.y2 = rc.y1 + (r.height * scale) + 1;


	if (!(mirror) != !(psp->Flags & (PSPF_FLIP)))
	{
		rc.u2 = tex->GetSpriteUL();
		rc.v1 = tex->GetSpriteVT();
		rc.u1 = tex->GetSpriteUR();
		rc.v2 = tex->GetSpriteVB();
	}
	else
	{
		rc.u1 = tex->GetSpriteUL();
		rc.v1 = tex->GetSpriteVT();
		rc.u2 = tex->GetSpriteUR();
		rc.v2 = tex->GetSpriteVB();
	}
	rc.tex = tex;
	return true;
}

