#ifndef __RECTUTILS_H
#define __RECTUTILS_H

#include "Bitmap.h"
#include "GraphicsDefs.h"


static inline GFX::rect
offset_rect(const GFX::rect &rect, sint16 x, sint16 y)
{
	GFX::rect newRect = rect;
	newRect.x += x;
	newRect.y += y;
	return newRect;
}



static inline GFX::rect
offset_rect_to(const GFX::rect &rect, sint16 x, sint16 y)
{
	GFX::rect newRect = rect;
	newRect.x = x;
	newRect.y = y;
	return newRect;
}


static inline GFX::point
offset_point(const GFX::point &point, sint16 x, sint16 y)
{
	GFX::point newPoint = point;
	newPoint.x += x;
	newPoint.y += y;
	return newPoint;
}


static inline bool
rects_intersect(const GFX::rect &rectA, const GFX::rect &rectB)
{
	return !(rectA.x > rectB.x + rectB.w
			|| rectA.y > rectB.y + rectB.h
			|| rectA.x + rectA.w < rectB.x
			|| rectA.y + rectA.h < rectB.y);
}


static inline bool
rect_contains(const GFX::rect& rect, const int16 x, const int16 y)
{
	if (x >= rect.x && x <= rect.x + rect.w
		&& y >= rect.y && y <= rect.y + rect.h)
		return true;
	return false;
}


static inline GFX::rect
center_rect_in_rect(const GFX::rect& toCenter, const GFX::rect& windowRect)
{
	GFX::rect rect = toCenter;

	rect.x = (windowRect.w - toCenter.x) / 2;
	rect.y = (windowRect.h - toCenter.y) / 2;

	return rect;
}


#endif // __RECTUTILS_H
