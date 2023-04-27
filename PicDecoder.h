/*
 * PicDecoder.h
 *
 *  Created on: 27 apr 2023
 *      Author: Stefano Ceccherini
 */

#ifndef PICDECODER_H_
#define PICDECODER_H_

class Bitmap;
class Stream;
class PicDecoder {
public:
	PicDecoder();
	~PicDecoder();

	Bitmap* GetImage(Stream* stream);
};

#endif /* PICDECODER_H_ */