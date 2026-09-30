#ifndef PX_PSD_H
#define PX_PSD_H

/* Editor-only PSD/PSB reader. The runtime never sees this format. */

#ifdef __cplusplus
extern "C" {
#endif

#define PX_PSD_OK 0
#define PX_PSD_ERR_FORMAT 1
#define PX_PSD_ERR_COLOR 2
#define PX_PSD_ERR_MEMORY 3
#define PX_PSD_ERR_EMPTY 4
#define PX_PSD_ERR_LIMIT 5

typedef struct PX_PsdRaster
{
	char *name;
	int left;
	int top;
	int width;
	int height;
	int visible;
	unsigned char *rgba;
} PX_PsdRaster;

typedef struct PX_PsdDocument
{
	int width;
	int height;
	int count;
	PX_PsdRaster *layers;
} PX_PsdDocument;

int PX_PsdReadDocument(const void *bytes, unsigned int size, PX_PsdDocument *out);
void PX_PsdFreeDocument(PX_PsdDocument *document);

#ifdef __cplusplus
}
#endif

#endif
