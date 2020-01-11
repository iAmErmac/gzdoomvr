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
** gl_weapon.cpp
** Weapon sprite drawing
**
*/

#include "gl/system/gl_system.h"
#include "sbar.h"
#include "r_utility.h"
#include "v_video.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_levellocals.h"

#include "gl/system/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/scene/hw_weapon.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/models/gl_models.h"
#include "gl/renderer/gl_quaddrawer.h"
#include "gl/stereo3d/gl_stereo3d.h"
#include "gl/dynlights/gl_lightbuffer.h"

EXTERN_CVAR(Bool, r_drawplayersprites)
EXTERN_CVAR(Float, transsouls)
EXTERN_CVAR(Int, gl_fuzztype)
EXTERN_CVAR(Bool, r_deathcamera)
EXTERN_CVAR(Int, r_PlayerSprites3DMode)
EXTERN_CVAR(Float, gl_fatItemWidth)

enum PlayerSprites3DMode
{
	CROSSED,
	BACK_ONLY,
	ITEM_ONLY,
	FAT_ITEM,
};

//==========================================================================
//
// R_DrawPSprite
//
//==========================================================================

void GLSceneDrawer::DrawPSprite (player_t * player,DPSprite *psp, float sx, float sy, int OverrideShader, bool alphatexture)
{

	WeaponRect rc;
	
	if (!GetWeaponRect(psp, sx, sy, player, rc)) return;
	gl_RenderState.SetMaterial(rc.tex, CLAMP_XY_NOMIP, 0, OverrideShader, alphatexture);
	if ((rc.tex->tex->GetTranslucency() || OverrideShader != -1)  && !s3d::Stereo3DMode::getCurrentMode().RenderPlayerSpritesCrossed())
	{
		gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	}
	gl_RenderState.Apply();
	if (r_PlayerSprites3DMode != ITEM_ONLY && r_PlayerSprites3DMode != FAT_ITEM)
	{
		FQuadDrawer qd;
		qd.Set(0, rc.x1, rc.y1, 0, rc.u1, rc.v1);
		qd.Set(1, rc.x1, rc.y2, 0, rc.u1, rc.v2);
		qd.Set(2, rc.x2, rc.y1, 0, rc.u2, rc.v1);
		qd.Set(3, rc.x2, rc.y2, 0, rc.u2, rc.v2);
		qd.Render(GL_TRIANGLE_STRIP);
	}

	//TODO Cleanup code for rendering weapon models from sprites in VR mode
	if (psp->GetID() == PSP_WEAPON && s3d::Stereo3DMode::getCurrentMode().RenderPlayerSpritesCrossed())
	{
		if (r_PlayerSprites3DMode == BACK_ONLY)
			return;

		float fU1,fV1;
		float fU2,fV2;

		AWeapon * wi=player->ReadyWeapon;

		// decide which patch to use
		bool mirror;
		FTextureID lump = sprites[psp->GetSprite()].GetSpriteFrame(psp->GetFrame(), 0, 0., &mirror);
		if (!lump.isValid()) return;

		FMaterial * tex = FMaterial::ValidateTexture(lump, true, false);
		if (!tex) return;

		gl_RenderState.SetMaterial(tex, CLAMP_XY_NOMIP, 0, OverrideShader, alphatexture);

		float vw = (float)viewwidth;
		float vh = (float)viewheight;

		FState* spawn = wi->FindState(NAME_Spawn);

		lump = sprites[spawn->sprite].GetSpriteFrame(0, 0, 0., &mirror);
		if (!lump.isValid()) return;

		tex = FMaterial::ValidateTexture(lump, true, false);
		if (!tex) return;

		gl_RenderState.SetMaterial(tex, CLAMP_XY_NOMIP, 0, OverrideShader, alphatexture);

		float z1 = 0.0f;
		float z2 = (rc.y2 - rc.y1) * MIN(3, tex->GetWidth() / tex->GetHeight());

		if (!(mirror) != !(psp->Flags & PSPF_FLIP))
		{
			fU2 = tex->GetSpriteUL();
			fV1 = tex->GetSpriteVT();
			fU1 = tex->GetSpriteUR();
			fV2 = tex->GetSpriteVB();
		}
		else
		{
			fU1 = tex->GetSpriteUL();
			fV1 = tex->GetSpriteVT();
			fU2 = tex->GetSpriteUR();
			fV2 = tex->GetSpriteVB();
		}

		if (r_PlayerSprites3DMode == FAT_ITEM)
		{
			float x1 = vw / 2 + (rc.x1 - vw / 2) * gl_fatItemWidth;
			float x2 = vw / 2 + (rc.x2 - vw / 2) * gl_fatItemWidth;

			for (float x = x1; x < x2; x += 1)
			{
				FQuadDrawer qd2;
				qd2.Set(0, x, rc.y1, -z1, fU1, fV1);
				qd2.Set(1, x, rc.y2, -z1, fU1, fV2);
				qd2.Set(2, x, rc.y1, -z2, fU2, fV1);
				qd2.Set(3, x, rc.y2, -z2, fU2, fV2);
				qd2.Render(GL_TRIANGLE_STRIP);
			}
		}
		else
		{
			float crossAt;
			if (r_PlayerSprites3DMode == ITEM_ONLY)
			{
				crossAt = 0.0f;
				sy = 0.0f;
			}
			else
			{
				sy = rc.y2 - rc.y1;
				crossAt = sy * 0.25f;
			}

			float y1 = rc.y1 - crossAt;
			float y2 = rc.y2 - crossAt;

			FQuadDrawer qd2;
			qd2.Set(0, vw / 2 - crossAt, y1, -z1, fU1, fV1);
			qd2.Set(1, vw / 2 + sy / 2, y2, -z1, fU1, fV2);
			qd2.Set(2, vw / 2 - crossAt, y1, -z2, fU2, fV1);
			qd2.Set(3, vw / 2 + sy / 2, y2, -z2, fU2, fV2);
			qd2.Render(GL_TRIANGLE_STRIP);

			FQuadDrawer qd3;
			qd3.Set(0, vw / 2 + crossAt, y1, -z1, fU1, fV1);
			qd3.Set(1, vw / 2 - sy / 2, y2, -z1, fU1, fV2);
			qd3.Set(2, vw / 2 + crossAt, y1, -z2, fU2, fV1);
			qd3.Set(3, vw / 2 - sy / 2, y2, -z2, fU2, fV2);
			qd3.Render(GL_TRIANGLE_STRIP);
		}
	}

	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
}

//==========================================================================
//
//
//
//==========================================================================

void GLSceneDrawer::SetupWeaponLight()
{
	weapondynlightindex.Clear();

	AActor *camera = r_viewpoint.camera;
	AActor * playermo = players[consoleplayer].camera;
	player_t * player = playermo->player;

	// this is the same as in DrawPlayerSprites below (i.e. no weapon being drawn.)
	if (!player ||
		!r_drawplayersprites ||
		!camera->player ||
		(player->cheats & CF_CHASECAM) ||
		(r_deathcamera && camera->health <= 0))
		return;

	// Check if lighting can be used on this item.
	if (camera->RenderStyle.BlendOp == STYLEOP_Shadow || !level.HasDynamicLights || !gl_light_sprites || FixedColormap != CM_DEFAULT || gl.legacyMode)
		return;

	for (DPSprite *psp = player->psprites; psp != nullptr && psp->GetID() < PSP_TARGETCENTER; psp = psp->GetNext())
	{
		if (psp->GetState() != nullptr)
		{
			FSpriteModelFrame *smf = playermo->player->ReadyWeapon ? gl_FindModelFrame(playermo->player->ReadyWeapon->GetClass(), psp->GetState()->sprite, psp->GetState()->GetFrame(), false) : nullptr;
			if (smf)
			{
				hw_GetDynModelLight(playermo, lightdata);
				weapondynlightindex[psp] = GLRenderer->mLights->UploadLights(lightdata);
			}
		}
	}
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void GLSceneDrawer::DrawPlayerSprites(sector_t * viewsector, bool hudModelStep)
{
	bool brightflash = false;
	AActor * playermo=players[consoleplayer].camera;
	player_t * player=playermo->player;
	
	s3d::Stereo3DMode::getCurrentMode().AdjustPlayerSprites();

	AActor *camera = r_viewpoint.camera;

	// this is the same as the software renderer
	if (!player ||
		!r_drawplayersprites ||
		!camera->player ||
		(player->cheats & CF_CHASECAM) || 
		(r_deathcamera && camera->health <= 0))
		return;

	WeaponPosition weap = GetWeaponPosition(camera->player);
	WeaponLighting light = GetWeaponLighting(viewsector, r_viewpoint.Pos, FixedColormap, in_area, camera->Pos());

	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);

	// hack alert! Rather than changing everything in the underlying lighting code let's just temporarily change
	// light mode here to draw the weapon sprite.
	int oldlightmode = level.lightmode;
	if (level.lightmode == 8) level.lightmode = 2;

	for(DPSprite *psp = player->psprites; psp != nullptr && psp->GetID() < PSP_TARGETCENTER; psp = psp->GetNext())
	{
		if (!psp->GetState()) continue;
		WeaponRenderStyle rs = GetWeaponRenderStyle(psp, camera);
		if (rs.RenderStyle.BlendOp == STYLEOP_None) continue;

		gl_SetRenderStyle(rs.RenderStyle, false, false);

		PalEntry ThingColor = (camera->RenderStyle.Flags & STYLEF_ColorIsFixed) ? camera->fillcolor : 0xffffff;
		ThingColor.a = 255;

		// now draw the different layers of the weapon.
		// For stencil render styles brightmaps need to be disabled.
		gl_RenderState.EnableBrightmap(!(rs.RenderStyle.Flags & STYLEF_ColorIsFixed));

		const bool bright = isBright(psp);
		const PalEntry finalcol = bright? ThingColor : ThingColor.Modulate(viewsector->SpecialColors[sector_t::sprites]);
		gl_RenderState.SetObjectColor(finalcol);

		auto ll = light;
		if (bright) ll.SetBright();

		// set the lighting parameters
		if (rs.RenderStyle.BlendOp == STYLEOP_Shadow)
		{
			gl_RenderState.SetColor(0.2f, 0.2f, 0.2f, 0.33f, ll.cm.Desaturation);
		}
		else
		{
			if (level.HasDynamicLights && FixedColormap == CM_DEFAULT && gl_light_sprites)
			{
				FSpriteModelFrame *smf = playermo->player->ReadyWeapon ? gl_FindModelFrame(playermo->player->ReadyWeapon->GetClass(), psp->GetState()->sprite, psp->GetState()->GetFrame(), false) : nullptr;
				if (!smf || gl.legacyMode)	// For models with per-pixel lighting this was done in a previous pass.
				{
					float out[3];
					gl_drawinfo->GetDynSpriteLight(playermo, nullptr, out);
					gl_RenderState.SetDynLight(out[0], out[1], out[2]);
				}
			}
			SetColor(ll.lightlevel, 0, ll.cm, rs.alpha, true);
		}

		FVector2 spos = BobWeapon(weap, psp);

		// [BB] In the HUD model step we just render the model and break out. 
		if (hudModelStep)
		{
			gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
			gl_RenderHUDModel(psp, spos.X, spos.Y, weapondynlightindex[psp]);
		}
		else
		{
			DrawPSprite(player, psp, spos.X, spos.Y, rs.OverrideShader, !!(rs.RenderStyle.Flags & STYLEF_RedIsAlpha));
		}
	}

	s3d::Stereo3DMode::getCurrentMode().DrawControllerModels();

	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.SetDynLight(0, 0, 0);
	gl_RenderState.EnableBrightmap(false);
	level.lightmode = oldlightmode;
	if (!hudModelStep)
	{
		s3d::Stereo3DMode::getCurrentMode().UnAdjustPlayerSprites();
	}
	
	
}


//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void GLSceneDrawer::DrawTargeterSprites()
{
	AActor * playermo=players[consoleplayer].camera;
	player_t * player=playermo->player;
	
	if(!player || playermo->renderflags&RF_INVISIBLE || !r_drawplayersprites ||
		GLRenderer->mViewActor!=playermo) return;

	gl_RenderState.EnableBrightmap(false);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl_RenderState.AlphaFunc(GL_GEQUAL,gl_mask_sprite_threshold);
	gl_RenderState.BlendEquation(GL_FUNC_ADD);
	gl_RenderState.ResetColor();
	gl_RenderState.SetTextureMode(TM_MODULATE);

	// The Targeter's sprites are always drawn normally.
	for (DPSprite *psp = player->FindPSprite(PSP_TARGETCENTER); psp != nullptr; psp = psp->GetNext())
	{
		if (psp->GetState() != nullptr) DrawPSprite(player, psp, psp->x, psp->y, 0, false);
	}
}
