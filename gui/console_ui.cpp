/*
	Copyright 2015 bigbiff/Dees_Troy TeamWin
	This file is part of TWRP/TeamWin Recovery Project.

	TWRP is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	TWRP is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with TWRP.  If not, see <http://www.gnu.org/licenses/>.
*/

// console.cpp - GUIConsole object

#include "../minuitwrp/minui.h"
#include "rapidxml.hpp"
#include "objects.hpp"
#include "gui.hpp"

GUIConsole::GUIConsole(xml_node<>* node) : GUIScrollList(node)
{
	xml_node<>* child;

	mLastCount = 0;
	scrollToEnd = true;
	mSlideoutX = mSlideoutY = mSlideoutW = mSlideoutH = 0;
	mSlideout = 0;
	mSlideoutState = visible;

	allowSelection = false;	// console doesn't support list item selections

	if (!node)
	{
		mRenderX = 0;
		mRenderY = 0;
		mRenderW = gr_fb_width();
		mRenderH = gr_fb_height();
	}
	else
	{
		child = FindNode(node, "color");
		if (child)
		{
			mFontColor = LoadAttrColor(child, "foreground", mFontColor);
			mBackgroundColor = LoadAttrColor(child, "background", mBackgroundColor);
			//mScrollColor = LoadAttrColor(child, "scroll", mScrollColor);
		}

		child = FindNode(node, "slideout");
		if (child)
		{
			mSlideout = 1;
			mSlideoutState = hidden;
			LoadPlacement(child, &mSlideoutX, &mSlideoutY, &mSlideoutW, &mSlideoutH, &mPlacement);

			mSlideoutImage = LoadAttrImage(child, "resource");

			if (mSlideoutImage && mSlideoutImage->GetResource())
			{
				mSlideoutW = mSlideoutImage->GetWidth();
				mSlideoutH = mSlideoutImage->GetHeight();
				if (mPlacement == CENTER || mPlacement == CENTER_X_ONLY) {
					mSlideoutX = mSlideoutX - (mSlideoutW / 2);
					if (mPlacement == CENTER) {
						mSlideoutY = mSlideoutY - (mSlideoutH / 2);
					}
				}
			}
		}
	}
}

int GUIConsole::RenderSlideout(void)
{
	if (!mSlideoutImage || !mSlideoutImage->GetResource())
		return -1;

	gr_blit(mSlideoutImage->GetResource(), 0, 0, mSlideoutW, mSlideoutH, mSlideoutX, mSlideoutY);
	return 0;
}

int GUIConsole::RenderConsole(void)
{
	Translate_Now();
	pthread_mutex_lock(&console_lock);
	AddLines(&gConsole, &gConsoleColor, &mLastCount, &rConsole, &rConsoleColor);
	pthread_mutex_unlock(&console_lock);
	GUIScrollList::Render();

	// if last line is fully visible, keep tracking the last line when new lines are added
	int bottom_offset = GetDisplayRemainder() - actualItemHeight;
	bool isAtBottom = firstDisplayedItem == (int)GetItemCount() - GetDisplayItemCount() - (bottom_offset != 0) && y_offset == bottom_offset;
	if (isAtBottom)
		scrollToEnd = true;
#if 0
	// debug - show if we are tracking the last line
	if (scrollToEnd) {
		gr_color(0,255,0,255);
		gr_fill(mRenderX+mRenderW-5, mRenderY+mRenderH-5, 5, 5);
	} else {
		gr_color(255,0,0,255);
		gr_fill(mRenderX+mRenderW-5, mRenderY+mRenderH-5, 5, 5);
	}
#endif
	return (mSlideout ? RenderSlideout() : 0);
}

int GUIConsole::Render(void)
{
	if (!isConditionTrue())
		return 0;

	if (mSlideout && mSlideoutState == hidden)
		return RenderSlideout();

	return RenderConsole();
}

int GUIConsole::Update(void)
{
	if (mSlideout && mSlideoutState != visible)
	{
		if (mSlideoutState == hidden)
			return 0;

		if (mSlideoutState == request_hide)
			mSlideoutState = hidden;

		if (mSlideoutState == request_show)
			mSlideoutState = visible;

		// Any time we activate the console, we reset the position
		SetVisibleListLocation(rConsole.size() - 1);
		mUpdate = 1;
		scrollToEnd = true;
	}

	pthread_mutex_lock(&console_lock);
	bool addedNewText = AddLines(&gConsole, &gConsoleColor, &mLastCount, &rConsole, &rConsoleColor);
	pthread_mutex_unlock(&console_lock);
	if (addedNewText) {
		// someone added new text
		// at least the scrollbar must be updated, even if the new lines are currently not visible
		mUpdate = 1;
	}

	if (scrollToEnd) {
		// keep the last line in view
		SetVisibleListLocation(rConsole.size() - 1);
	}

	GUIScrollList::Update();

	if (mUpdate) {
		mUpdate = 0;
		if (Render() == 0)
			return 2;
	}
	return 0;
}

// IsInRegion - Checks if the request is handled by this object
//  Return 1 if this object handles the request, 0 if not
int GUIConsole::IsInRegion(int x, int y)
{
	if (mSlideout) {
		// Check if they tapped the slideout button
		if (x >= mSlideoutX && x < mSlideoutX + mSlideoutW && y >= mSlideoutY && y < mSlideoutY + mSlideoutH)
			return 1;

		// If we're only rendering the slideout, bail now
		if (mSlideoutState == hidden)
			return 0;
	}

	return GUIScrollList::IsInRegion(x, y);
}

// NotifyTouch - Notify of a touch event
//  Return 0 on success, >0 to ignore remainder of touch, and <0 on error
int GUIConsole::NotifyTouch(TOUCH_STATE state, int x, int y)
{
	if (!isConditionTrue())
		return -1;

	if (mSlideout && x >= mSlideoutX && x < mSlideoutX + mSlideoutW && y >= mSlideoutY && y < mSlideoutY + mSlideoutH) {
		if (state == TOUCH_START) {
			if (mSlideoutState == hidden)
				mSlideoutState = request_show;
			else if (mSlideoutState == visible)
				mSlideoutState = request_hide;
		}
		return 1;
	}
	scrollToEnd = false;
	return GUIScrollList::NotifyTouch(state, x, y);
}

size_t GUIConsole::GetItemCount()
{
	return rConsole.size();
}

void GUIConsole::RenderItem(size_t itemindex, int yPos, bool selected __unused)
{
	// Set the color for the font
	if (rConsoleColor[itemindex] == "normal") {
		gr_color(mFontColor.red, mFontColor.green, mFontColor.blue, mFontColor.alpha);
	} else {
		COLOR FontColor;
		std::string color = rConsoleColor[itemindex];
		ConvertStrToColor(color, &FontColor);
		FontColor.alpha = 255;
		gr_color(FontColor.red, FontColor.green, FontColor.blue, FontColor.alpha);
	}

	// render text
	const char* text = rConsole[itemindex].c_str();
	gr_textEx_scaleW(mRenderX, yPos, text, mFont->GetResource(), mRenderW, TOP_LEFT, 0);
}

void GUIConsole::NotifySelect(size_t item_selected __unused)
{
	// do nothing - console ignores selections
}

void GUIConsole::Translate_Now()
{
	pthread_mutex_lock(&console_lock);
	size_t message_count = gMessages.size();
	if (message_count <= last_message_count)
	{
		pthread_mutex_unlock(&console_lock);
		return;
	}

	for (size_t m = last_message_count; m < message_count; m++) {
		std::string message = gMessages[m];
		std::string color = "normal";
		if (gMessages[m].GetKind() == msg::kError)
			color = "error";
		else if (gMessages[m].GetKind() == msg::kHighlight)
			color = "highlight";
		else if (gMessages[m].GetKind() == msg::kWarning)
			color = "warning";
		else if (gMessages[m].GetKind() == msg::kProcess)
			color = "process";
		gConsole.push_back(message);
		gConsoleColor.push_back(color);
	}
	last_message_count = message_count;
	pthread_mutex_unlock(&console_lock);
}

void GUIConsole::Clear_For_Retranslation()
{
	pthread_mutex_lock(&console_lock);
	last_message_count = 0;
	gConsole.clear();
	gConsoleColor.clear();
	pthread_mutex_unlock(&console_lock);
}