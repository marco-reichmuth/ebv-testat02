/* Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty. This file is offered as-is,
 * without any warranty.
 */

/*! @file process_frame.c
 * @brief Contains the actual algorithm and calculations.
 */

/* Definitions specific to this application. Also includes the Oscar main header file. */
#include "template.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

OSC_ERR OscVisDrawBoundingBoxBW(struct OSC_PICTURE *picIn,
		struct OSC_VIS_REGIONS *regions, uint8 Color);

void ProcessFrame(uint8 *pInputImg) {
	int c, r;
	int nc = OSC_CAM_MAX_IMAGE_WIDTH / 2;
	int siz = sizeof(data.u8TempImage[GRAYSCALE]);

	struct OSC_PICTURE Pic1, Pic2;//we require these structures to use Oscar functions
	struct OSC_VIS_REGIONS ImgRegions;//these contain the foreground objects

	//Testat2
	uint32 Hist[256]; //Histogram
	uint8 *p = data.u8TempImage[GRAYSCALE]; //Pointer to picture
	uint32 W0[256]; //Omega0
	uint32 W1[256]; //Omepa1
	uint32 M0[256]; //My0
	uint32 M1[256]; //My1
	uint32 sb2[256]; //Sigma b im Quadrat
	short k, g, thr;

	memset(Hist, 0, sizeof(Hist));

	if (data.ipc.state.nStepCounter == 1) {
		/* this is the first time we call this function */
		/* first time we call this; index 1 always has the background image */
		memcpy(data.u8TempImage[BACKGROUND], data.u8TempImage[GRAYSCALE],
				sizeof(data.u8TempImage[GRAYSCALE]));
		/* set foreground counter to zero */
		memset(data.u8TempImage[FGRCOUNTER], 0,
				sizeof(data.u8TempImage[FGRCOUNTER]));
	} else {
		/* this is the default case */
		//Wenn Threshold ungleich 0 dann wird manueller Schwellwert benutzt
		if (data.ipc.state.nThreshold != 0) {
			for (r = 0; r < siz; r += nc)/* we strongly rely on the fact that them images have the same size */
			{
				for (c = 0; c < nc; c++) {
					/* first determine the foreground estimate */
					data.u8TempImage[THRESHOLD][r + c]
							= (short) data.u8TempImage[GRAYSCALE][r + c]
									> data.ipc.state.nThreshold ? 0 : 0xff; //Testat2 Manueller Schwellwert auf Bild anwenden
				}
			}

			//Wenn Threshold gleich 0 dann wird ein automatischer Schwellwert benutzt
		} else {
			// Histogramm füllen
			for (r = 0; r < siz; r += nc) {
				for (c = 0; c < nc; c++) {
					Hist[p[r + c]] += 1;
				}
			}
			//grösstes Sigma (thr) auf 0 setzen
			thr = 0;
			//Histogramm auswerten
			for (k = 0; k <= 255; k++) {
				//W0 und M0 bestimmen (Zuerst auf 0 setzen)
				W0[k] = 0;
				M0[k] = 0;
				for (g = 0; g <= k; g++) {
					W0[k] += Hist[g];
					M0[k] += Hist[g] * g;
				}
				//W1 und M1 bestimmen (Zuerst auf 0 setzen)
				W1[k] = 0;
				M1[k] = 0;
				for (g = k + 1; g <= 255; g++) {
					W1[k] += Hist[g];
					M1[k] += Hist[g] * g;
				}
				//sb2 bestimmen
				sb2[k] = (uint32) (pow(((1 / (double) W0[k]) * (double) M0[k]
						- (1 / (double) W1[k]) * (double) M1[k]), 2) / 90240
						* W0[k] * W1[k]);
				//grösstes Signma bestimmen
				if (sb2[k] > sb2[thr]) {
					thr = k;
				}
			}
			//Otsu auf Bild anwenden
			for (r = 0; r < siz; r += nc)/* we strongly rely on the fact that them images have the same size */
			{
				for (c = 0; c < nc; c++) {
					/* first determine the foreground estimate */
					data.u8TempImage[THRESHOLD][r + c]
							= (short) data.u8TempImage[GRAYSCALE][r + c] > thr ? 0
									: 0xff; //Testat2
				}
			}
			//for debugging -> log Histogram
			//			for (k=0;k<=255;k++)
			//			{
			//				//log all
			//				OscLog(INFO, "[%d]\t %d\t %d\t %d\t %d\t %d\t %f\n",k,Hist[k],W0[k],M0[k],W1[k],M1[k],ob2[k]);
			//				//log custom
			//				OscLog(INFO, "Hist[%d]\t %d\n", k , Hist[k]);
			//				OscLog(INFO, "W0[%d]\t %d\n", k , W0[k]);
			//				OscLog(INFO, "M0[%d]\t %d\n", k , M0[k]);
			//				OscLog(INFO, "W1[%d]\t %d\n", k , W1[k]);
			//				OscLog(INFO, "M1[%d]\t %d\n", k , M1[k]);
			//	O			scLog(INFO, "ob2[%d]\t %d\n", k , ob2[k]);
			//
			//			}
			//			OscLog(INFO, "thr\t %d\n", thr);


		}

		// Schliessung machen (Dilation & Erosion)

		for (r = nc; r < siz - nc; r += nc)/* we skip the first and last line */
		{
			for (c = 1; c < nc - 1; c++)/* we skip the first and last column */
			{
				unsigned char* p = &data.u8TempImage[THRESHOLD][r + c];
				data.u8TempImage[DILATION][r + c] = *(p - nc - 1) | *(p - nc)
						| *(p - nc + 1) | *(p - 1) | *p | *(p + 1) | *(p + nc
						- 1) | *(p + nc) | *(p + nc + 1);

			}
		}

		for (r = nc; r < siz - nc; r += nc)/* we skip the first and last line */
		{
			for (c = 1; c < nc - 1; c++)/* we skip the first and last column */
			{
				unsigned char* p = &data.u8TempImage[DILATION][r + c];
				data.u8TempImage[EROSION][r + c] = *(p - nc - 1) & *(p - nc)
						& *(p - nc + 1) & *(p - 1) & *p & *(p + 1) & *(p + nc
						- 1) & *(p + nc) & *(p + nc + 1);
			}
		}

		// Testat2


		//wrap image DILATION in picture struct
		Pic1.data = data.u8TempImage[DILATION];
		Pic1.width = nc;
		Pic1.height = OSC_CAM_MAX_IMAGE_HEIGHT / 2;
		Pic1.type = OSC_PICTURE_GREYSCALE;
		//as well as EROSION (will be used as output)
		Pic2.data = data.u8TempImage[EROSION];
		Pic2.width = nc;
		Pic2.height = OSC_CAM_MAX_IMAGE_HEIGHT / 2;
		Pic2.type = OSC_PICTURE_BINARY;//probably has no consequences
		//have to convert to OSC_PICTURE_BINARY which has values 0x01 (and not 0xff)
		OscVisGrey2BW(&Pic1, &Pic2, 0x80, false);

		//now do region labeling and feature extraction
		OscVisLabelBinary(&Pic2, &ImgRegions);
		OscVisGetRegionProperties(&ImgRegions);

		//OscLog(INFO, "number of objects %d\n", ImgRegions.noOfObjects);
		//plot bounding boxes both in gray and dilation image
		Pic2.data = data.u8TempImage[GRAYSCALE];
		OscVisDrawBoundingBoxBW(&Pic2, &ImgRegions, 255);
		OscVisDrawBoundingBoxBW(&Pic1, &ImgRegions, 128);
	}
}


/* Drawing Function for Bounding Boxes; own implementation because Oscar only allows colored boxes; here in Gray value "Color"  */
/* should only be used for debugging purposes because we should not drawn into a gray scale image */
OSC_ERR OscVisDrawBoundingBoxBW(struct OSC_PICTURE *picIn,
	struct OSC_VIS_REGIONS *regions, uint8 Color) {
uint16 i, o;
uint8 *pImg = (uint8*) picIn->data;
const uint16 width = picIn->width;
for (o = 0; o < regions->noOfObjects; o++)//loop over regions
{
	/* Draw the horizontal lines. */
	for (i = regions->objects[o].bboxLeft; i < regions->objects[o].bboxRight; i
			+= 1) {
		pImg[width * regions->objects[o].bboxTop + i] = Color;
		pImg[width * (regions->objects[o].bboxBottom - 1) + i] = Color;
	}

	/* Draw the vertical lines. */
	for (i = regions->objects[o].bboxTop; i < regions->objects[o].bboxBottom
			- 1; i += 1) {
		pImg[width * i + regions->objects[o].bboxLeft] = Color;
		pImg[width * i + regions->objects[o].bboxRight] = Color;
	}
}
return SUCCESS;
}

