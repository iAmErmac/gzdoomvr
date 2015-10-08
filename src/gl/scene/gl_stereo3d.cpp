#include "v_video.h"
#include "c_dispatch.h"
#include "gl/system/gl_system.h"
#include "gl/system/gl_cvars.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/scene/gl_stereo3d.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_colormask.h"
#include "gl/scene/gl_hudtexture.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_convert.h"
#include "doomstat.h"
#include "d_player.h"
#include "r_utility.h" // viewpitch
#include "g_game.h"
#include "c_console.h"
#include "sbar.h"
#include "am_map.h"
#include "gl/scene/gl_localhudrenderer.h"
#include <cmath>

#include "Extras/OVR_Math.h"

extern void P_CalcHeight (player_t *player);

extern DBaseStatusBar *StatusBar; // To access crosshair drawing


EXTERN_CVAR(Bool, gl_draw_sync)
EXTERN_CVAR(Float, vr_screendist)
//
CVAR(Int, vr_mode, 0, CVAR_GLOBALCONFIG)
CVAR(Bool, vr_swap, false, CVAR_GLOBALCONFIG)
// intraocular distance in meters
CVAR(Float, vr_ipd, 0.062f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // METERS
CVAR(Float, vr_rift_fov, 117.4f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // DEGREES
CVAR(Float, vr_view_yoffset, 4.0, 0) // MAP UNITS - raises your head to be closer to soldier height
// Especially Oculus Rift VR geometry depends on exact mapping between doom map units and real world.
// Supposed to be 32 units per meter, according to http://doom.wikia.com/wiki/Map_unit
// But ceilings and floors look too close at that scale.
CVAR(Float, vr_player_height_meters, 1.75f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // Used for stereo 3D
CVAR(Float, vr_rift_aspect, 640.0/800.0, CVAR_GLOBALCONFIG) // Used for stereo 3D
CVAR(Float, vr_weapon_height, 0.0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // Used for oculus rift
CVAR(Float, vr_weapondist, 0.6, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // METERS
CVAR(Int, vr_device, 1, CVAR_GLOBALCONFIG) // 1 for DK1, 2 for DK2 (Default to DK2)
CVAR(Float, vr_sprite_scale, 0.40, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // weapon size
CVAR(Float, vr_hud_scale, 0.6, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // menu/message size
CVAR(Bool, vr_lowpersist, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// Command to set "standard" rift settings
EXTERN_CVAR(Int, con_scaletext)
EXTERN_CVAR(Bool, hud_scale)
EXTERN_CVAR(Int, hud_althudscale)
EXTERN_CVAR(Bool, crosshairscale)
EXTERN_CVAR(Bool, freelook)
EXTERN_CVAR(Float, movebob)
EXTERN_CVAR(Float, turbo)
EXTERN_CVAR(Int, screenblocks)
EXTERN_CVAR(Int, m_use_mouse)
EXTERN_CVAR(Int, crosshair)
CCMD(oculardium_optimosa)
{
	// scale up all HUD chrome
	con_scaletext = 1; // console and messages
	hud_scale = 1;
	hud_althudscale = 1;
	crosshairscale = 1;
	movebob = 0.05; // No bobbing
	turbo = 65; // Slower walking
	vr_mode = 8; // Rift mode
	// Use minimal or no HUD
	// if (screenblocks <= 10)
	// 	screenblocks = 11;
	vr_lowpersist = true;
	m_use_mouse = 0; // no mouse in menus
	// freelook = false; // no up/down look with mouse // too intrusive?
	crosshair = 1; // show crosshair
	vr_view_yoffset = 4;
	// AddCommandString("vid_setmode 1920 1080 32\n"); // causes crash
}


// Create aliases for comfort mode controls
// Quick turn commands for VR comfort mode
static void set_turn_aliases() {
	static bool b_turn_aliases_set = false;
	if (b_turn_aliases_set) return;
	AddCommandString("alias turn45left \"alias turn45_step \\\"wait 5;-left;turnspeeds 640 1280 320 320;alias turn45_step\\\";turn45_step;wait;turnspeeds 2048 2048 2048 2048;+left\"\n");
	AddCommandString("alias turn45right \"alias turn45_step \\\"wait 5;-right;turnspeeds 640 1280 320 320;alias turn45_step\\\";turn45_step;wait;turnspeeds 2048 2048 2048 2048;+right\"\n");
	b_turn_aliases_set = true;
}
// These commands snap45left snap45right are linked in the "controller configuration" menu
CCMD(snap45left)
{
	set_turn_aliases();
	AddCommandString("turn45left\n");
}
CCMD(snap45right)
{
	set_turn_aliases();
	AddCommandString("turn45right");
}

void Stereo3D::resetPosition() {
	if (sharedRiftHmd != NULL)
		sharedRiftHmd->recenter_pose();
}

CCMD(vr_reset_position)
{
	Stereo3DMode.resetPosition();
}

// Render HUD items twice, once for each eye
// TODO - these flags don't work
static bool doBufferHud = true;


// Global shared Stereo3DMode object
Stereo3D Stereo3DMode;

static fixed_t savedPlayerViewHeight = 0;

// Delegated screen functions from v_video.h
int getStereoScreenWidth() {
	return Stereo3DMode.getScreenWidth();
}
int getStereoScreenHeight() {
	return Stereo3DMode.getScreenHeight();
}
void stereoScreenUpdate() {
	Stereo3DMode.updateScreen();
}

// length scale, to convert from meters to doom units
static float calc_mapunits_per_meter(player_t * player) {
	float vh = 41.0;
	if (player != NULL)
		vh = FIXED2FLOAT(player->mo->ViewHeight);
	return vh/(0.95 * vr_player_height_meters);
}

// Stack-scope class to temporarily adjust view position, based on positional tracking
struct ViewPositionShifter {
	ViewPositionShifter(player_t * player, FGLRenderer& renderer_param)
		: mapunits_per_meter(41.0)
		, renderer(&renderer_param)
		, saved_viewx(viewx)
		, saved_viewy(viewy)
		, saved_viewz(viewz)
	{
		mapunits_per_meter = calc_mapunits_per_meter(player);
	}

	// restore camera position after object falls out of scope
	virtual ~ViewPositionShifter() {
		setPositionFixed(saved_viewx, saved_viewy, saved_viewz);
	}

protected:
	// In player camera coordinates
	void incrementPositionFloat(float dx, float dy, float dz) {
		float xf = FIXED2FLOAT(viewx);
		float yf = FIXED2FLOAT(viewy);
		float zf = FIXED2FLOAT(viewz);

		// TODO - conversion from player to doom coordinates does not take into account roll.

		// view angle, for conversion from body to world
		float yaw = DEG2RAD( ANGLE_TO_FLOAT(viewangle) );
		float cy = cos(yaw);
		float sy = sin(yaw);

		zf += dz * mapunits_per_meter; // doom pixel aspect correction 1.20
		xf += ( sy * dx + cy * dy) * mapunits_per_meter * 1.20;
		yf += (-cy * dx + sy * dy) * mapunits_per_meter * 1.20;

		setPositionFixed( FLOAT2FIXED(xf), FLOAT2FIXED(yf), FLOAT2FIXED(zf) );
	}

	// In doom world coordinates
	void setPositionFixed(fixed_t x, fixed_t y, fixed_t z) {
		viewx = x;
		viewy = y;
		viewz = z;
		renderer->SetCameraPos(viewx, viewy, viewz, viewangle);
		renderer->SetViewMatrix(false, false);
	}

private:
	fixed_t saved_viewx;
	fixed_t saved_viewy;
	fixed_t saved_viewz;
	FGLRenderer * renderer;
	float mapunits_per_meter;
};

enum EyeView {
	EYE_VIEW_LEFT,
	EYE_VIEW_RIGHT
};


// Stack-scope class to temporarily shift the camera position for stereoscopic rendering.
struct EyeViewShifter : public ViewPositionShifter
{
	// construct a new EyeViewShifter, to temporarily shift camera viewpoint
	EyeViewShifter(EyeView eyeView, player_t * player, FGLRenderer& renderer_param)
		: ViewPositionShifter(player, renderer_param)
	{
		float eyeShift = vr_ipd / 2.0;
		if (eyeView == EYE_VIEW_LEFT) {
			eyeShift = -eyeShift;
		}
		// Account for roll angle
		float roll = renderer_param.mAngles.Roll * 3.14159/180.0;
		float cr = cos(roll);
		float sr = sin(roll);
		// Printf("%.3f\n", roll);
		/* */
		incrementPositionFloat(
			cr * eyeShift, // left-right
			0, 
			-sr * eyeShift  // up-down; sign adjusted empirically
			);
		/*	*/
	}
};


// Stack-scope class to temporarily shift the camera position for stereoscopic rendering.
struct PositionTrackingShifter : public ViewPositionShifter
{
	// construct a new EyeViewShifter, to temporarily shift camera viewpoint
	PositionTrackingShifter(RiftHmd * tracker, player_t * player, FGLRenderer& renderer_param)
		: ViewPositionShifter(player, renderer_param)
	{
		if (tracker == NULL) return;
		// TODO - calibrate to center...
		// Doom uses Z-UP convention, Rift uses Y-UP convention
		// Printf("%.3f\n", tracker->getPositionX());
		ovrPosef pose = tracker->getCurrentEyePose();

		// Convert from Rift camera coordinates to game coordinates
		// float gameYaw = renderer_param.mAngles.Yaw;
		OVR::Quatf hmdRot(pose.Orientation);
		float hmdYaw, hmdPitch, hmdRoll;
		hmdRot.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&hmdYaw, &hmdPitch, &hmdRoll);
		OVR::Quatf yawCorrection(OVR::Vector3f(0, 1, 0), -hmdYaw); // 
		// OVR::Vector3f trans0(pose.Position);
		OVR::Vector3f trans2 = yawCorrection.Rotate(pose.Position);
		/* */
		incrementPositionFloat(
			 trans2.x, // pose.Position.x, // LEFT_RIGHT
			-trans2.z, // pose.Position.z, // FORWARD_BACK
			 trans2.y // pose.Position.y  // UP_DOWN
		); 
		/* */
	}
};


Stereo3D::Stereo3D() 
	: mode(MONO)
{}

static HudTexture* checkHudTexture(HudTexture* hudTexture, float screenScale) {
		if (hudTexture)
			hudTexture->setScreenScale(screenScale); // BEFORE checkScreenSize
		if ( (hudTexture == NULL) || (! hudTexture->checkScreenSize(SCREENWIDTH, SCREENHEIGHT) ) ) {
			if (hudTexture)
				delete(hudTexture);
			hudTexture = new HudTexture(SCREENWIDTH, SCREENHEIGHT, screenScale);
			HudTexture::crosshairTexture = new HudTexture(SCREENWIDTH, SCREENHEIGHT, screenScale);
			hudTexture->bindToFrameBuffer();
			glClearColor(0, 0, 0, 0);
			glClear(GL_COLOR_BUFFER_BIT);
			hudTexture->unbind();
		}
		return hudTexture;
}

static void bindAndClearHudTexture(Stereo3D& stereo3d) {
	if (doBufferHud) {
		stereo3d.bindHudTexture(true);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		// glViewport(0, 0, SCREENWIDTH, SCREENHEIGHT);
	}
}


PitchRollYaw Stereo3D::getHeadOrientation(FGLRenderer& renderer) {
	PitchRollYaw result;

	result.pitch = renderer.mAngles.Pitch;
	result.roll = renderer.mAngles.Roll;
	result.yaw = renderer.mAngles.Yaw;

	if (mode == OCULUS_RIFT) {
		ovrPosef pose = sharedRiftHmd->getCurrentEyePose();
		OVR::Quatf hmdRot(pose.Orientation);
		float yaw, pitch, roll;
		hmdRot.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yaw, &pitch, &roll);
		result.yaw = yaw;
		result.pitch = pitch;
		result.roll = -roll;
	}

	return result;
}

void Stereo3D::render(FGLRenderer& renderer, GL_IRECT * bounds, float fov0, float ratio0, float fovratio0, bool toscreen, sector_t * viewsector, player_t * player) 
{
	if (doBufferHud)
		LocalHudRenderer::unbind();

	// Reset pitch and roll when leaving Rift mode
	if ( (mode == OCULUS_RIFT) && ((int)mode != vr_mode) ) {
		renderer.mAngles.Roll = 0;
		renderer.mAngles.Pitch = 0;
	}
	setMode(vr_mode);

	GLboolean supportsStereo = false;
	GLboolean supportsBuffered = false;
	// Task: manually calibrate oculusFov by slowly yawing view. 
	// If subjects approach center of view too fast, oculusFov is too small.
	// If subjects approach center of view too slowly, oculusFov is too large.
	// If subjects approach correctly , oculusFov is just right.
	// 90 is too large, 80 is too small.
	// float oculusFov = 85 * fovratio; // Hard code probably wider fov for oculus // use vr_rift_fov

	const bool doAdjustPlayerViewHeight = true; // disable/enable for testing
	if (doAdjustPlayerViewHeight) {
		if (mode == OCULUS_RIFT) {
		// if (false) {
			renderer.mCurrentFoV = vr_rift_fov; // needed for Frustum angle calculation
			// Adjust player eye height, but only in oculus rift mode...
			if (player != NULL) { // null check to avoid aliens crash
				if (savedPlayerViewHeight == 0) {
					savedPlayerViewHeight = player->mo->ViewHeight;
				}
				fixed_t testHeight = savedPlayerViewHeight + FLOAT2FIXED(vr_view_yoffset);
				if (player->mo->ViewHeight != testHeight) {
					player->mo->ViewHeight = testHeight;
					P_CalcHeight(player);
				}
			}
		} else {
			// Revert player eye height when leaving Rift mode
			if ( (savedPlayerViewHeight != 0) && (player->mo->ViewHeight != savedPlayerViewHeight) ) {
				player->mo->ViewHeight = savedPlayerViewHeight;
				savedPlayerViewHeight = 0;
				P_CalcHeight(player);
			}
		}
	}

	angle_t a1 = renderer.FrustumAngle();

	switch(mode) 
	{

	case MONO:
		{
			doBufferHud = false;
			setViewportFull(renderer, bounds);
			setMonoView(renderer, fov0, ratio0, fovratio0, player);
			renderer.RenderOneEye(a1, toscreen, true);
			renderer.EndDrawScene(viewsector);
			break;
		}

	case GREEN_MAGENTA:
		{
			doBufferHud = false;
			setViewportFull(renderer, bounds);
			{ // Local scope for color mask
				// Left eye green
				LocalScopeGLColorMask colorMask(0,1,0,1); // green
				setLeftEyeView(renderer, fov0, ratio0, fovratio0, player);
				{
					EyeViewShifter vs(EYE_VIEW_LEFT, player, renderer);
					renderer.RenderOneEye(a1, toscreen, false);
				}

				// Right eye magenta
				colorMask.setColorMask(1,0,1,1); // magenta
				setRightEyeView(renderer, fov0, ratio0, fovratio0, player);
				{
					EyeViewShifter vs(EYE_VIEW_RIGHT, player, renderer);
					renderer.RenderOneEye(a1, toscreen, true);
				}
			} // close scope to auto-revert glColorMask
			renderer.EndDrawScene(viewsector);
			break;
		}

	case RED_CYAN:
		{
			doBufferHud = false;
			setViewportFull(renderer, bounds);
			{ // Local scope for color mask
				// Left eye red
				LocalScopeGLColorMask colorMask(1,0,0,1); // red
				setLeftEyeView(renderer, fov0, ratio0, fovratio0, player);
				{
					EyeViewShifter vs(EYE_VIEW_LEFT, player, renderer);
					renderer.RenderOneEye(a1, toscreen, false);
				}

				// Right eye cyan
				colorMask.setColorMask(0,1,1,1); // cyan
				setRightEyeView(renderer, fov0, ratio0, fovratio0, player);
				{
					EyeViewShifter vs(EYE_VIEW_RIGHT, player, renderer);
					renderer.RenderOneEye(a1, toscreen, true);
				}
			} // close scope to auto-revert glColorMask
			renderer.EndDrawScene(viewsector);
			break;
		}

	case SIDE_BY_SIDE:
		{
			doBufferHud = true;
			HudTexture::hudTexture = checkHudTexture(HudTexture::hudTexture, 0.5);

			// FIRST PASS - 3D
			// Temporarily modify global variables, so HUD could draw correctly
			// each view is half width
			int oldViewwidth = viewwidth;

			int one_eye_viewport_width = oldViewwidth / 2;

			viewwidth = one_eye_viewport_width;
			// left
			setViewportLeft(renderer, bounds);
			setLeftEyeView(renderer, fov0, ratio0/2, fovratio0, player); // TODO is that fovratio?
			{
				EyeViewShifter vs(EYE_VIEW_LEFT, player, renderer);
				renderer.RenderOneEye(a1, false, false); // False, to not swap yet
			}
			// right
			// right view is offset to right
			int oldViewwindowx = viewwindowx;
			viewwindowx += one_eye_viewport_width;
			setViewportRight(renderer, bounds);
			setRightEyeView(renderer, fov0, ratio0/2, fovratio0, player);
			{
				EyeViewShifter vs(EYE_VIEW_RIGHT, player, renderer);
				renderer.RenderOneEye(a1, toscreen, true);
			}

			//
			// SECOND PASS weapon sprite
			// Weapon sprite to bottom, at expense of edges
			viewwidth = 2 * one_eye_viewport_width;
			viewwindowx = one_eye_viewport_width/2;
			//
			renderer.EndDrawScene(viewsector); // right view
			viewwindowx -= one_eye_viewport_width;
			renderer.EndDrawScene(viewsector); // left view

			blitHudTextureToScreen(2.0);

			//
			// restore global state
			viewwidth = oldViewwidth;
			viewwindowx = oldViewwindowx;

			bindAndClearHudTexture(*this);

			break;
		}

	case SIDE_BY_SIDE_SQUISHED:
		{
			doBufferHud = true;
			HudTexture::hudTexture = checkHudTexture(HudTexture::hudTexture, 0.5);

			// FIRST PASS - 3D
			// Temporarily modify global variables, so HUD could draw correctly
			// each view is half width
			int oldViewwidth = viewwidth;

			int one_eye_viewport_width = oldViewwidth / 2;

			viewwidth = one_eye_viewport_width;
			// left
			setViewportLeft(renderer, bounds);
			setLeftEyeView(renderer, fov0, ratio0, fovratio0*2, player);
			{
				EyeViewShifter vs(EYE_VIEW_LEFT, player, renderer);
				renderer.RenderOneEye(a1, toscreen, false);
			}
			// right
			// right view is offset to right
			int oldViewwindowx = viewwindowx;
			viewwindowx += one_eye_viewport_width;
			setViewportRight(renderer, bounds);
			setRightEyeView(renderer, fov0, ratio0, fovratio0*2, player);
			{
				EyeViewShifter vs(EYE_VIEW_RIGHT, player, renderer);
				renderer.RenderOneEye(a1, false, true);
			}
			//

			// SECOND PASS weapon sprite
			// viewwidth = oldViewwidth/2; // TODO - narrow aspect of weapon...
			// Ensure weapon is at bottom of screen
			viewwidth = 2 * one_eye_viewport_width;
			viewwindowx = one_eye_viewport_width/2;

			// TODO - encapsulate weapon shift for other modes
			int weaponShift = int( -vr_ipd * 0.25 * one_eye_viewport_width / (vr_weapondist * 2.0*tan(0.5*fov0)) );
			viewwindowx += weaponShift;

			renderer.EndDrawScene(viewsector); // right view
			viewwindowx -= one_eye_viewport_width;
			viewwindowx -= 2*weaponShift;
			renderer.EndDrawScene(viewsector); // left view

			blitHudTextureToScreen(2.0);

			//
			// restore global state
			viewwidth = oldViewwidth;
			viewwindowx = oldViewwindowx;

			bindAndClearHudTexture(*this);

			break;
		}

	case OCULUS_RIFT:
		{
			doBufferHud = true;
			sharedRiftHmd->init_graphics();

			{
				// Activate positional tracking
				// PositionTrackingShifter positionTracker(sharedRiftHmd, player, renderer);

				setViewDirection(renderer);

				HudTexture::hudTexture = checkHudTexture(HudTexture::hudTexture, 1.0);

				sharedRiftHmd->bindToSceneFrameBufferAndUpdate();
				glClearColor(0, 0, 1, 0);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				// FIRST PASS - 3D
				// Temporarily modify global variables, so HUD could draw correctly
				int oldScreenBlocks = screenblocks;
				screenblocks = 12; // full screen
				//
				// left eye view - 3D scene pass
				{
					sharedRiftHmd->setSceneEyeView(ovrEye_Left, 10, 10000); // Left eye
					PositionTrackingShifter positionTracker(sharedRiftHmd, player, renderer);
					glEnable(GL_DEPTH_TEST);
					renderer.RenderOneEye(a1, false, false);
				}
				ovrPosef leftEyePose = sharedRiftHmd->getCurrentEyePose();

				// right eye view - 3D scene pass
				{
					sharedRiftHmd->setSceneEyeView(ovrEye_Right, 10, 10000); // Right eye
					PositionTrackingShifter positionTracker(sharedRiftHmd, player, renderer);
					renderer.RenderOneEye(a1, false, true);
				}
				ovrPosef rightEyePose = sharedRiftHmd->getCurrentEyePose();

				//// HUD Pass ////
				// screenblocks = oldScreenBlocks;
				HudTexture::hudTexture->bindRenderTexture();
				glEnable(GL_TEXTURE_2D);
				glDisable(GL_DEPTH_TEST);
				glDisable(GL_BLEND);
				glAlphaFunc(GL_GREATER, 0.2); // 0.2 -> 0.3 causes console background to show
				// TODO suppress crosshair during hud pass?
				// left eye view - hud pass
				{
					sharedRiftHmd->setSceneEyeView(ovrEye_Left, 10, 10000); // Left eye
					PositionTrackingShifter positionTracker(sharedRiftHmd, player, renderer);
					sharedRiftHmd->paintHudQuad(vr_hud_scale, -25);
				}
				// right eye view - hud pass
				{
					sharedRiftHmd->setSceneEyeView(ovrEye_Right, 10, 10000); // Right eye
					PositionTrackingShifter positionTracker(sharedRiftHmd, player, renderer);
					sharedRiftHmd->paintHudQuad(vr_hud_scale, -25);
				}

				//// Crosshair Pass ////
				// screenblocks = 12; // full screen
				HudTexture::crosshairTexture->bindRenderTexture();
				glEnable(GL_BLEND);
				// left eye view - crosshair pass
				{
					sharedRiftHmd->setSceneEyeView(ovrEye_Left, 10, 10000); // Left eye
					PositionTrackingShifter positionTracker(sharedRiftHmd, player, renderer);
					sharedRiftHmd->paintCrosshairQuad(leftEyePose, rightEyePose);
				}
				// right eye view - crosshair pass
				{
					sharedRiftHmd->setSceneEyeView(ovrEye_Right, 10, 10000); // Right eye
					PositionTrackingShifter positionTracker(sharedRiftHmd, player, renderer);
					sharedRiftHmd->paintCrosshairQuad(rightEyePose, leftEyePose);
				}

				/* */
				//// Weapon Pass ////
				// Temporarily adjust view window parameters, to fool weapon into drawing full size,
				// even when "screenblocks" is a smaller number
				int oldViewWindowX = viewwindowx;
				int oldViewWindowY = viewwindowy;
				int oldViewWidth = viewwidth;
				int oldViewHeight = viewheight;
				viewwindowx = 0;
				viewwindowy = 0;
				viewwidth = SCREENWIDTH;
				viewheight = SCREENHEIGHT;
				bindAndClearHudTexture(*this);
				renderer.EndDrawScene(viewsector); // TODO paint weapon
				HudTexture::hudTexture->unbind();
				sharedRiftHmd->bindToSceneFrameBuffer();
				/* */
				glEnable(GL_TEXTURE_2D);
				HudTexture::hudTexture->bindRenderTexture();
				glDisable(GL_DEPTH_TEST);
				glDisable(GL_BLEND); // for distinct transparency threshold
				// left eye view - weapon pass
				{
					sharedRiftHmd->setSceneEyeView(ovrEye_Left, 10, 10000); // Left eye
					PositionTrackingShifter positionTracker(sharedRiftHmd, player, renderer);
					sharedRiftHmd->paintWeaponQuad(leftEyePose, rightEyePose, vr_weapondist);
				}
				// right eye view - weapon pass
				{
					sharedRiftHmd->setSceneEyeView(ovrEye_Right, 10, 10000); // Right eye
					PositionTrackingShifter positionTracker(sharedRiftHmd, player, renderer);
					sharedRiftHmd->paintWeaponQuad(rightEyePose, leftEyePose, vr_weapondist);
				}
				glEnable(GL_BLEND);
				glBindTexture(GL_TEXTURE_2D, 0);
				// Restore previous values
				viewwindowx = oldViewWindowX;
				viewwindowy = oldViewWindowY;
				viewwidth = oldViewWidth;
				viewheight = oldViewHeight;

				/* */

				/*
				// Second pass sprites (especially weapon)
				int oldViewwindowy = viewwindowy;
				const bool showSprites = true;
				if (showSprites) {
					// SECOND PASS weapon sprite
					glEnable(GL_TEXTURE_2D);
					screenblocks = 12;
					// TODO: Sprite needs some offset to appear at correct distance, rather than at infinity.
					// Counteract effect of status bar on weapon position
					if (oldScreenBlocks <= 10) { // lower weapon in status mode
						// spriteOffsetY += 0.227 * viewwidth; // empirical - lines up brutal doom down sight in 1920x1080 Rift mode
					}
					sharedRiftHmd->setEyeView(ovrEye_Right, 5, 5000); // Right eye
					renderer.EndDrawScene(viewsector); // right view

					sharedRiftHmd->setEyeView(ovrEye_Left, 5, 5000); // Left eye
					renderer.EndDrawScene(viewsector); // left view
				}

				// Third pass HUD
				screenblocks = max(oldScreenBlocks, 10); // Don't vignette main 3D view
				if (doBufferHud) {
					// Draw HUD again, to avoid flashing? - and render to screen
					// blitHudTextureToScreen(); // HUD pass now occurs in main doom loop! Since I delegated screen->Update to stereo3d.updateScreen().
				}
				*/

				// blitHudTextureToScreen();

				sharedRiftHmd->submitFrame(1.0/calc_mapunits_per_meter(player));

				// Clear crosshair
				HudTexture::crosshairTexture->bindToFrameBuffer();
				glViewport(0, 0, SCREENWIDTH, SCREENHEIGHT);
				glScissor(0, 0, SCREENWIDTH, SCREENHEIGHT);
				glClearColor(0, 0, 0, 0);
				glClear(GL_COLOR_BUFFER_BIT);

				// Clear HUD
				HudTexture::hudTexture->bindToFrameBuffer();
				glViewport(0, 0, SCREENWIDTH, SCREENHEIGHT);
				glScissor(0, 0, SCREENWIDTH, SCREENHEIGHT);
				glClearColor(0,1,1,0);
				glClear(GL_COLOR_BUFFER_BIT);

				screenblocks = oldScreenBlocks;

				//
				// restore global state
				// viewwindowy = oldViewwindowy;
			}

			// Update orientation for NEXT frame, after expensive render has occurred this frame

			// Set up 2D rendering to write to our hud renderbuffer
			bindAndClearHudTexture(*this);
			break;
		}

	case LEFT_EYE_VIEW:
		{
			doBufferHud = false;
			setViewportFull(renderer, bounds);
			setLeftEyeView(renderer, fov0, ratio0, fovratio0, player);
			{
				EyeViewShifter vs(EYE_VIEW_LEFT, player, renderer);
				renderer.RenderOneEye(a1, toscreen, true);
			}
			renderer.EndDrawScene(viewsector);
			break;
		}

	case RIGHT_EYE_VIEW:
		{
			doBufferHud = false;
			setViewportFull(renderer, bounds);
			setRightEyeView(renderer, fov0, ratio0, fovratio0, player);
			{
				EyeViewShifter vs(EYE_VIEW_RIGHT, player, renderer);
				renderer.RenderOneEye(a1, toscreen, true);
			}
			renderer.EndDrawScene(viewsector);
			break;
		}

	case QUAD_BUFFERED:
		{
			doBufferHud = false;
			setViewportFull(renderer, bounds);
			glGetBooleanv(GL_STEREO, &supportsStereo);
			glGetBooleanv(GL_DOUBLEBUFFER, &supportsBuffered);
			if (supportsStereo && supportsBuffered && toscreen)
			{ 
				// Right first this time, so more generic GL_BACK_LEFT will remain for other modes
				glDrawBuffer(GL_BACK_RIGHT);
				setRightEyeView(renderer, fov0, ratio0, fovratio0, player);
				{
					EyeViewShifter vs(EYE_VIEW_RIGHT, player, renderer);
					renderer.RenderOneEye(a1, toscreen, false);
				}
				// Left
				glDrawBuffer(GL_BACK_LEFT);
				setLeftEyeView(renderer, fov0, ratio0, fovratio0, player);
				{
					EyeViewShifter vs(EYE_VIEW_LEFT, player, renderer);
					renderer.RenderOneEye(a1, toscreen, true);
				}
				// Want HUD in both views
				glDrawBuffer(GL_BACK);
			} else { // mono view, in case hardware stereo is not supported
				setMonoView(renderer, fov0, ratio0, fovratio0, player);
				renderer.RenderOneEye(a1, toscreen, true);			
			}
			renderer.EndDrawScene(viewsector);
			break;
		}

	default:
		{
			doBufferHud = false;
			setViewportFull(renderer, bounds);
			setMonoView(renderer, fov0, ratio0, fovratio0, player);
			renderer.RenderOneEye(a1, toscreen, true);			
			renderer.EndDrawScene(viewsector);
			break;
		}

	}
}

void Stereo3D::bindHudTexture(bool doUse)
{
	HudTexture * ht = HudTexture::hudTexture;
	if (ht == NULL)
		return;
	if (! doUse) {
		ht->unbind(); // restore drawing to real screen
	}
	if (! doBufferHud)
		return;
	if (doUse) {
		ht->bindToFrameBuffer();
		glViewport(0, 0, ht->getWidth(), ht->getHeight());
		glScissor(0, 0, ht->getWidth(), ht->getHeight());
	}
	else {
		ht->unbind(); // restore drawing to real screen
		glViewport(0, 0, screen->GetWidth(), screen->GetHeight());
		glScissor(0, 0, screen->GetWidth(), screen->GetHeight());
	}
}

void Stereo3D::updateScreen() {
	if ( (vr_mode == OCULUS_RIFT) && (gamestate != GS_LEVEL) && (gamestate != GS_INTERMISSION) ) {
		gamestate_t x = gamestate;
	}
	// Unbind texture before update, so Fraps could work
	bool htWasBound = false;
	HudTexture * ht = HudTexture::hudTexture;
	if (ht && ht->isBound()) {
		htWasBound = true;
		ht->unbind();
		// blitHudTextureToScreen(); // causes double hud in rift
	}
	// Special handling of intermission screen for Oculus SDK warping
	if ( (vr_mode == OCULUS_RIFT) ) 
	{
		// TODO - what about fraps?
		// update Rift during "other" modes, such as title screen
		if (gamestate != GS_LEVEL) 
		{
			sharedRiftHmd->bindToSceneFrameBufferAndUpdate();
			glClearColor(0.15, 0.03, 0.03, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			//// HUD Pass ////
			HudTexture::hudTexture = checkHudTexture(HudTexture::hudTexture, 1.0);
			HudTexture::hudTexture->bindRenderTexture();
			glEnable(GL_TEXTURE_2D);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			// glAlphaFunc(GL_GREATER, 0.2); // 0.2 -> 0.3 causes console background to show
			// TODO suppress crosshair during hud pass?
			// left eye view - hud pass
			{
				sharedRiftHmd->setSceneEyeView(ovrEye_Left, 10, 10000); // Left eye
				// PositionTrackingShifter positionTracker(sharedRiftHmd, player, renderer);
				sharedRiftHmd->paintHudQuad(vr_hud_scale, 0);
			}
			// right eye view - hud pass
			{
				sharedRiftHmd->setSceneEyeView(ovrEye_Right, 10, 10000); // Right eye
				// PositionTrackingShifter positionTracker(sharedRiftHmd, player, renderer);
				sharedRiftHmd->paintHudQuad(vr_hud_scale, 0);
			}

			sharedRiftHmd->submitFrame(1.0/41.0);
			// Clear HUD
			HudTexture::hudTexture->bindToFrameBuffer();
			glViewport(0, 0, SCREENWIDTH, SCREENHEIGHT);
			glScissor(0, 0, SCREENWIDTH, SCREENHEIGHT);
			glClearColor(0,1,1,0);
			glClear(GL_COLOR_BUFFER_BIT);
		}


	} else {
		screen->Update();
	}
	if (htWasBound)
		ht->bindToFrameBuffer();
}

int Stereo3D::getScreenWidth() {
	return screen->GetWidth();
}
int Stereo3D::getScreenHeight() {
	return screen->GetHeight();
}

void Stereo3D::blitHudTextureToScreen(float yScale) {
	glEnable(GL_TEXTURE_2D);
	if (! doBufferHud)
		return;

	// Compute viewport coordinates
	float h = HudTexture::hudTexture->getHeight() * yScale;
	float w = HudTexture::hudTexture->getWidth();
	float x = (SCREENWIDTH/2-w)*0.5;
	float hudOffsetY = 0.00 * h; // nudge crosshair up
	int hudOffsetX = 0; // kludge to set apparent hud distance

	if (mode == OCULUS_RIFT) {
		// First pass blit unwarped hudTexture into oculusTexture, in two places
		hudOffsetY -= 0.005 * SCREENHEIGHT; // reverse effect of oculus head raising.
		hudOffsetX = (int)(0.004*SCREENWIDTH/2); // kludge to set hud distance
		if (vr_swap)
			hudOffsetX = -hudOffsetX;
		if (screenblocks <= 10)
			hudOffsetY -= 0.080 * h; // lower crosshair when status bar is on
	}

	// Left side
	float y = (SCREENHEIGHT-h)*0.5 + hudOffsetY; // offset to move cross hair up to correct spot
	glViewport(x+hudOffsetX, y, w, h); // Not infinity, but not as close as the weapon.
	HudTexture::hudTexture->renderToScreen();

	// Right side
	x += SCREENWIDTH/2;
	glViewport(x-hudOffsetX, y, w, h);
	HudTexture::hudTexture->renderToScreen();

	if (mode == OCULUS_RIFT) {
	}
}

void Stereo3D::setMode(int m) {
	mode = static_cast<Mode>(m);
};

void Stereo3D::setMonoView(FGLRenderer& renderer, float fov, float ratio, float fovratio, player_t * player) {
	renderer.SetProjection(fov, ratio, fovratio, 0);
}

void Stereo3D::setLeftEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio, player_t * player, bool frustumShift) {
	renderer.SetProjection(fov, ratio, fovratio, vr_swap ? +vr_ipd/2 : -vr_ipd/2, frustumShift);
}

void Stereo3D::setRightEyeView(FGLRenderer& renderer, float fov, float ratio, float fovratio, player_t * player, bool frustumShift) {
	renderer.SetProjection(fov, ratio, fovratio, vr_swap ? -vr_ipd/2 : +vr_ipd/2, frustumShift);
}

bool Stereo3D::hasHeadTracking() const {
	if (! (mode == OCULUS_RIFT) )
		return false;
	return true;
}

void Stereo3D::setViewDirection(FGLRenderer& renderer) {
	// Set HMD angle parameters for NEXT frame
	static float previousYaw = 0;
	if (mode == OCULUS_RIFT) {
		PitchRollYaw prw = getHeadOrientation(renderer);
		double dYaw = prw.yaw - previousYaw;
		G_AddViewAngle(-32768.0*dYaw/3.14159); // determined empirically
		previousYaw = prw.yaw;

		// Pitch
		int pitch = -32768/3.14159*prw.pitch;
		int dPitch = (pitch - viewpitch/65536); // empirical
		G_AddViewPitch(-dPitch);

		// Roll can be local, because it doesn't affect gameplay.
		renderer.mAngles.Roll = prw.roll * 180.0 / 3.14159;
	}
}

// Normal full screen viewport
void Stereo3D::setViewportFull(FGLRenderer& renderer, GL_IRECT * bounds) {
	renderer.SetViewport(bounds);
}

// Left half of screen
void Stereo3D::setViewportLeft(FGLRenderer& renderer, GL_IRECT * bounds) {
	if (bounds) {
		GL_IRECT leftBounds;
		leftBounds.width = bounds->width / 2;
		leftBounds.height = bounds->height;
		leftBounds.left = bounds->left;
		leftBounds.top = bounds->top;
		renderer.SetViewport(&leftBounds);
	}
	else {
		renderer.SetViewport(bounds);
	}
}

// Right half of screen
void Stereo3D::setViewportRight(FGLRenderer& renderer, GL_IRECT * bounds) {
	if (bounds) {
		GL_IRECT rightBounds;
		rightBounds.width = bounds->width / 2;
		rightBounds.height = bounds->height;
		rightBounds.left = bounds->left + rightBounds.width;
		rightBounds.top = bounds->top;
		renderer.SetViewport(&rightBounds);
	}
	else {
		renderer.SetViewport(bounds);
	}
}

/* static */ void LocalHudRenderer::bind()
{
	Stereo3DMode.bindHudTexture(true);
}

/* static */ void LocalHudRenderer::unbind()
{
	Stereo3DMode.bindHudTexture(false);
}

LocalHudRenderer::LocalHudRenderer()
{
	bind();
}

LocalHudRenderer::~LocalHudRenderer()
{
	unbind();
}

