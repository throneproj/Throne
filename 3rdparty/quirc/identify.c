/* quirc - QR-code recognition library
 * Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#ifdef QUIRC_USE_TGMATH
#include <tgmath.h>
#else
#include <math.h>
#endif // QUIRC_USE_TGMATH
#include "quirc_internal.h"

/************************************************************************
 * Linear algebra routines
 */

static int line_intersect(const struct quirc_point *p0,
			  const struct quirc_point *p1,
			  const struct quirc_point *q0,
			  const struct quirc_point *q1,
			  struct quirc_point *r)
{
	/* (a, b) is perpendicular to line p */
	int a = -(p1->y - p0->y);
	int b = p1->x - p0->x;

	/* (c, d) is perpendicular to line q */
	int c = -(q1->y - q0->y);
	int d = q1->x - q0->x;

	/* e and f are dot products of the respective vectors with p and q */
	int e = a * p1->x + b * p1->y;
	int f = c * q1->x + d * q1->y;

	/* Now we need to solve:
	 *     [a b] [rx]   [e]
	 *     [c d] [ry] = [f]
	 *
	 * We do this by inverting the matrix and applying it to (e, f):
	 *       [ d -b] [e]   [rx]
	 * 1/det [-c  a] [f] = [ry]
	 */
	int det = (a * d) - (b * c);

	if (!det)
		return 0;

	r->x = (d * e - b * f) / det;
	r->y = (-c * e + a * f) / det;

	return 1;
}

static void perspective_setup(quirc_float_t *c,
			      const struct quirc_point *rect,
			      quirc_float_t w, quirc_float_t h)
{
	quirc_float_t x0 = rect[0].x;
	quirc_float_t y0 = rect[0].y;
	quirc_float_t x1 = rect[1].x;
	quirc_float_t y1 = rect[1].y;
	quirc_float_t x2 = rect[2].x;
	quirc_float_t y2 = rect[2].y;
	quirc_float_t x3 = rect[3].x;
	quirc_float_t y3 = rect[3].y;

	quirc_float_t wden = w * (x2*y3 - x3*y2 + (x3-x2)*y1 + x1*(y2-y3));
	quirc_float_t hden = h * (x2*y3 + x1*(y2-y3) - x3*y2 + (x3-x2)*y1);

	c[0] = (x1*(x2*y3-x3*y2) + x0*(-x2*y3+x3*y2+(x2-x3)*y1) +
		x1*(x3-x2)*y0) / wden;
	c[1] = -(x0*(x2*y3+x1*(y2-y3)-x2*y1) - x1*x3*y2 + x2*x3*y1
		 + (x1*x3-x2*x3)*y0) / hden;
	c[2] = x0;
	c[3] = (y0*(x1*(y3-y2)-x2*y3+x3*y2) + y1*(x2*y3-x3*y2) +
		x0*y1*(y2-y3)) / wden;
	c[4] = (x0*(y1*y3-y2*y3) + x1*y2*y3 - x2*y1*y3 +
		y0*(x3*y2-x1*y2+(x2-x3)*y1)) / hden;
	c[5] = y0;
	c[6] = (x1*(y3-y2) + x0*(y2-y3) + (x2-x3)*y1 + (x3-x2)*y0) / wden;
	c[7] = (-x2*y3 + x1*y3 + x3*y2 + x0*(y1-y2) - x3*y1 + (x2-x1)*y0) /
		hden;
}

static void perspective_map(const quirc_float_t *c,
			    quirc_float_t u, quirc_float_t v, struct quirc_point *ret)
{
	quirc_float_t den = c[6]*u + c[7]*v + 1.0;
	quirc_float_t x = (c[0]*u + c[1]*v + c[2]) / den;
	quirc_float_t y = (c[3]*u + c[4]*v + c[5]) / den;

	ret->x = (int) rint(x);
	ret->y = (int) rint(y);
}

static void perspective_unmap(const quirc_float_t *c,
			      const struct quirc_point *in,
			      quirc_float_t *u, quirc_float_t *v)
{
	quirc_float_t x = in->x;
	quirc_float_t y = in->y;
	quirc_float_t den = -c[0]*c[7]*y + c[1]*c[6]*y + (c[3]*c[7]-c[4]*c[6])*x +
		c[0]*c[4] - c[1]*c[3];

	*u = -(c[1]*(y-c[5]) - c[2]*c[7]*y + (c[5]*c[7]-c[4])*x + c[2]*c[4]) /
		den;
	*v = (c[0]*(y-c[5]) - c[2]*c[6]*y + (c[5]*c[6]-c[3])*x + c[2]*c[3]) /
		den;
}

/************************************************************************
 * Span-based floodfill routine
 */

typedef void (*span_func_t)(void *user_data, int y, int left, int right);

static void flood_fill_line(struct quirc *q, int x, int y,
			    int from, int to,
			    span_func_t func, void *user_data,
			    int *leftp, int *rightp)
{
	quirc_pixel_t *row;
	int left;
	int right;
	int i;

	row = q->pixels + y * q->w;
	QUIRC_ASSERT(row[x] == from);

	left = x;
	right = x;

	while (left > 0 && row[left - 1] == from)
		left--;

	while (right < q->w - 1 && row[right + 1] == from)
		right++;

	/* Fill the extent */
	for (i = left; i <= right; i++)
		row[i] = to;

	/* Return the processed range */
	*leftp = left;
	*rightp = right;

	if (func)
		func(user_data, y, left, right);
}

static struct quirc_flood_fill_vars *flood_fill_call_next(
			struct quirc *q,
			quirc_pixel_t *row,
			int from, int to,
			span_func_t func, void *user_data,
			struct quirc_flood_fill_vars *vars,
			int direction)
{
	int *leftp;

	if (direction < 0) {
		leftp = &vars->left_up;
	} else {
		leftp = &vars->left_down;
	}

	while (*leftp <= vars->right) {
		if (row[*leftp] == from) {
			struct quirc_flood_fill_vars *next_vars;
			int next_left;

			/* Set up the next context */
			next_vars = vars + 1;
			next_vars->y = vars->y + direction;

			/* Fill the extent */
			flood_fill_line(q,
					*leftp,
					next_vars->y,
					from, to,
					func, user_data,
					&next_left,
					&next_vars->right);
			next_vars->left_down = next_left;
			next_vars->left_up = next_left;

			return next_vars;
		}
		(*leftp)++;
	}
	return NULL;
}

static void flood_fill_seed(struct quirc *q,
			    int x0, int y0,
			    int from, int to,
			    span_func_t func, void *user_data)
{
	struct quirc_flood_fill_vars *const stack = q->flood_fill_vars;
	const size_t stack_size = q->num_flood_fill_vars;
	const struct quirc_flood_fill_vars *const last_vars =
	    &stack[stack_size - 1];

	QUIRC_ASSERT(from != to);
	QUIRC_ASSERT(q->pixels[y0 * q->w + x0] == from);

	struct quirc_flood_fill_vars *next_vars;
	int next_left;

	/* Set up the first context  */
	next_vars = stack;
	next_vars->y = y0;

	/* Fill the extent */
	flood_fill_line(q, x0, next_vars->y, from, to,
			func, user_data,
			&next_left, &next_vars->right);
	next_vars->left_down = next_left;
	next_vars->left_up = next_left;

	while (true) {
		struct quirc_flood_fill_vars * const vars = next_vars;
		quirc_pixel_t *row;

		if (vars == last_vars) {
			/*
			 * "Stack overflow".
			 * Just stop and return.
			 * This can be caused by very complex shapes in
			 * the image, which is not likely a part of
			 * a valid QR code anyway.
			 */
			break;
		}

		/* Seed new flood-fills */
		if (vars->y > 0) {
			row = q->pixels + (vars->y - 1) * q->w;

			next_vars = flood_fill_call_next(q, row,
							 from, to,
							 func, user_data,
							 vars, -1);
			if (next_vars != NULL) {
				continue;
			}
		}

		if (vars->y < q->h - 1) {
			row = q->pixels + (vars->y + 1) * q->w;

			next_vars = flood_fill_call_next(q, row,
							 from, to,
							 func, user_data,
							 vars, 1);
			if (next_vars != NULL) {
				continue;
			}
		}

		if (vars > stack) {
			/* Restore the previous context */
			next_vars = vars - 1;
			continue;
		}

		/* We've done. */
		break;
	}
}

/************************************************************************
 * Adaptive thresholding
 */

static uint8_t otsu(const struct quirc *q)
{
	unsigned int numPixels = q->w * q->h;

	// Calculate histogram
	unsigned int histogram[UINT8_MAX + 1];
	(void)memset(histogram, 0, sizeof(histogram));
	uint8_t* ptr = q->image;
	unsigned int length = numPixels;
	while (length--) {
		uint8_t value = *ptr++;
		histogram[value]++;
	}

	// Calculate weighted sum of histogram values
	quirc_float_t sum = 0;
	unsigned int i = 0;
	for (i = 0; i <= UINT8_MAX; ++i) {
		sum += i * histogram[i];
	}

	// Compute threshold
	quirc_float_t sumB = 0;
	unsigned int q1 = 0;
	quirc_float_t max = 0;
	uint8_t threshold = 0;
	for (i = 0; i <= UINT8_MAX; ++i) {
		// Weighted background
		q1 += histogram[i];
		if (q1 == 0)
			continue;

		// Weighted foreground
		const unsigned int q2 = numPixels - q1;
		if (q2 == 0)
			break;

		sumB += i * histogram[i];
		const quirc_float_t m1 = sumB / q1;
		const quirc_float_t m2 = (sum - sumB) / q2;
		const quirc_float_t m1m2 = m1 - m2;
		const quirc_float_t variance = m1m2 * m1m2 * q1 * q2;
		if (variance >= max) {
			threshold = i;
			max = variance;
		}
	}

	return threshold;
}

static void area_count(void *user_data, int y, int left, int right)
{
	((struct quirc_region *)user_data)->count += right - left + 1;
}

static int region_code(struct quirc *q, int x, int y)
{
	int pixel;
	struct quirc_region *box;
	int region;

	if (x < 0 || y < 0 || x >= q->w || y >= q->h)
		return -1;

	pixel = q->pixels[y * q->w + x];

	if (pixel >= QUIRC_PIXEL_REGION)
		return pixel;

	if (pixel == QUIRC_PIXEL_WHITE)
		return -1;

	if (q->num_regions >= QUIRC_MAX_REGIONS)
		return -1;

	region = q->num_regions;
	box = &q->regions[q->num_regions++];

	memset(box, 0, sizeof(*box));

	box->seed.x = x;
	box->seed.y = y;
	box->capstone = -1;

	flood_fill_seed(q, x, y, pixel, region, area_count, box);

	return region;
}

struct polygon_score_data {
	struct quirc_point	ref;

	int			scores[4];
	struct quirc_point	*corners;
};

static void find_one_corner(void *user_data, int y, int left, int right)
{
	struct polygon_score_data *psd =
		(struct polygon_score_data *)user_data;
	int xs[2] = {left, right};
	int dy = y - psd->ref.y;
	int i;

	for (i = 0; i < 2; i++) {
		int dx = xs[i] - psd->ref.x;
		int d = dx * dx + dy * dy;

		if (d > psd->scores[0]) {
			psd->scores[0] = d;
			psd->corners[0].x = xs[i];
			psd->corners[0].y = y;
		}
	}
}

static void find_other_corners(void *user_data, int y, int left, int right)
{
	struct polygon_score_data *psd =
		(struct polygon_score_data *)user_data;
	int xs[2] = {left, right};
	int i;

	for (i = 0; i < 2; i++) {
		int up = xs[i] * psd->ref.x + y * psd->ref.y;
		int right = xs[i] * -psd->ref.y + y * psd->ref.x;
		int scores[4] = {up, right, -up, -right};
		int j;

		for (j = 0; j < 4; j++) {
			if (scores[j] > psd->scores[j]) {
				psd->scores[j] = scores[j];
				psd->corners[j].x = xs[i];
				psd->corners[j].y = y;
			}
		}
	}
}

static void find_region_corners(struct quirc *q,
				int rcode, const struct quirc_point *ref,
				struct quirc_point *corners)
{
	struct quirc_region *region = &q->regions[rcode];
	struct polygon_score_data psd;
	int i;

	memset(&psd, 0, sizeof(psd));
	psd.corners = corners;

	memcpy(&psd.ref, ref, sizeof(psd.ref));
	psd.scores[0] = -1;
	flood_fill_seed(q, region->seed.x, region->seed.y,
			rcode, QUIRC_PIXEL_BLACK,
			find_one_corner, &psd);

	psd.ref.x = psd.corners[0].x - psd.ref.x;
	psd.ref.y = psd.corners[0].y - psd.ref.y;

	for (i = 0; i < 4; i++)
		memcpy(&psd.corners[i], &region->seed,
		       sizeof(psd.corners[i]));

	i = region->seed.x * psd.ref.x + region->seed.y * psd.ref.y;
	psd.scores[0] = i;
	psd.scores[2] = -i;
	i = region->seed.x * -psd.ref.y + region->seed.y * psd.ref.x;
	psd.scores[1] = i;
	psd.scores[3] = -i;

	flood_fill_seed(q, region->seed.x, region->seed.y,
			QUIRC_PIXEL_BLACK, rcode,
			find_other_corners, &psd);
}

static void record_capstone(struct quirc *q, int ring, int stone)
{
	struct quirc_region *stone_reg = &q->regions[stone];
	struct quirc_region *ring_reg = &q->regions[ring];
	struct quirc_capstone *capstone;
	int cs_index;

	if (q->num_capstones >= QUIRC_MAX_CAPSTONES)
		return;

	cs_index = q->num_capstones;
	capstone = &q->capstones[q->num_capstones++];

	memset(capstone, 0, sizeof(*capstone));

	capstone->qr_grid = -1;
	capstone->ring = ring;
	capstone->stone = stone;
	stone_reg->capstone = cs_index;
	ring_reg->capstone = cs_index;

	/* Find the corners of the ring */
	find_region_corners(q, ring, &stone_reg->seed, capstone->corners);

	/* Set up the perspective transform and find the center */
	perspective_setup(capstone->c, capstone->corners, 7.0, 7.0);
	perspective_map(capstone->c, 3.5, 3.5, &capstone->center);
}

static void test_capstone(struct quirc *q, unsigned int x, unsigned int y,
			  unsigned int *pb)
{
	int ring_right = region_code(q, x - pb[4], y);
	int stone = region_code(q, x - pb[4] - pb[3] - pb[2], y);
	int ring_left = region_code(q, x - pb[4] - pb[3] -
				    pb[2] - pb[1] - pb[0],
				    y);
	struct quirc_region *stone_reg;
	struct quirc_region *ring_reg;
	unsigned int ratio;

	if (ring_left < 0 || ring_right < 0 || stone < 0)
		return;

	/* Left and ring of ring should be connected */
	if (ring_left != ring_right)
		return;

	/* Ring should be disconnected from stone */
	if (ring_left == stone)
		return;

	stone_reg = &q->regions[stone];
	ring_reg = &q->regions[ring_left];

	/* Already detected */
	if (stone_reg->capstone >= 0 || ring_reg->capstone >= 0)
		return;

	/* Ratio should ideally be 37.5 */
	ratio = stone_reg->count * 100 / ring_reg->count;
	if (ratio < 10 || ratio > 70)
		return;

	record_capstone(q, ring_left, stone);
}

static void finder_scan(struct quirc *q, unsigned int y)
{
	quirc_pixel_t *row = q->pixels + y * q->w;
	unsigned int x;
	int last_color = 0;
	unsigned int run_length = 0;
	unsigned int run_count = 0;
	unsigned int pb[5];

	memset(pb, 0, sizeof(pb));
	for (x = 0; x < q->w; x++) {
		int color = row[x] ? 1 : 0;

		if (x && color != last_color) {
			memmove(pb, pb + 1, sizeof(pb[0]) * 4);
			pb[4] = run_length;
			run_length = 0;
			run_count++;

			if (!color && run_count >= 5) {
				const int scale = 16;
				static const unsigned int check[5] = {1, 1, 3, 1, 1};
				unsigned int avg, err;
				unsigned int i;
				int ok = 1;

				avg = (pb[0] + pb[1] + pb[3] + pb[4]) * scale / 4;
				err = avg * 3 / 4;

				for (i = 0; i < 5; i++)
					if (pb[i] * scale < check[i] * avg - err ||
					    pb[i] * scale > check[i] * avg + err)
						ok = 0;

				if (ok)
					test_capstone(q, x, y, pb);
			}
		}

		run_length++;
		last_color = color;
	}
}

static void find_alignment_pattern(struct quirc *q, int index)
{
	struct quirc_grid *qr = &q->grids[index];
	struct quirc_capstone *c0 = &q->capstones[qr->caps[0]];
	struct quirc_capstone *c2 = &q->capstones[qr->caps[2]];
	struct quirc_point a;
	struct quirc_point b;
	struct quirc_point c;
	int size_estimate;
	int step_size = 1;
	int dir = 0;
	quirc_float_t u, v;

	/* Grab our previous estimate of the alignment pattern corner */
	memcpy(&b, &qr->align, sizeof(b));

	/* Guess another two corners of the alignment pattern so that we
	 * can estimate its size.
	 */
	perspective_unmap(c0->c, &b, &u, &v);
	perspective_map(c0->c, u, v + 1.0, &a);
	perspective_unmap(c2->c, &b, &u, &v);
	perspective_map(c2->c, u + 1.0, v, &c);

	size_estimate = abs((a.x - b.x) * -(c.y - b.y) +
			    (a.y - b.y) * (c.x - b.x));

	/* Spiral outwards from the estimate point until we find something
	 * roughly the right size. Don't look too far from the estimate
	 * point.
	 */
	while (step_size * step_size < size_estimate * 100) {
		static const int dx_map[] = {1, 0, -1, 0};
		static const int dy_map[] = {0, -1, 0, 1};
		int i;

		for (i = 0; i < step_size; i++) {
			int code = region_code(q, b.x, b.y);

			if (code >= 0) {
				struct quirc_region *reg = &q->regions[code];

				if (reg->count >= size_estimate / 2 &&
				    reg->count <= size_estimate * 2) {
					qr->align_region = code;
					return;
				}
			}

			b.x += dx_map[dir];
			b.y += dy_map[dir];
		}

		dir = (dir + 1) % 4;
		if (!(dir & 1))
			step_size++;
	}
}

static void find_leftmost_to_line(void *user_data, int y, int left, int right)
{
	struct polygon_score_data *psd =
		(struct polygon_score_data *)user_data;
	int xs[2] = {left, right};
	int i;

	for (i = 0; i < 2; i++) {
		int d = -psd->ref.y * xs[i] + psd->ref.x * y;

		if (d < psd->scores[0]) {
			psd->scores[0] = d;
			psd->corners[0].x = xs[i];
			psd->corners[0].y = y;
		}
	}
}

static quirc_float_t length(struct quirc_point a, struct quirc_point b)
{
	quirc_float_t x = abs(a.x - b.x) + 1;
	quirc_float_t y = abs(a.y - b.y) + 1;
	return sqrt(x * x +  y * y);
}
/* Estimate grid size by determing distance between capstones
 */
static void measure_grid_size(struct quirc *q, int index)
{
	struct quirc_grid *qr = &q->grids[index];

	struct quirc_capstone *a = &(q->capstones[qr->caps[0]]);
	struct quirc_capstone *b = &(q->capstones[qr->caps[1]]);
	struct quirc_capstone *c = &(q->capstones[qr->caps[2]]);

	quirc_float_t ab = length(b->corners[0], a->corners[3]);
	quirc_float_t capstone_ab_size = (length(b->corners[0], b->corners[3]) + length(a->corners[0], a->corners[3]))/2.0;
	quirc_float_t ver_grid = 7.0 * ab / capstone_ab_size;

	quirc_float_t bc = length(b->corners[0], c->corners[1]);
	quirc_float_t capstone_bc_size = (length(b->corners[0], b->corners[1]) + length(c->corners[0], c->corners[1]))/2.0;
	quirc_float_t hor_grid = 7.0 * bc / capstone_bc_size;
	
	quirc_float_t grid_size_estimate = (ver_grid + hor_grid) / 2;

	int ver = (int)((grid_size_estimate - 17.0 + 2.0) / 4.0);
	
	qr->grid_size =  4*ver + 17;
}

/* Read a cell from a grid using the currently set perspective
 * transform. Returns +/- 1 for black/white, 0 for cells which are
 * out of image bounds.
 */
static int read_cell(const struct quirc *q, int index, int x, int y)
{
	const struct quirc_grid *qr = &q->grids[index];
	struct quirc_point p;

	perspective_map(qr->c, x + 0.5, y + 0.5, &p);
	if (p.y < 0 || p.y >= q->h || p.x < 0 || p.x >= q->w)
		return 0;

	return q->pixels[p.y * q->w + p.x] ? 1 : -1;
}

static int fitness_cell(const struct quirc *q, int index, int x, int y)
{
	const struct quirc_grid *qr = &q->grids[index];
	int score = 0;
	int u, v;

	for (v = 0; v < 3; v++)
		for (u = 0; u < 3; u++) {
			static const quirc_float_t offsets[] = {0.3, 0.5, 0.7};
			struct quirc_point p;

			perspective_map(qr->c, x + offsets[u],
					       y + offsets[v], &p);
			if (p.y < 0 || p.y >= q->h || p.x < 0 || p.x >= q->w)
				continue;

			if (q->pixels[p.y * q->w + p.x])
				score++;
			else
				score--;
		}

	return score;
}

static int fitness_ring(const struct quirc *q, int index, int cx, int cy,
			int radius)
{
	int i;
	int score = 0;

	for (i = 0; i < radius * 2; i++) {
		score += fitness_cell(q, index, cx - radius + i, cy - radius);
		score += fitness_cell(q, index, cx - radius, cy + radius - i);
		score += fitness_cell(q, index, cx + radius, cy - radius + i);
		score += fitness_cell(q, index, cx + radius - i, cy + radius);
	}

	return score;
}

static int fitness_apat(const struct quirc *q, int index, int cx, int cy)
{
	return fitness_cell(q, index, cx, cy) -
		fitness_ring(q, index, cx, cy, 1) +
		fitness_ring(q, index, cx, cy, 2);
}

static int fitness_capstone(const struct quirc *q, int index, int x, int y)
{
	x += 3;
	y += 3;

	return fitness_cell(q, index, x, y) +
		fitness_ring(q, index, x, y, 1) -
		fitness_ring(q, index, x, y, 2) +
		fitness_ring(q, index, x, y, 3);
}

/* Compute a fitness score for the currently configured perspective
 * transform, using the features we expect to find by scanning the
 * grid.
 */
static int fitness_all(const struct quirc *q, int index)
{
	const struct quirc_grid *qr = &q->grids[index];
	int version = (qr->grid_size - 17) / 4;
	const struct quirc_version_info *info = &quirc_version_db[version];
	int score = 0;
	int i, j;
	int ap_count;

	/* Check the timing pattern */
	for (i = 0; i < qr->grid_size - 14; i++) {
		int expect = (i & 1) ? 1 : -1;

		score += fitness_cell(q, index, i + 7, 6) * expect;
		score += fitness_cell(q, index, 6, i + 7) * expect;
	}

	/* Check capstones */
	score += fitness_capstone(q, index, 0, 0);
	score += fitness_capstone(q, index, qr->grid_size - 7, 0);
	score += fitness_capstone(q, index, 0, qr->grid_size - 7);

	if (version < 0 || version > QUIRC_MAX_VERSION)
		return score;

	/* Check alignment patterns */
	ap_count = 0;
	while ((ap_count < QUIRC_MAX_ALIGNMENT) && info->apat[ap_count])
		ap_count++;

	for (i = 1; i + 1 < ap_count; i++) {
		score += fitness_apat(q, index, 6, info->apat[i]);
		score += fitness_apat(q, index, info->apat[i], 6);
	}

	for (i = 1; i < ap_count; i++)
		for (j = 1; j < ap_count; j++)
			score += fitness_apat(q, index,
					info->apat[i], info->apat[j]);

	return score;
}

static void jiggle_perspective(struct quirc *q, int index)
{
	struct quirc_grid *qr = &q->grids[index];
	int best = fitness_all(q, index);
	int pass;
	quirc_float_t adjustments[8];
	int i;

	for (i = 0; i < 8; i++)
		adjustments[i] = qr->c[i] * 0.02;

	for (pass = 0; pass < 5; pass++) {
		for (i = 0; i < 16; i++) {
			int j = i >> 1;
			int test;
			quirc_float_t old = qr->c[j];
			quirc_float_t step = adjustments[j];
			quirc_float_t new;

			if (i & 1)
				new = old + step;
			else
				new = old - step;

			qr->c[j] = new;
			test = fitness_all(q, index);

			if (test > best)
				best = test;
			else
				qr->c[j] = old;
		}

		for (i = 0; i < 8; i++)
			adjustments[i] *= 0.5;
	}
}

/* Once the capstones are in place and an alignment point has been
 * chosen, we call this function to set up a grid-reading perspective
 * transform.
 */
static void setup_qr_perspective(struct quirc *q, int index)
{
	struct quirc_grid *qr = &q->grids[index];
	struct quirc_point rect[4];

	/* Set up the perspective map for reading the grid */
	memcpy(&rect[0], &q->capstones[qr->caps[1]].corners[0],
	       sizeof(rect[0]));
	memcpy(&rect[1], &q->capstones[qr->caps[2]].corners[0],
	       sizeof(rect[0]));
	memcpy(&rect[2], &qr->align, sizeof(rect[0]));
	memcpy(&rect[3], &q->capstones[qr->caps[0]].corners[0],
	       sizeof(rect[0]));
	perspective_setup(qr->c, rect, qr->grid_size - 7, qr->grid_size - 7);

	jiggle_perspective(q, index);
}

/* Rotate the capstone with so that corner 0 is the leftmost with respect
 * to the given reference line.
 */
static void rotate_capstone(struct quirc_capstone *cap,
			    const struct quirc_point *h0,
			    const struct quirc_point *hd)
{
	struct quirc_point copy[4];
	int j;
	int best = 0;
	int best_score = INT_MAX;

	for (j = 0; j < 4; j++) {
		struct quirc_point *p = &cap->corners[j];
		int score = (p->x - h0->x) * -hd->y +
			(p->y - h0->y) * hd->x;

		if (!j || score < best_score) {
			best = j;
			best_score = score;
		}
	}

	/* Rotate the capstone */
	for (j = 0; j < 4; j++)
		memcpy(&copy[j], &cap->corners[(j + best) % 4],
		       sizeof(copy[j]));
	memcpy(cap->corners, copy, sizeof(cap->corners));
	perspective_setup(cap->c, cap->corners, 7.0, 7.0);
}

static void record_qr_grid(struct quirc *q, int a, int b, int c)
{
	struct quirc_point h0, hd;
	int i;
	int qr_index;
	struct quirc_grid *qr;

	if (q->num_grids >= QUIRC_MAX_GRIDS)
		return;

	/* Construct the hypotenuse line from A to C. B should be to
	 * the left of this line.
	 */
	memcpy(&h0, &q->capstones[a].center, sizeof(h0));
	hd.x = q->capstones[c].center.x - q->capstones[a].center.x;
	hd.y = q->capstones[c].center.y - q->capstones[a].center.y;

	/* Make sure A-B-C is clockwise */
	if ((q->capstones[b].center.x - h0.x) * -hd.y +
	    (q->capstones[b].center.y - h0.y) * hd.x > 0) {
		int swap = a;

		a = c;
		c = swap;
		hd.x = -hd.x;
		hd.y = -hd.y;
	}

	/* Record the grid and its components */
	qr_index = q->num_grids;
	qr = &q->grids[q->num_grids++];

	memset(qr, 0, sizeof(*qr));
	qr->caps[0] = a;
	qr->caps[1] = b;
	qr->caps[2] = c;
	qr->align_region = -1;

	/* Rotate each capstone so that corner 0 is top-left with respect
	 * to the grid.
	 */
	for (i = 0; i < 3; i++) {
		struct quirc_capstone *cap = &q->capstones[qr->caps[i]];

		rotate_capstone(cap, &h0, &hd);
		cap->qr_grid = qr_index;
	}

	/* Check the timing pattern by measuring grid size. This doesn't require a perspective
	 * transform.
	 */
	measure_grid_size(q, qr_index);
	/* Make an estimate based for the alignment pattern based on extending
	 * lines from capstones A and C.
	 */
	if (!line_intersect(&q->capstones[a].corners[0],
			    &q->capstones[a].corners[1],
			    &q->capstones[c].corners[0],
			    &q->capstones[c].corners[3],
			    &qr->align))
		goto fail;

	/* On V2+ grids, we should use the alignment pattern. */
	if (qr->grid_size > 21) {
		/* Try to find the actual location of the alignment pattern. */
		find_alignment_pattern(q, qr_index);

		/* Find the point of the alignment pattern closest to the
		 * top-left of the QR grid.
		 */
		if (qr->align_region >= 0) {
			struct polygon_score_data psd;
			struct quirc_region *reg =
				&q->regions[qr->align_region];

			/* Start from some point inside the alignment pattern */
			memcpy(&qr->align, &reg->seed, sizeof(qr->align));

			memcpy(&psd.ref, &hd, sizeof(psd.ref));
			psd.corners = &qr->align;
			psd.scores[0] = -hd.y * qr->align.x +
				hd.x * qr->align.y;

			flood_fill_seed(q, reg->seed.x, reg->seed.y,
					qr->align_region, QUIRC_PIXEL_BLACK,
					NULL, NULL);
			flood_fill_seed(q, reg->seed.x, reg->seed.y,
					QUIRC_PIXEL_BLACK, qr->align_region,
					find_leftmost_to_line, &psd);
		}
	}

	setup_qr_perspective(q, qr_index);
	return;

fail:
	/* We've been unable to complete setup for this grid. Undo what we've
	 * recorded and pretend it never happened.
	 */
	for (i = 0; i < 3; i++)
		q->capstones[qr->caps[i]].qr_grid = -1;
	q->num_grids--;
}

struct neighbour {
	int		index;
	quirc_float_t		distance;
};

struct neighbour_list {
	struct neighbour	n[QUIRC_MAX_CAPSTONES];
	int			count;
};

static void test_neighbours(struct quirc *q, int i,
			    const struct neighbour_list *hlist,
			    const struct neighbour_list *vlist)
{
	/* Test each possible grouping */
	for (int j = 0; j < hlist->count; j++) {
		const struct neighbour *hn = &hlist->n[j];
		for (int k = 0; k < vlist->count; k++) {
			const struct neighbour *vn = &vlist->n[k];
			quirc_float_t squareness = fabs(1.0 - hn->distance / vn->distance);
			if (squareness < 0.2)
				record_qr_grid(q, hn->index, i, vn->index);
		}
	}
}

static void test_grouping(struct quirc *q, unsigned int i)
{
	struct quirc_capstone *c1 = &q->capstones[i];
	int j;
	struct neighbour_list hlist;
	struct neighbour_list vlist;

	hlist.count = 0;
	vlist.count = 0;

	/* Look for potential neighbours by examining the relative gradients
	 * from this capstone to others.
	 */
	for (j = 0; j < q->num_capstones; j++) {
		struct quirc_capstone *c2 = &q->capstones[j];
		quirc_float_t u, v;

		if (i == j)
			continue;

		perspective_unmap(c1->c, &c2->center, &u, &v);

		u = fabs(u - 3.5);
		v = fabs(v - 3.5);

		if (u < 0.2 * v) {
			struct neighbour *n = &hlist.n[hlist.count++];

			n->index = j;
			n->distance = v;
		}

		if (v < 0.2 * u) {
			struct neighbour *n = &vlist.n[vlist.count++];

			n->index = j;
			n->distance = u;
		}
	}

	if (!(hlist.count && vlist.count))
		return;

	test_neighbours(q, i, &hlist, &vlist);
}

static void pixels_setup(struct quirc *q, uint8_t threshold)
{
	if (QUIRC_PIXEL_ALIAS_IMAGE) {
		q->pixels = (quirc_pixel_t *)q->image;
	}

	uint8_t* source = q->image;
	quirc_pixel_t* dest = q->pixels;
	int length = q->w * q->h;
	while (length--) {
		uint8_t value = *source++;
		*dest++ = (value < threshold) ? QUIRC_PIXEL_BLACK : QUIRC_PIXEL_WHITE;
	}
}

uint8_t *quirc_begin(struct quirc *q, int *w, int *h)
{
	q->num_regions = QUIRC_PIXEL_REGION;
	q->num_capstones = 0;
	q->num_grids = 0;

	if (w)
		*w = q->w;
	if (h)
		*h = q->h;

	return q->image;
}

void quirc_end(struct quirc *q)
{
	int i;

	uint8_t threshold = otsu(q);
	pixels_setup(q, threshold);

	for (i = 0; i < q->h; i++)
		finder_scan(q, i);

	for (i = 0; i < q->num_capstones; i++)
		test_grouping(q, i);
}

void quirc_extract(const struct quirc *q, int index,
		   struct quirc_code *code)
{
	const struct quirc_grid *qr = &q->grids[index];
	int y;
	int i = 0;

	memset(code, 0, sizeof(*code));

	if (index < 0 || index > q->num_grids)
		return;

	perspective_map(qr->c, 0.0, 0.0, &code->corners[0]);
	perspective_map(qr->c, qr->grid_size, 0.0, &code->corners[1]);
	perspective_map(qr->c, qr->grid_size, qr->grid_size,
			&code->corners[2]);
	perspective_map(qr->c, 0.0, qr->grid_size, &code->corners[3]);

	code->size = qr->grid_size;

	/* Skip out early so as not to overrun the buffer. quirc_decode
	 * will return an error on interpreting the code.
	 */
	if (code->size > QUIRC_MAX_GRID_SIZE)
		return;

	for (y = 0; y < qr->grid_size; y++) {
		int x;
		for (x = 0; x < qr->grid_size; x++) {
			if (read_cell(q, index, x, y) > 0) {
				code->cell_bitmap[i >> 3] |= (1 << (i & 7));
			}
			i++;
		}
	}
}
