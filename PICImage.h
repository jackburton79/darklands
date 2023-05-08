/*
 * PicDecoder.h
 *
 *  Created on: 27 apr 2023
 *      Author: Stefano Ceccherini
 */

#ifndef PICIMAGE_H_
#define PICIMAGE_H_

#include "SupportDefs.h"

class Bitmap;
class Stream;
class PICImage {
public:
	PICImage(Stream* stream);
	~PICImage();

	uint16 Width() const;
	uint16 Height() const;

	Bitmap* Image();

private:
	Stream* fStream;
};

#endif /* PICIMAGE_H_ */
