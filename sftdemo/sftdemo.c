#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>

#include "../schrift.h"
#include "../utf8.h"

wchar_t *utf8_utf32(char *str)
{
	size_t sz = mbstowcs(NULL, str, 0) + 1;
	wchar_t *ret;

	if(sz == (size_t)-1)
		return NULL;

	ret = SDL_malloc(sz);
	if(ret == NULL)
		goto out;

	mbstowcs(ret, str, sz);

out:
	return ret;
}

int get_image_width(struct SFT *sft, wchar_t *wide)
{
	int width = 0;

	for(wchar_t *w = wide; *w != L'\0'; w++)
	{
		struct SFT_Char chr;

		if(sft_char(sft, *w, &chr) < 0)
			continue;

		width += chr.advance;
	}

	return width;
}

char *open_and_read_file(const char *filename)
{
	FILE *f;
	size_t s;
	char *ret = NULL;

	f = fopen(filename, "rb");
	if(f == NULL)
		goto out;

	fseek(f, 0, SEEK_END);
	s = ftell(f);
	rewind(f);

	ret = malloc(s);
	if(ret == NULL)
	{
		fclose(f);
		goto out;
	}

	fread(ret, 1, s, f);
	fclose(f);

out:
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;
	char *str = NULL;
	const char font[] = "font.ttf";
	struct SFT sft = {
		.xScale = 64, .yScale = 64
	};
	SDL_Surface *surf;
	wchar_t *wstr = NULL;
	int width = 0;

	if(argc != 3)
	{
		puts("Usage: sch2bmp TEXT.txt OUT.bmp");
		goto err;
	}

	SDL_Init(SDL_INIT_VIDEO);

	/* Prepare font. */
	sft.font = sft_loadfile(font);
	if(sft.font == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Unable to initialise font");
		goto err;
	}

	str = open_and_read_file(argv[1]);
	if(str == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Unable to read text file %s", argv[1]);
		goto err;
	}

	wstr = utf8_utf32(str);
	width = get_image_width(&sft, wstr);

	/* Render image after we get the destination width. */
	sft.flags |= SFT_RENDER_IMAGE;

	surf = SDL_CreateRGBSurfaceWithFormat(0, width,
					      sft.yScale, 32,
					      SDL_PIXELFORMAT_ARGB32);

	SDL_Color colors[256];
	for(int i = 0; i < 256; i++)
	{
		colors[i].r = colors[i].g = colors[i].b = i;
		colors[i].a = i;
	}

	SDL_SetPaletteColors(surf->format->palette, colors, 0, 256);

	for(wchar_t *wide = wstr; *wide != L'\0'; wide++)
	{
		struct SFT_Char chr;
		static SDL_Rect dstr;
		SDL_Surface *src;

		if(sft_char(&sft, *wide, &chr) < 0)
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
				    "Couldn't load character 0x%04X.\n", *wide);
			continue;
		}

		src = SDL_CreateRGBSurfaceWithFormatFrom(chr.image,
							 chr.width, chr.height,
							 8, chr.width,
							 SDL_PIXELFORMAT_INDEX8);
		SDL_SetPaletteColors(src->format->palette, colors, 0, 256);

		dstr.x += chr.x;
		dstr.y += chr.y;
		dstr.w = chr.width;
		dstr.h = chr.height;

		fprintf(stdout, "%c (%d,%d) (%d, %d) adv: %f\n",
			*wide, chr.x, chr.y, chr.width, chr.height, chr.advance);

		SDL_BlitSurface(src, NULL, surf, &dstr);
		dstr.x += chr.advance - chr.x;

		SDL_FreeSurface(src);
		SDL_free(chr.image);
		fflush(stdout);
	}

	SDL_SaveBMP(surf, argv[2]);
	SDL_FreeSurface(surf);
	ret = EXIT_SUCCESS;

out:
	SDL_free(wstr);
	free(str);
	SDL_Quit();
	return ret;

err:
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "An error occured");
	goto out;
}
