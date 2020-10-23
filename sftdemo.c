#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bmp.h"
#include "schrift.h"

#define RGB2GRAY(r,g,b) ((0.2126f*r) + (0.7152f*g) + (0.0722f*b))

wchar_t *utf8_utf32(char *str)
{
	size_t sz = mbstowcs(NULL, str, 0) + 1;
	wchar_t *ret;

	if(sz == (size_t)-1)
		return NULL;

	ret = malloc(sz);
	if(ret == NULL)
		goto out;

	mbstowcs(ret, str, sz);

out:
	return ret;
}

unsigned get_image_width(struct SFT *sft, wchar_t *wstr)
{
	unsigned width = 0;

	for(wchar_t *w = wstr; *w != L'\0'; w++)
	{
		struct SFT_Char chr;

		if(sft_char(sft, *w, &chr) < 0)
			continue;

		width += (unsigned)ceil(chr.advance);
	}

	return width;
}

unsigned get_number_of_lines(wchar_t *str)
{
	unsigned lines = 1;
	wchar_t *w;

	for(w = str; *w != L'\0'; w++)
	{
		if(*w != L'\n')
			continue;

		lines++;
	}

	/* If the end of file is a new line, do not print that line. */
	if(*(w - 1) == L'\n')
		lines--;

	return lines;
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

	ret = malloc(s + 1);
	if(ret == NULL)
	{
		fclose(f);
		goto out;
	}

	s = fread(ret, 1, s, f);
	ret[s] = '\0';
	fclose(f);

out:
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;
	struct SFT sft = {
		.xScale = 64, .yScale = 64
	};
	wchar_t *wstr = NULL;
	//int width, height;

	if(argc != 4)
	{
		puts("Usage: sch2bmp TEXT.txt OUT.bmp FONT.ttf");
		return 1;
	}

	/* Prepare font. */
	sft.font = sft_loadfile(argv[3]);
	if(sft.font == NULL)
	{
		fprintf(stderr, "Unable to initialise font\n");
		goto err;
	}

	/* Convert UTF8 string to UTF32. */
	{
		char *str = NULL;
		str = open_and_read_file(argv[1]);
		if(str == NULL)
		{
			fprintf(stderr, "Unable to read text file %s\n",
				argv[1]);
			goto err;
		}

		wstr = utf8_utf32(str);
		free(str);

		if(wstr == NULL)
			goto err;
	}

	//width = get_image_width(&sft, wstr);
	//height = get_number_of_lines(wstr) * 64;

	/* Render image after we get the destination width. */
	sft.flags |= SFT_RENDER_IMAGE;
	//sft.flags |= SFT_DOWNWARD_Y;

	for(wchar_t *wide = wstr; *wide != L'\0'; wide++)
	{
		struct SFT_Char chr;
		size_t sz;
		unsigned char *gray_image;
		void *image;
#if 0
		if(*wide == L'\n')
		{
			dstr.y += 64;
			continue;
	}
#endif

		if(sft_char(&sft, *wide, &chr) < 0)
		{
			fprintf(stderr, "Couldn't load character 0x%04X.\n",
				*wide);
			continue;
		}

		if(chr.width == 0)
			continue;

		sz = bmp_size(chr.width, chr.height);
		image = calloc(sz, 1);
		if(image == NULL)
		{
			fprintf(stderr, "calloc failure\n");
			goto err;
		}

		gray_image = chr.image;
		bmp_init(image, chr.width, chr.height);
		for(int y = 0; y < chr.height; y++)
		{
			for(int x = 0; x < chr.width; x++)
			{
				unsigned char g = *gray_image;
				unsigned long rgb = g | g << 8 | g << 16;
				bmp_set(image, x, y, rgb);
				gray_image++;
			}
		}

		{
			char buf[256];
			FILE *f;
			snprintf(buf, sizeof(buf), "%s_%lc.bmp", argv[2], *wide);
			f = fopen(buf, "wb");
			fwrite(image, 1, sz, f);
			fclose(f);
		}

		free(chr.image);
		free(image);
	}

#if 0
	{
		const char *filename = argv[2];
		FILE *f = fopen(filename, "wb");
		fwrite(image, 1, BMP_SIZE(width, height), f);
		fclose(f);
}
#endif

	ret = EXIT_SUCCESS;

out:
	free(wstr);
	sft_freefont(sft.font);
	return ret;

err:
	fprintf(stderr, "Error\n");
	goto out;
}
