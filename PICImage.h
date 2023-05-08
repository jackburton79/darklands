/*
 * PicDecoder.h
 *
 *  Created on: 27 apr 2023
 *      Author: Stefano Ceccherini
 */

#ifndef PICIMAGE_H_
#define PICIMAGE_H_

class Bitmap;
class DecodingContext;
class Stream;
class PICImage {
public:
	PICImage();
	~PICImage();

	Bitmap* GetImage(Stream* stream);
private:
	DecodingContext* fContext;
};

#endif /* PICIMAGE_H_ */
