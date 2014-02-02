#include "xbrz.h"

extern "C" void GL_XBRZ_ResampleTexture(unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight)
{
    int width_factor = (outwidth / inwidth);
	int height_factor (outheight / inheight);
	int factor = (width_factor < height_factor ? width_factor : height_factor);
	unsigned *tmp = NULL;
	int tmpwidth;
	int tmpheight;

	if (factor > 5)
		factor = 5;

	if (factor >= 2)
	{
		tmpwidth = inwidth * factor;
		tmpheight = inheight * factor;
		tmp = (unsigned*)malloc(tmpwidth*tmpheight*sizeof(unsigned));
		xbrz::scale(
			factor,
			const_cast<const uint32_t*>(static_cast<uint32_t*>(in)),
			tmp,
			inwidth,
			inheight);
	}
	else
	{
		tmp = in;
		tmpwidth = inwidth;
		tmpheight = inheight;
	}


	xbrz::nearestNeighborScale(
		tmp,
		tmpwidth,
		tmpheight,
		tmpwidth*sizeof(uint32_t),
		static_cast<uint32_t*>(out),
		outwidth,
		outheight,
		outwidth*sizeof(uint32_t),
		xbrz::NN_SCALE_SLICE_SOURCE,
		0,
		outheight);

    if (tmp && tmp != in)
		free(tmp);
}
