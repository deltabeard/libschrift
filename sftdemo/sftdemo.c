#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>

#include "../schrift.h"
#include "../utf8.h"

int main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;
	char *str = NULL;
	size_t str_len;
	const char *end;
	const char *font = "font.ttf";
	struct SFT sft = {0};
	SDL_Surface *surf;

	if(argc != 3)
	{
		puts("Usage: sch2bmp IN.txt OUT.bmp");
		goto err;
	}

	SDL_Init(SDL_INIT_VIDEO);

	/* Prepare font. */
	sft.font = sft_loadfile(font);
	if(sft.font == NULL)
	{
		fprintf(stderr, "Unable to initialise font\n");
		goto err;
	}
	sft.xScale = 64;
	sft.yScale = 64;
	sft.flags |= SFT_RENDER_IMAGE;
	//sft.flags |= SFT_DOWNWARD_Y;

	str_len = strlen(argv[2]);
	/* Three bytes of padding required for UTF8 library. */
	str = calloc(str_len + 3, 1);
	if(str == NULL)
		goto err;

	end = str + str_len;
	strcpy(str, argv[1]);

	surf = SDL_CreateRGBSurfaceWithFormat(0, sft.xScale * str_len,
					      sft.yScale, 32, SDL_PIXELFORMAT_ARGB32);

	SDL_Color colors[256];

	for(int i = 0; i < 256; i++)
	{
		colors[i].r = colors[i].g = colors[i].b = i;
		colors[i].a = i;
	}

	colors[0].a = SDL_ALPHA_TRANSPARENT;

	SDL_SetPaletteColors(surf->format->palette, colors, 0, 256);

	while(str < end)
	{
		uint32_t c;
		int e = 0;
		struct SFT_Char chr;
		static SDL_Rect dstr = {0};
		SDL_Surface *src;

		str = utf8_decode(str, &c, &e);

		if(sft_char(&sft, c, &chr) < 0)
		{
			printf("Couldn't load character '%c' (0x%02X).\n", c, c);
		}

		src = SDL_CreateRGBSurfaceWithFormatFrom(chr.image,
							 chr.width, chr.height,
							 8, chr.width, SDL_PIXELFORMAT_INDEX8);
		SDL_SetPaletteColors(src->format->palette, colors, 0, 256);

		dstr.x += chr.advance;
		dstr.y += chr.y;
		dstr.w = chr.width;
		dstr.h = chr.height;

#if 0
		{
			FILE *f;
			uint32_t outfile[12] = {c, 0};

			strcat(outfile, ".data");
			f = fopen(outfile, "wb");
			fwrite(chr.image, 1, chr.width * chr.height, f);
			fclose(f);
		}
#endif

		printf("%c (%d,%d) (%d, %d) adv: %f\n",
		       c, chr.x, chr.y, chr.width, chr.height, chr.advance);
		fflush(stdout);
		SDL_BlitSurface(src, NULL, surf, &dstr);

		SDL_FreeSurface(src);
	}

	SDL_SaveBMP(surf, argv[2]);
	SDL_FreeSurface(surf);
	ret = EXIT_SUCCESS;

out:
	free(str);
	return ret;

err:
	fprintf(stderr, "An error occured\n");
	goto out;
}
