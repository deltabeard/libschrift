/* See LICENSE file for copyright and license details. */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "schrift.h"

#define SCHRIFT_VERSION "0.8.0"

#define FILE_MAGIC_ONE             0x00010000
#define FILE_MAGIC_TWO             0x74727565

#define HORIZONTAL_KERNING         0x01
#define MINIMUM_KERNING            0x02
#define CROSS_STREAM_KERNING       0x04
#define OVERRIDE_KERNING           0x08

#define POINT_IS_ON_CURVE          0x01
#define X_CHANGE_IS_SMALL          0x02
#define Y_CHANGE_IS_SMALL          0x04
#define REPEAT_FLAG                0x08
#define X_CHANGE_IS_ZERO           0x10
#define X_CHANGE_IS_POSITIVE       0x10
#define Y_CHANGE_IS_ZERO           0x20
#define Y_CHANGE_IS_POSITIVE       0x20

#define OFFSETS_ARE_LARGE          0x001
#define ACTUAL_XY_OFFSETS          0x002
#define GOT_A_SINGLE_SCALE         0x008
#define THERE_ARE_MORE_COMPONENTS  0x020
#define GOT_AN_X_AND_Y_SCALE       0x040
#define GOT_A_SCALE_MATRIX         0x080

/* macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SIGN(x) ((x) >= 0 ? 1 : -1)
/* Allocate values on the stack if they are small enough, else spill to heap. */
#define STACK_ALLOC(var, len, thresh) \
	uint8_t var##_stack_[thresh]; \
	var = (len) <= (thresh) ? (void *) var##_stack_ : malloc(len);
#define STACK_FREE(var) \
	if ((void *) var != (void *) var##_stack_) free(var);

enum { SrcMapping, SrcUser };

/* structs */
struct point { double x, y; };
struct line  { uint_least16_t beg, end; };
struct curve { uint_least16_t beg, end, ctrl; };
struct cell  { double area, cover; };

struct outline
{
	struct point *points;
	struct curve *curves;
	struct line *lines;
	unsigned int numPoints, numCurves, numLines;
	unsigned int capPoints, capCurves, capLines;
};

struct buffer
{
	struct cell **rows;
	int width, height;
};

struct SFT_Font
{
	const uint8_t *memory;
	unsigned long size;
	int source;
};

/* function declarations */
/* file loading */
static int  map_file(SFT_Font *font, const char *filename);
static void unmap_file(SFT_Font *font);
/* mathematical utilities */
static struct point midpoint(struct point a, struct point b);
static void transform_points(int numPts, struct point *points, double trf[6]);
static void clip_points(int numPts, struct point *points, int width, int height);
/* 'buffer' data structure management */
static int  init_buffer(struct buffer *buf, int width, int height);
static void free_buffer(struct buffer *buf);
static void flip_buffer(struct buffer *buf);
/* 'outline' data structure management */
static int  init_outline(struct outline *outl);
static void free_outline(struct outline *outl);
static int  grow_points(struct outline *outl);
static int  grow_curves(struct outline *outl);
static int  grow_lines(struct outline *outl);
/* TTF parsing utilities */
static void *csearch(const void *key, const void *base,
	size_t nmemb, size_t size, int (*compar)(const void *, const void *));
static int  cmpu16(const void *a, const void *b);
static int  cmpu32(const void *a, const void *b);
static inline uint8_t  getu8 (SFT_Font *font, unsigned long offset);
static inline int8_t   geti8 (SFT_Font *font, unsigned long offset);
static inline uint16_t getu16(SFT_Font *font, unsigned long offset);
static inline int16_t  geti16(SFT_Font *font, unsigned long offset);
static inline uint32_t getu32(SFT_Font *font, unsigned long offset);
static long gettable(SFT_Font *font, char tag[4]);
/* sft -> transform */
static int  units_per_em(SFT_Font *font);
/* codepoint -> glyph */
static long cmap_fmt4(SFT_Font *font, unsigned long table, unsigned long charCode);
static long cmap_fmt6(SFT_Font *font, unsigned long table, unsigned long charCode);
static long glyph_id(SFT_Font *font, unsigned long charCode);
/* glyph -> hmtx */
static int  num_long_hmtx(SFT_Font *font);
static int  hor_metrics(SFT_Font *font, long glyph, int *advanceWidth, int *leftSideBearing);
/* glyph -> outline */
static long outline_offset(SFT_Font *font, long glyph);
/* decoding outlines */
static long simple_flags(SFT_Font *font, unsigned long offset, int numPts, uint8_t *flags);
static int  simple_points(SFT_Font *font, unsigned long offset, int numPts, uint8_t *flags, struct point *points);
static int  decode_contour(uint8_t *flags, unsigned int basePoint, unsigned int count, struct outline *outl);
static int  simple_outline(SFT_Font *font, unsigned long offset, int numContours, struct outline *outl);
static int  compound_outline(SFT_Font *font, unsigned long offset, int recDepth, struct outline *outl);
static int  decode_outline(SFT_Font *font, unsigned long offset, int recDepth, struct outline *outl);
/* tesselation */
static int  is_flat(struct outline *outl, struct curve curve, double flatness);
static int  tesselate_curve(struct curve curve, struct outline *outl);
static int  tesselate_curves(struct outline *outl);
/* silhouette rasterization */
static void draw_dot(struct buffer buf, int px, int py, double xAvg, double yDiff);
static void draw_line(struct buffer buf, struct point origin, struct point goal);
static void draw_lines(struct outline *outl, struct buffer buf);
/* post-processing */
static void post_process(struct buffer buf, uint8_t *image);

/* function implementations */

const char *
sft_version(void)
{
	return SCHRIFT_VERSION;
}

/* Loads a font from a user-supplied memory range. */
SFT_Font *
sft_loadmem(const void *mem, unsigned long size)
{
	SFT_Font *font;
	unsigned long scalerType;
	if ((font = calloc(1, sizeof(SFT_Font))) == NULL) {
		return NULL;
	}
	font->memory = mem;
	font->size = size;
	font->source = SrcUser;
	/* Check for a compatible scalerType (magic number). */
	scalerType = getu32(font, 0);
	if (scalerType != FILE_MAGIC_ONE && scalerType != FILE_MAGIC_TWO) {
		sft_freefont(font);
		return NULL;
	}
	return font;
}

/* Loads a font from the file system. To do so, it has to map the entire font into memory. */
SFT_Font *
sft_loadfile(char const *filename)
{
	SFT_Font *font;
	unsigned long scalerType;
	if ((font = calloc(1, sizeof(SFT_Font))) == NULL) {
		return NULL;
	}
	if (map_file(font, filename) < 0) {
		free(font);
		return NULL;
	}
	/* Check for a compatible scalerType (magic number). */
	scalerType = getu32(font, 0);
	if (scalerType != FILE_MAGIC_ONE && scalerType != FILE_MAGIC_TWO) {
		sft_freefont(font);
		return NULL;
	}
	return font;
}

void
sft_freefont(SFT_Font *font)
{
	if (font == NULL) return;
	/* Only unmap if we mapped it ourselves. */
	if (font->source == SrcMapping)
		unmap_file(font);
	free(font);
}

int
sft_linemetrics(const struct SFT *sft, double *ascent, double *descent, double *gap)
{
	double factor;
	long hhea;
	int unitsPerEm;
	if ((unitsPerEm = units_per_em(sft->font)) < 0)
		return -1;
	if ((hhea = gettable(sft->font, "hhea")) < 0)
		return -1;
	if (sft->font->size < (unsigned long) hhea + 36) return -1;
	factor = sft->yScale / unitsPerEm;
	*ascent  = geti16(sft->font, hhea + 4) * factor;
	*descent = geti16(sft->font, hhea + 6) * factor;
	*gap     = geti16(sft->font, hhea + 8) * factor;
	return 0;
}

int
sft_kerning(const struct SFT *sft, unsigned long leftChar, unsigned long rightChar, double kerning[2])
{
	void *match;
	unsigned long offset;
	long kern;
	unsigned int numTables, numPairs, length, format, flags, value;
	int unitsPerEm;
	uint8_t key[4];

	kerning[0] = 0.0;
	kerning[1] = 0.0;

	if ((kern = gettable(sft->font, "kern")) < 0)
		return 0;
	offset = kern;

	/* Read kern table header. */
	if (sft->font->size < offset + 4)
		return -1;
	if (getu16(sft->font, offset) != 0)
		return 0;
	numTables = getu16(sft->font, offset + 2);
	offset += 4;

	while (numTables > 0) {
		/* Read subtable header. */
		if (sft->font->size < offset + 6)
			return -1;
		length = getu16(sft->font, offset + 2);
		format = getu8 (sft->font, offset + 4);
		flags  = getu8 (sft->font, offset + 5);
		offset += 6;

		if (format == 0 && (flags & HORIZONTAL_KERNING) && !(flags & MINIMUM_KERNING)) {
			/* Read format 0 header. */
			if (sft->font->size < offset + 8)
				return -1;
			numPairs = getu16(sft->font, offset);
			offset += 8;
			/* Look up character code pair via binary search. */
			key[0] = (leftChar >> 8) & 0xFF;
			key[1] = leftChar & 0xFF;
			key[2] = (rightChar >> 8) & 0xFF;
			key[3] = rightChar & 0xFF;
			if ((match = bsearch(key, sft->font->memory + offset,
				numPairs, 6, cmpu32)) != NULL) {
				
				value = geti16(sft->font, (uint8_t *) match - sft->font->memory + 4);
				if (flags & CROSS_STREAM_KERNING) {
					kerning[1] += value;
				} else {
					kerning[0] += value;
				}
			}

		}

		offset += length;
		--numTables;
	}

	if ((unitsPerEm = units_per_em(sft->font)) < 0)
		return -1;
	kerning[0] = kerning[0] / unitsPerEm * sft->xScale;
	kerning[1] = kerning[1] / unitsPerEm * sft->yScale;

	return 0;
}

struct RenderCookie
{
	double xOrigin;
	double yOrigin;
	unsigned long outline;
};

static int
query_glyph(const struct SFT *sft, unsigned long glyph, struct SFT_Char *chr, struct RenderCookie *rc)
{
	double xScale, yScale, xOff, yOff;
	long glyph, outline;
	int unitsPerEm;
	int advance, leftSideBearing;
	int x1, y1, x2, y2;

	memset(chr, 0, sizeof(*chr));

	/* Set up the initial transformation from
	 * glyph coordinate space to SFT coordinate space. */
	if ((unitsPerEm = units_per_em(sft->font)) < 0)
		return -1;
	xScale = sft->xScale / unitsPerEm;
	yScale = sft->yScale / unitsPerEm;
	xOff = sft->x;
	yOff = sft->y;

	if (hor_metrics(sft->font, glyph, &advance, &leftSideBearing) < 0)
		return -1;
	/* We can compute the advance width early because the scaling factors
	 * won't be changed. This is neccessary for glyphs with completely
	 * empty outlines. */
	chr->advance = (int) round(advance * xScale);

	if ((outline = outline_offset(sft->font, glyph)) < 0)
		return -1;
	rc->outline = outline;
	/* A glyph may have a completely empty outline. */
	if (!outline)
		return 0;

	/* Read the bounding box from the font file verbatim. */
	if (sft->font->size < (unsigned long) outline + 10)
		return -1;
	x1 = geti16(sft->font, outline + 2);
	y1 = geti16(sft->font, outline + 4);
	x2 = geti16(sft->font, outline + 6);
	y2 = geti16(sft->font, outline + 8);
	if (x2 <= x1 || y2 <= y1)
		return -1;

	/* Shift the transformation along the X axis such that
	 * x1 and leftSideBearing line up. Derivation:
	 *     lsb * xScale + xOff_1 = x1 * xScale + xOff_2
	 * <=> lsb * xScale + xOff_1 - x1 * xScale = xOff_2
	 * <=> (lsb - x1) * xScale + xOff_1 = xOff_2 */
	rc->xShift = (leftSideBearing - x1) * xScale;
	xOff += rc->xShift;

	/* Transform the bounding box into SFT coordinate space. */
	x1 = (int) floor(x1 * xScale + xOff);
	y1 = (int) floor(y1 * yScale + yOff);
	x2 = (int) ceil(x2 * xScale + xOff) + 1;
	y2 = (int) ceil(y2 * yScale + yOff) + 1;

	rc->xOrigin = x1 - xShift;
	rc->yOrigin = y1;

	/* Compute the user-facing bounding box, respecting Y direction etc. */
	chr->x = x1;
	chr->y = sft->flags & SFT_DOWNWARD_Y ? -y2 : y1;
	chr->width = x2 - x1;
	chr->height = y2 - y1;

	return 0;
}

static int
render_glyph(const struct SFT *sft, struct SFT_Char *chr, const struct RenderCookie *rc)
{
	double transform[6];
	struct outline outl;
	struct buffer buf;
	int unitsPerEm;

	/* Set up the transformation matrix such that
	 * the transformed bounding boxes min corner lines
	 * up with the (0, 0) point. */
	if ((unitsPerEm = units_per_em(sft->font)) < 0)
		return -1;
	transform[0] = sft->xScale / unitsPerEm;
	transform[1] = 0.0;
	transform[2] = 0.0;
	transform[3] = sft->yScale / unitsPerEm;
	transform[4] = sft->x - rc->xOrigin;
	transform[5] = sft->y - rc->yOrigin;

	/* TTF --> outline */
	if (init_outline(&outl) < 0)
		return -1;
	if (decode_outline(sft->font, rc->outline, 0, &outl) < 0)
		goto fail_with_outl;

	/* outline processing */
	transform_points(outl.numPoints, outl.points, transform);
	clip_points(outl.numPoints, outl.points, chr->width, chr->height);
	if (tesselate_curves(&outl) < 0)
		goto fail_with_outl;
	
	/* outline --> buffer */
	if (init_buffer(&buf, chr->width, chr->height) < 0)
		goto fail_with_outl;
	draw_lines(&outl, buf);
	free_outline(&outl);
	if (sft->flags & SFT_DOWNWARD_Y) flip_buffer(&buf);
	
	/* buffer --> image */
	if ((chr->image = calloc(chr->width * chr->height, 1)) == NULL)
		goto fail_with_buf;
	post_process(buf, chr->image);
	free_buffer(&buf);

	return 0;

fail_with_outl:
	free_outline(&outl);
	return -1;

fail_with_buf:
	free_buffer(&buf);
	return -1;
}

int
sft_char(const struct SFT *sft, unsigned long charCode, struct SFT_Char *chr)
{
	double xScale, yScale, xOff, yOff;
	long glyph, outline;
	int unitsPerEm;
	int advance, leftSideBearing;
	int x1, y1, x2, y2;

	double transform[6];
	struct outline outl;
	struct buffer buf;
	void *image;
	int width, height;

	memset(chr, 0, sizeof(*chr));

	if ((glyph = glyph_id(sft->font, charCode)) < 0)
		return -1;
	if (glyph == 0 && (sft->flags & SFT_CATCH_MISSING))
		return 1;
	if ((outline = outline_offset(sft->font, glyph)) < 0)
		return -1;

	/* Set up the initial transformation from
	 * glyph coordinate space to SFT coordinate space. */
	if ((unitsPerEm = units_per_em(sft->font)) < 0)
		return -1;
	xScale = sft->xScale / unitsPerEm;
	yScale = sft->yScale / unitsPerEm;
	xOff = sft->x;
	yOff = sft->y;

	if (hor_metrics(sft->font, glyph, &advance, &leftSideBearing) < 0)
		return -1;
	/* We can compute the advance width early because the scaling factors
	 * won't be changed. This is neccessary for glyphs with completely
	 * empty outlines. */
	chr->advance = (int) round(advance * xScale);

	/* A glyph may have a completely empty outline. */
	if (!outline)
		return 0;

	/* Read the bounding box from the font file verbatim. */
	if (sft->font->size < (unsigned long) outline + 10)
		return -1;
	x1 = geti16(sft->font, outline + 2);
	y1 = geti16(sft->font, outline + 4);
	x2 = geti16(sft->font, outline + 6);
	y2 = geti16(sft->font, outline + 8);
	if (x2 <= x1 || y2 <= y1)
		return -1;

	/* Shift the transformation along the X axis such that
	 * x1 and leftSideBearing line up. Derivation:
	 *     lsb * xScale + xOff_1 = x1 * xScale + xOff_2
	 * <=> lsb * xScale + xOff_1 - x1 * xScale = xOff_2
	 * <=> (lsb - x1) * xScale + xOff_1 = xOff_2 */
	xOff += (leftSideBearing - x1) * xScale;

	/* Transform the bounding box into SFT coordinate space. */
	x1 = (int) floor(x1 * xScale + xOff);
	y1 = (int) floor(y1 * yScale + yOff);
	x2 = (int) ceil(x2 * xScale + xOff) + 1;
	y2 = (int) ceil(y2 * yScale + yOff) + 1;

	/* Compute the user-facing bounding box, respecting Y direction etc. */
	chr->x = x1;
	chr->y = sft->flags & SFT_DOWNWARD_Y ? -y2 : y1;
	chr->width = x2 - x1;
	chr->height = y2 - y1;

	/* Render the outline (if requested). */
	if (sft->flags & SFT_RENDER_IMAGE) {
		/* Set up the transformation matrix such that
		 * the transformed bounding boxes min corner lines
		 * up with the (0, 0) point. */
		transform[0] = xScale;
		transform[1] = 0.0;
		transform[2] = 0.0;
		transform[3] = yScale;
		transform[4] = xOff - x1;
		transform[5] = yOff - y1;

		width = x2 - x1;
		height = y2 - y1;

		if (init_outline(&outl) < 0)
			return -1;
		if (decode_outline(sft->font, outline, 0, &outl) < 0)
			goto fail_with_outl;

		transform_points(outl.numPoints, outl.points, transform);
		clip_points(outl.numPoints, outl.points, width, height);

		if (tesselate_curves(&outl) < 0)
			goto fail_with_outl;

		if (init_buffer(&buf, width, height) < 0)
			goto fail_with_outl;
		draw_lines(&outl, buf);
		free_outline(&outl);
		if (sft->flags & SFT_DOWNWARD_Y) flip_buffer(&buf);

		if ((image = calloc(width * height, 1)) == NULL)
			goto fail_with_buf;
		post_process(buf, image);
		free_buffer(&buf);

		chr->image = image;
	}

	return glyph == 0;

fail_with_outl:
	free_outline(&outl);
	return -1;
fail_with_buf:
	free_buffer(&buf);
	return -1;
}

int
sft_querychar(const struct SFT *sft, unsigned long charCode, struct SFT_Char *chr)
{
	struct DrawCookie dc;
	if (query_char(sft, charCode, &chr, &dc) < 0)
		return -1;
	return glyph == 0;
}

int
sft_drawchar(const struct SFT *sft, unsigned long charCode, struct SFT_Char *chr)
{
	struct DrawCookie dc;
	if (query_char(sft, charCode, &chr, &dc) < 0)
		return -1;
	if ((chr->image = draw_char(sft, &dc)) == NULL)
		return -1;
	return glyph == 0;
}

int
sft_drawknchar(const struct SFT *sft, unsigned long charCode, struct SFT_Char *chr)
{
	struct DrawCookie dc;
	if (query_char(&chr, &dc) < 0)
		return -1;
	if (glyph == 0) return 1;
	if ((chr->image = draw_char(&dc)) == NULL)
		return -1;
	return 0;
}

static int
map_file(SFT_Font *font, const char *filename)
{
	struct stat info;
	int fd;
	font->memory = MAP_FAILED;
	font->size = 0;
	font->source = SrcMapping;
	if ((fd = open(filename, O_RDONLY)) < 0) {
		return -1;
	}
	if (fstat(fd, &info) < 0) {
		close(fd);
		return -1;
	}
	font->memory = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	font->size = info.st_size;
	close(fd);
	return font->memory == MAP_FAILED ? -1 : 0;
}

static void
unmap_file(SFT_Font *font)
{
	assert(font->memory != MAP_FAILED);
	munmap((void *) font->memory, font->size);
}

static struct point
midpoint(struct point a, struct point b)
{
	return (struct point) {
		0.5 * a.x + 0.5 * b.x,
		0.5 * a.y + 0.5 * b.y
	};
}

/* Applies an affine linear transformation matrix to a set of points. */
static void
transform_points(int numPts, struct point *points, double trf[6])
{
	struct point *restrict pt;
	int i;
	for (i = 0; i < numPts; ++i) {
		pt = &points[i];
		*pt = (struct point) {
			pt->x * trf[0] + pt->y * trf[2] + trf[4],
			pt->x * trf[1] + pt->y * trf[3] + trf[5]
		};
	}
}

static void
clip_points(int numPts, struct point *points, int width, int height)
{
	struct point pt;
	double dv;
	uint64_t *ip;
	int i;

	for (i = 0; i < numPts; ++i) {
		pt = points[i];

		if (pt.x < 0.0) {
			/* printf("negative x\n"); */
			points[i].x = 0.0;
		}
		if (pt.x >= width) {
			/* printf("too large x\n"); */
			dv = width;
			ip = (void *) &dv;
			--*ip;
			points[i].x = dv;
		}
		if (pt.y < 0.0) {
			/* printf("negative y\n"); */
			points[i].y = 0.0;
		}
		if (pt.y >= height) {
			/* printf("too large y\n"); */
			dv = height;
			ip = (void *) &dv;
			--*ip;
			points[i].y = dv;
		}
	}
}

static int
init_buffer(struct buffer *buf, int width, int height)
{
	struct cell *ptr;
	size_t rowsSize, cellsSize;
	int i;

	buf->rows = NULL;
	buf->width = width;
	buf->height = height;

	rowsSize = (size_t) height * sizeof(buf->rows[0]);
	cellsSize = (size_t) width * height * sizeof(struct cell);
	if ((buf->rows = calloc(rowsSize + cellsSize, 1)) == NULL)
		return -1;

	ptr = (void *) (buf->rows + height);
	for (i = 0; i < height; ++i) {
		buf->rows[i] = ptr;
		ptr += width;
	}

	return 0;
}

static void
free_buffer(struct buffer *buf)
{
	free(buf->rows);
}

static void
flip_buffer(struct buffer *buf)
{
	struct cell *row;
	int front = 0, back = buf->height - 1;
	while (front < back) {
		row = buf->rows[front];
		buf->rows[front] = buf->rows[back];
		buf->rows[back] = row;
		++front, --back;
	}
}

static int
init_outline(struct outline *outl)
{
	outl->numPoints = 0;
	outl->capPoints = 64;
	if ((outl->points = malloc(outl->capPoints * sizeof(outl->points[0]))) == NULL)
		return -1;
	outl->numCurves = 0;
	outl->capCurves = 64;
	if ((outl->curves = malloc(outl->capCurves * sizeof(outl->curves[0]))) == NULL)
		return -1;
	outl->numLines = 0;
	outl->capLines = 64;
	if ((outl->lines = malloc(outl->capLines * sizeof(outl->lines[0]))) == NULL)
		return -1;
	return 0;
}

static void
free_outline(struct outline *outl)
{
	free(outl->points);
	free(outl->curves);
	free(outl->lines);
}

static int
grow_points(struct outline *outl)
{
	void *mem;
	int cap = outl->capPoints * 2;
	if ((mem = realloc(outl->points, cap * sizeof(outl->points[0]))) == NULL)
		return -1;
	outl->capPoints = cap;
	outl->points = mem;
	return 0;
}

static int
grow_curves(struct outline *outl)
{
	void *mem;
	int cap = outl->capCurves * 2;
	if ((mem = realloc(outl->curves, cap * sizeof(outl->curves[0]))) == NULL)
		return -1;
	outl->capCurves = cap;
	outl->curves = mem;
	return 0;
}

static int
grow_lines(struct outline *outl)
{
	void *mem;
	int cap = outl->capLines * 2;
	if ((mem = realloc(outl->lines, cap * sizeof(outl->lines[0]))) == NULL)
		return -1;
	outl->capLines = cap;
	outl->lines = mem;
	return 0;
}

/* Like bsearch(), but returns the next highest element if key could not be found. */
static void *
csearch(const void *key, const void *base,
	size_t nmemb, size_t size,
	int (*compar)(const void *, const void *))
{
	const uint8_t *bytes = base, *sample;
	size_t low = 0, high = nmemb - 1, mid;
	if (nmemb == 0)
		return NULL;
	while (low != high) {
		mid = low + (high - low) / 2;
		sample = bytes + mid * size;
		if (compar(key, sample) > 0) {
			low = mid + 1;
		} else {
			high = mid;
		}
	}
	return (uint8_t *) bytes + low * size;
}

/* Used as a comparison function for [bc]search(). */
static int
cmpu16(const void *a, const void *b)
{
	return memcmp(a, b, 2);
}

/* Used as a comparison function for [bc]search(). */
static int
cmpu32(const void *a, const void *b)
{
	return memcmp(a, b, 4);
}

static inline uint8_t
getu8(SFT_Font *font, unsigned long offset)
{
	assert(offset + 1 <= font->size);
	return *(font->memory + offset);
}

static inline int8_t
geti8(SFT_Font *font, unsigned long offset)
{
	return (int8_t) getu8(font, offset);
}

static inline uint16_t
getu16(SFT_Font *font, unsigned long offset)
{
	assert(offset + 2 <= font->size);
	const uint8_t *base = font->memory + offset;
	uint16_t b1 = base[0], b0 = base[1]; 
	return (uint16_t) (b1 << 8 | b0);
}

static inline int16_t
geti16(SFT_Font *font, unsigned long offset)
{
	return (int16_t) getu16(font, offset);
}

static inline uint32_t
getu32(SFT_Font *font, unsigned long offset)
{
	assert(offset + 4 <= font->size);
	const uint8_t *base = font->memory + offset;
	uint32_t b3 = base[0], b2 = base[1], b1 = base[2], b0 = base[3]; 
	return (uint32_t) (b3 << 24 | b2 << 16 | b1 << 8 | b0);
}

static long
gettable(SFT_Font *font, char tag[4])
{
	void *match;
	unsigned int numTables;
	if (font->size < 12)
		return -1;
	numTables = getu16(font, 4);
	if (font->size < 12 + (size_t) numTables * 16)
		return -1;
	if ((match = bsearch(tag, font->memory + 12, numTables, 16, cmpu32)) == NULL)
		return -1;
	return getu32(font, (uint8_t *) match - font->memory + 8);
}

static int
units_per_em(SFT_Font *font)
{
	long head;
	if ((head = gettable(font, "head")) < 0)
		return -1;
	if (font->size < (unsigned long) head + 54)
		return -1;
	return getu16(font, head + 18);
}

static long
cmap_fmt4(SFT_Font *font, unsigned long table, unsigned long charCode)
{
	unsigned long endCodes, startCodes, idDeltas, idRangeOffsets, idOffset;
	unsigned int segCountX2, segIdxX2, startCode, idRangeOffset, id;
	int idDelta;
	uint8_t key[2] = { charCode >> 8, charCode };
	/* cmap format 4 only supports the Unicode BMP. */
	if (charCode > 0xFFFF)
		return 0;
	if (font->size < table + 8)
		return -1;
	segCountX2 = getu16(font, table);
	if ((segCountX2 & 1) || !segCountX2)
		return -1;
	/* Find starting positions of the relevant arrays. */
	endCodes = table + 8;
	startCodes = endCodes + segCountX2 + 2;
	idDeltas = startCodes + segCountX2;
	idRangeOffsets = idDeltas + segCountX2;
	if (font->size < idRangeOffsets + segCountX2)
		return -1;
	/* Find the segment that contains charCode by binary searching over the highest codes in the segments. */
	segIdxX2 = (uintptr_t) csearch(key, font->memory + endCodes,
		segCountX2 / 2, 2, cmpu16) - (uintptr_t) (font->memory + endCodes);
	/* Look up segment info from the arrays & short circuit if the spec requires. */
	if ((startCode = getu16(font, startCodes + segIdxX2)) > charCode)
		return 0;
	idDelta = geti16(font, idDeltas + segIdxX2);
	if (!(idRangeOffset = getu16(font, idRangeOffsets + segIdxX2)))
		return (charCode + idDelta) & 0xFFFF;
	/* Calculate offset into glyph array and determine ultimate value. */
	idOffset = idRangeOffsets + segIdxX2 + idRangeOffset + 2 * (charCode - startCode);
	if (font->size < idOffset + 2)
		return -1;
	id = getu16(font, idOffset);
	return id ? (id + idDelta) & 0xFFFF : 0L;
}

static long
cmap_fmt6(SFT_Font *font, unsigned long table, unsigned long charCode)
{
	unsigned int firstCode, entryCount;
	/* cmap format 6 only supports the Unicode BMP. */
	if (charCode > 0xFFFF)
		return 0;
	if (font->size < table + 4)
		return -1;
	firstCode = getu16(font, table);
	entryCount = getu16(font, table + 2);
	if (font->size < table + 4 + 2 * entryCount)
		return -1;
	if (charCode < firstCode)
		return -1;
	charCode -= firstCode;
	if (!(charCode < entryCount))
		return -1;
	return getu16(font, table + 4 + 2 * charCode);
}

/* Maps Unicode code points to glyph indices. */
static long
glyph_id(SFT_Font *font, unsigned long charCode)
{
	unsigned long entry, table;
	long cmap;
	unsigned int idx, numEntries;
	int type;

	if ((cmap = gettable(font, "cmap")) < 0)
		return -1;

	if (font->size < (unsigned long) cmap + 4)
		return -1;
	numEntries = getu16(font, cmap + 2);
	
	if (font->size < (unsigned long) cmap + 4 + numEntries * 8)
		return -1;
	/* Search for the first Unicode BMP entry. */
	for (idx = 0; idx < numEntries; ++idx) {
		entry = cmap + 4 + idx * 8;
		type = getu16(font, entry) * 0100 + getu16(font, entry + 2);
		if (type == 0003 || type == 0301) {
			table = cmap + getu32(font, entry + 4);
			if (font->size < table + 6)
				return -1;
			/* Dispatch based on cmap format. */
			switch (getu16(font, table)) {
			case 4:
				return cmap_fmt4(font, table + 6, charCode);
			case 6:
				return cmap_fmt6(font, table + 6, charCode);
			default:
				return -1;
			}
		}
	}

	return -1;
}

static int
num_long_hmtx(SFT_Font *font)
{
	long hhea;
	if ((hhea = gettable(font, "hhea")) < 0)
		return -1;
	if (font->size < (unsigned long) hhea + 36)
		return -1;
	return getu16(font, hhea + 34);
}

static int
hor_metrics(SFT_Font *font, long glyph, int *advanceWidth, int *leftSideBearing)
{
	unsigned long offset, boundary;
	long hmtx;
	int numLong;
	if ((numLong = num_long_hmtx(font)) < 0)
		return -1;
	if ((hmtx = gettable(font, "hmtx")) < 0)
		return -1;
	if (glyph < numLong) {
		/* glyph is inside long metrics segment. */
		offset = hmtx + 4 * glyph;
		if (font->size < offset + 4)
			return -1;
		*advanceWidth = getu16(font, offset);
		*leftSideBearing = geti16(font, offset + 2);
		return 0;
	} else {
		/* glyph is inside short metrics segment. */
		boundary = hmtx + 4 * numLong;
		if (boundary < 4)
			return -1;
		
		offset = boundary - 4;
		if (font->size < offset + 4)
			return -1;
		*advanceWidth = getu16(font, offset);
		
		offset = boundary + 2 * (glyph - numLong);
		if (font->size < offset + 2)
			return -1;
		*leftSideBearing = geti16(font, offset);
		return 0;
	}
}

/* Returns the offset into the font that the glyph's outline is stored at. */
static long
outline_offset(SFT_Font *font, long glyph)
{
	unsigned long base, this, next;
	long head, loca, glyf;
	int format;

	if ((head = gettable(font, "head")) < 0)
		return -1;
	if ((loca = gettable(font, "loca")) < 0)
		return -1;
	if ((glyf = gettable(font, "glyf")) < 0)
		return -1;

	if (font->size < (unsigned long) head + 54)
		return -1;
	format = geti16(font, head + 50);
	
	if (format == 0) {
		base = loca + 2 * glyph;

		if (font->size < base + 4)
			return -1;
		
		this = 2L * getu16(font, base);
		next = 2L * getu16(font, base + 2);
	} else {
		base = loca + 4 * glyph;

		if (font->size < base + 8)
			return -1;

		this = getu32(font, base);
		next = getu32(font, base + 4);
	}

	return this == next ? 0 : glyf + this;
}

/* For a 'simple' outline, determines each point of the outline with a set of flags. */
static long
simple_flags(SFT_Font *font, unsigned long offset, int numPts, uint8_t *flags)
{
	int value = 0, repeat = 0, i;
	for (i = 0; i < numPts; ++i) {
		if (repeat) {
			--repeat;
		} else {
			if (font->size < offset + 1)
				return -1;
			value = getu8(font, offset++);
			if (value & REPEAT_FLAG) {
				if (font->size < offset + 1)
					return -1;
				repeat = getu8(font, offset++);
			}
		}
		flags[i] = value;
	}
	return offset;
}

/* For a 'simple' outline, decodes both X and Y coordinates for each point of the outline. */
static int
simple_points(SFT_Font *font, unsigned long offset, int numPts, uint8_t *flags, struct point *points)
{
	long accum, value, bit;
	int i;

	assert(numPts > 0);

	accum = 0L;
	for (i = 0; i < numPts; ++i) {
		if (flags[i] & X_CHANGE_IS_SMALL) {
			if (font->size < offset + 1)
				return -1;
			value = (long) getu8(font, offset++);
			bit = !!(flags[i] & X_CHANGE_IS_POSITIVE);
			accum -= (value ^ -bit) + bit;
		} else if (!(flags[i] & X_CHANGE_IS_ZERO)) {
			if (font->size < offset + 2)
				return -1;
			accum += geti16(font, offset);
			offset += 2;
		}
		points[i].x = accum;
	}

	accum = 0L;
	for (i = 0; i < numPts; ++i) {
		if (flags[i] & Y_CHANGE_IS_SMALL) {
			if (font->size < offset + 1)
				return -1;
			value = (long) getu8(font, offset++);
			bit = !!(flags[i] & Y_CHANGE_IS_POSITIVE);
			accum -= (value ^ -bit) + bit;
		} else if (!(flags[i] & Y_CHANGE_IS_ZERO)) {
			if (font->size < offset + 2)
				return -1;
			accum += geti16(font, offset);
			offset += 2;
		}
		points[i].y = accum;
	}

	return 0;
}

static int
decode_contour(uint8_t *flags, unsigned int basePoint, unsigned int count, struct outline *outl)
{
	unsigned int looseEnd, beg, ctrl, center;
	unsigned int gotCtrl, i;

	/* Skip contours with less than two points, since the following algorithm can't handle them and
	 * they should appear invisible either way (because they don't have any area). */
	if (count < 2) return 0;

	if (flags[0] & POINT_IS_ON_CURVE) {
		looseEnd = basePoint++;
		++flags;
		--count;
	} else if (flags[count - 1] & POINT_IS_ON_CURVE) {
		looseEnd = basePoint + --count;
	} else {
		if (outl->numPoints >= outl->capPoints && grow_points(outl) < 0)
			return -1;

		looseEnd = outl->numPoints;
		outl->points[outl->numPoints++] = midpoint(
			outl->points[basePoint],
			outl->points[basePoint + count - 1]);
	}
	beg = looseEnd;
	gotCtrl = 0;
	for (i = 0; i < count; ++i) {
		if (flags[i] & POINT_IS_ON_CURVE) {
			if (gotCtrl) {
				if (outl->numCurves >= outl->capCurves && grow_curves(outl) < 0)
					return -1;
				outl->curves[outl->numCurves++] = (struct curve) { beg, basePoint + i, ctrl };
			} else {
				if (outl->numLines >= outl->capLines && grow_lines(outl) < 0)
					return -1;
				outl->lines[outl->numLines++] = (struct line) { beg, basePoint + i };
			}
			beg = basePoint + i;
			gotCtrl = 0;
		} else {
			if (gotCtrl) {
				center = outl->numPoints;
				if (outl->numPoints >= outl->capPoints && grow_points(outl) < 0)
					return -1;
				outl->points[center] = midpoint(outl->points[ctrl], outl->points[basePoint + i]);
				++outl->numPoints;

				if (outl->numCurves >= outl->capCurves && grow_curves(outl) < 0)
					return -1;
				outl->curves[outl->numCurves++] = (struct curve) { beg, center, ctrl };

				beg = center;
			}
			ctrl = basePoint + i;
			gotCtrl = 1;
		}
	}
	if (gotCtrl) {
		if (outl->numCurves >= outl->capCurves && grow_curves(outl) < 0)
			return -1;
		outl->curves[outl->numCurves++] = (struct curve) { beg, looseEnd, ctrl };
	} else {
		if (outl->numLines >= outl->capLines && grow_lines(outl) < 0)
			return -1;
		outl->lines[outl->numLines++] = (struct line) { beg, looseEnd };
	}

	return 0;
}

static int
simple_outline(SFT_Font *font, unsigned long offset, int numContours, struct outline *outl)
{
	unsigned int *endPts;
	uint8_t *memory = NULL, *flags;
	unsigned long memLen;
	long sgnOffset;
	unsigned int numPts;
	int i;

	unsigned int basePoint = outl->numPoints;

	if (font->size < offset + numContours * 2 + 2)
		goto failure;
	numPts = getu16(font, offset + (numContours - 1) * 2) + 1;

	while (outl->capPoints < basePoint + numPts) {
		if (grow_points(outl) < 0)
			return -1;
	}
	
	memLen = numContours * sizeof(endPts[0]);
	memLen += numPts * sizeof(flags[0]);
	STACK_ALLOC(memory, memLen, 2048) ;
	if (memory == NULL)
		goto failure;
	endPts = (unsigned int *) memory;
	flags = (uint8_t *) (endPts + numContours);

	for (i = 0; i < numContours; ++i) {
		endPts[i] = getu16(font, offset);
		offset += 2;
	}
	/* Ensure that endPts are never falling.
	 * Falling endPts have no sensible interpretation and most likely only occur in malicious input.
	 * Therefore, we bail, should we ever encounter such input. */
	for (i = 0; i < numContours - 1; ++i) {
		if (endPts[i + 1] < endPts[i] + 1)
			goto failure;
	}
	offset += 2 + getu16(font, offset);

	if ((sgnOffset = simple_flags(font, offset, numPts, flags)) < 0)
		goto failure;
	offset = sgnOffset;
	if (simple_points(font, offset, numPts, flags, outl->points + basePoint) < 0)
		goto failure;
	outl->numPoints += numPts;
	
	unsigned int contourBase = 0;
	for (int c = 0; c < numContours; ++c) {
		unsigned int count = endPts[c] - contourBase + 1;
		if (decode_contour(flags, basePoint, count, outl) < 0)
			goto failure;
		flags += count;
		basePoint += count;
		contourBase += count;
	}

	STACK_FREE(memory) ;
	return 0;
failure:
	STACK_FREE(memory) ;
	return -1;
}

static int
compound_outline(SFT_Font *font, unsigned long offset, int recDepth, struct outline *outl)
{
	double local[6];
	long outline;
	unsigned int flags, glyph;
	/* Guard against infinite recursion (compound glyphs that have themselves as component). */
	if (recDepth >= 4)
		return -1;
	do {
		memset(local, 0, sizeof(local));
		if (font->size < offset + 4)
			return -1;
		flags = getu16(font, offset);
		glyph = getu16(font, offset + 2);
		offset += 4;
		/* We don't implement point matching, and neither does stb_truetype for that matter. */
		if (!(flags & ACTUAL_XY_OFFSETS))
			return -1;
		/* Read additional X and Y offsets (in FUnits) of this component. */
		if (flags & OFFSETS_ARE_LARGE) {
			if (font->size < offset + 4)
				return -1;
			local[4] = geti16(font, offset);
			local[5] = geti16(font, offset + 2);
			offset += 4;
		} else {
			if (font->size < offset + 2)
				return -1;
			local[4] = geti8(font, offset);
			local[5] = geti8(font, offset + 1);
			offset += 2;
		}
		if (flags & GOT_A_SINGLE_SCALE) {
			if (font->size < offset + 2)
				return -1;
			local[0] = geti16(font, offset) / 16384.0;
			local[3] = local[0];
			offset += 2;
		} else if (flags & GOT_AN_X_AND_Y_SCALE) {
			if (font->size < offset + 4)
				return -1;
			local[0] = geti16(font, offset + 0) / 16384.0;
			local[3] = geti16(font, offset + 2) / 16384.0;
			offset += 4;
		} else if (flags & GOT_A_SCALE_MATRIX) {
			if (font->size < offset + 8)
				return -1;
			local[0] = geti16(font, offset + 0) / 16384.0;
			local[1] = geti16(font, offset + 2) / 16384.0;
			local[2] = geti16(font, offset + 4) / 16384.0;
			local[3] = geti16(font, offset + 6) / 16384.0;
			offset += 8;
		} else {
			local[0] = 1.0;
			local[3] = 1.0;
		}
		/* At this point, Apple's spec more or less tells you to scale the matrix by its own L1 norm.
		 * But stb_truetype scales by the L2 norm. And FreeType2 doesn't scale at all.
		 * Furthermore, Microsoft's spec doesn't even mention anything like this.
		 * It's almost as if nobody ever uses this feature anyway. */
		if ((outline = outline_offset(font, glyph)) < 0)
			return -1;
		if (outline) {
			unsigned int basePoint = outl->numPoints;
			if (decode_outline(font, outline, recDepth + 1, outl) < 0)
				return -1;
			transform_points(outl->numPoints - basePoint, outl->points + basePoint, local);
		}
	} while (flags & THERE_ARE_MORE_COMPONENTS);

	return 0;
}

static int
decode_outline(SFT_Font *font, unsigned long offset, int recDepth, struct outline *outl)
{
	int numContours;
	if (font->size < offset + 10)
		return -1;
	numContours = geti16(font, offset);
	if (numContours >= 0) {
		/* Glyph has a 'simple' outline consisting of a number of contours. */
		return simple_outline(font, offset + 10, numContours, outl);
	} else {
		/* Glyph has a compound outline combined from mutiple other outlines. */
		return compound_outline(font, offset + 10, recDepth, outl);
	}
}

/* A heuristic to tell whether a given curve can be approximated closely enough by a line. */
static int
is_flat(struct outline *outl, struct curve curve, double flatness)
{
	struct point beg = outl->points[curve.beg];
	struct point end = outl->points[curve.end];
	struct point ctrl = outl->points[curve.ctrl];
	struct point mid = midpoint(beg, end);
	double x = ctrl.x - mid.x;
	double y = ctrl.y - mid.y;
	return x * x + y * y <= flatness * flatness;
}

static int
tesselate_curve(struct curve curve, struct outline *outl)
{
	/* From my tests I can conclude that this stack barely reaches a top height
	 * of 4 elements even for the largest font sizes I'm willing to support. And
	 * as space requirements should only grow logarithmically, I think 10 is
	 * more than enough. */
#define STACK_SIZE 10
	struct curve stack[STACK_SIZE];
	unsigned int top = 0;
	for (;;) {
		if (is_flat(outl, curve, 0.5) || top >= STACK_SIZE) {
			if (outl->numLines >= outl->capLines && grow_lines(outl) < 0)
				return -1;
			outl->lines[outl->numLines++] = (struct line) { curve.beg, curve.end };
			if (top == 0) break;
			curve = stack[--top];
		} else {
			unsigned int ctrl0 = outl->numPoints;
			if (outl->numPoints >= outl->capPoints && grow_points(outl) < 0)
				return -1;
			outl->points[ctrl0] = midpoint(outl->points[curve.beg], outl->points[curve.ctrl]);
			++outl->numPoints;

			unsigned int ctrl1 = outl->numPoints;
			if (outl->numPoints >= outl->capPoints && grow_points(outl) < 0)
				return -1;
			outl->points[ctrl1] = midpoint(outl->points[curve.ctrl], outl->points[curve.end]);
			++outl->numPoints;

			unsigned int pivot = outl->numPoints;
			if (outl->numPoints >= outl->capPoints && grow_points(outl) < 0)
				return -1;
			outl->points[pivot] = midpoint(outl->points[ctrl0], outl->points[ctrl1]);
			++outl->numPoints;

			stack[top++] = (struct curve) { curve.beg, pivot, ctrl0 };
			curve = (struct curve) { pivot, curve.end, ctrl1 };
		}
	}
	return 0;
#undef STACK_SIZE
}

static int
tesselate_curves(struct outline *outl)
{
	unsigned int i;
	for (i = 0; i < outl->numCurves; ++i) {
		if (tesselate_curve(outl->curves[i], outl) < 0)
			return -1;
	}
	return 0;
}

static void
draw_dot(struct buffer buf, int px, int py, double xAvg, double yDiff)
{
	struct cell *restrict ptr = &buf.rows[py][px];
	struct cell cell = *ptr;
	cell.cover += yDiff;
	cell.area += (1.0 - xAvg) * yDiff;
	*ptr = cell;
}

/* Draws a line into the buffer. Uses a custom 2D raycasting algorithm to do so. */
static void
draw_line(struct buffer buf, struct point origin, struct point goal)
{
	double originX, originY;
	double goalX, goalY;
	double deltaX, deltaY;
	double nextCrossingX = 100.0, nextCrossingY = 100.0;
	double crossingGapX = 0.0, crossingGapY = 0.0;
	double prevDistance = 0.0;
	int pixelX, pixelY;
	int iter, numIters;

	originX = origin.x;
	goalX = goal.x;
	deltaX = goalX - originX;
	pixelX = (int) originX;
	if (deltaX != 0.0) {
		double signedGapX = 1.0 / deltaX;
		nextCrossingX = (int) originX - originX;
		nextCrossingX += deltaX > 0.0;
		nextCrossingX *= signedGapX;
		crossingGapX = fabs(signedGapX);
	}

	originY = origin.y;
	goalY = goal.y;
	deltaY = goalY - originY;
	pixelY = (int) originY;
	if (deltaY != 0.0) {
		double signedGapY = 1.0 / deltaY;
		nextCrossingY = (int) originY - originY;
		nextCrossingY += deltaY > 0.0;
		nextCrossingY *= signedGapY;
		crossingGapY = fabs(signedGapY);
	}

	numIters = abs((int) goalX - (int) originX) + abs((int) goalY - (int) originY);
	for (iter = 0; iter < numIters; ++iter) {
		if (nextCrossingX < nextCrossingY) {
			double deltaDistance = nextCrossingX - prevDistance;
			double averageX = (deltaX > 0) - 0.5 * deltaX * deltaDistance;
			draw_dot(buf, pixelX, pixelY, averageX, deltaY * deltaDistance);
			pixelX += SIGN(deltaX);
			prevDistance = nextCrossingX;
			nextCrossingX += crossingGapX;
		} else {
			double deltaDistance = nextCrossingY - prevDistance;
			double x = originX - pixelX + nextCrossingY * deltaX;
			double averageX = x - 0.5 * deltaX * deltaDistance;
			draw_dot(buf, pixelX, pixelY, averageX, deltaY * deltaDistance);
			pixelY += SIGN(deltaY);
			prevDistance = nextCrossingY;
			nextCrossingY += crossingGapY;
		}
	}

	double deltaDistance = 1.0 - prevDistance;
	double averageX = (goalX - pixelX) - 0.5 * deltaX * deltaDistance;
	draw_dot(buf, pixelX, pixelY, averageX, deltaY * deltaDistance);
}

static void
draw_lines(struct outline *outl, struct buffer buf)
{
	unsigned int i;
	for (i = 0; i < outl->numLines; ++i) {
		struct line line = outl->lines[i];
		struct point origin = outl->points[line.beg];
		struct point goal = outl->points[line.end];
		if (origin.y != goal.y) {
			draw_line(buf, origin, goal);
		}
	}
}

/* Integrate the values in the buffer to arrive at the final grayscale image. */
static void
post_process(struct buffer buf, uint8_t *image)
{
	struct cell *restrict in, cell;
	uint8_t *restrict out;
	double accum, value;
	int x, y;
	out = image;
	for (y = 0; y < buf.height; ++y) {
		accum = 0.0;
		in = buf.rows[y];
		for (x = 0; x < buf.width; ++x) {
			cell = *in++;
			value = fabs(accum + cell.area);
			value = MIN(value, 1.0);
			value = value * 255.0 + 0.5;
			*out++ = (uint8_t) value;
			accum += cell.cover;
		}
	}
}

