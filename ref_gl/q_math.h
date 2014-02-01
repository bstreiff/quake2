#pragma once

#include "../game/q_shared.h"

// 3x3 matrix type
typedef vec_t mat3x3_t[3][3];

#define MATRIX_INLINE __inline

// Given column vectors (3x1 matrix) A and B, computes (A)(B^T). e.g:
//
// [a0]
// [a1] * [b0, b1, b2] = C
// [a2]
//
MATRIX_INLINE void MatrixMultiplyColumnVectorWithTransposedColumnVector(const vec3_t a, const vec3_t b, mat3x3_t c)
{
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			c[i][j] = a[i] * b[j];
		}
	}
}

// Multiply a 3x3 matrix by a column vector (3x1).
MATRIX_INLINE void MatrixMultiplyWithColumnVector(const mat3x3_t a, const vec3_t b, vec3_t c)
{
	for (int i = 0; i < 3; ++i)
	{
		vec_t sum = 0;
		for (int k = 0; k < 3; ++k)
		{
			sum += a[i][k] * b[k];
		}
		c[i] = sum;
	}
}

MATRIX_INLINE void MatrixMultiply(const mat3x3_t a, const mat3x3_t b, mat3x3_t c)
{
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			vec_t sum = 0;
			for (int k = 0; k < 3; ++k)
			{
				sum += a[i][k] * b[k][j];
			}
			c[i][j] = sum;
		}
	}
}

// Add two matrices.
MATRIX_INLINE void MatrixAdd(const mat3x3_t a, const mat3x3_t b, mat3x3_t c)
{
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			c[i][j] = a[i][j] + b[i][j];
		}
	}
}

// The determinant of a 3x3 matrix:
// [ a, b, c ]
// [ d, e, f ]
// [ g, h, i ]
//
// has the value (aei + bfg + cdh) - (ceg + bdi + afh)
MATRIX_INLINE vec_t MatrixDeterminant(const mat3x3_t a)
{
	return
		((a[0][0] * a[1][1] * a[2][2]) +
		(a[0][1] * a[1][2] * a[2][0]) +
		(a[0][2] * a[1][0] * a[2][1])) -
		((a[0][2] * a[1][1] * a[2][0]) +
		(a[0][1] * a[1][0] * a[2][2]) +
		(a[0][0] * a[1][2] * a[2][1]));
}

MATRIX_INLINE void MatrixScale(const mat3x3_t a, const vec_t scalar, mat3x3_t result)
{
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			result[i][j] = a[i][j] * scalar;
		}
	}
}

MATRIX_INLINE void MatrixTranspose(mat3x3_t a)
{
	for (int i = 0; i < 3; ++i)
	{
		for (int j = i + 1; j < 3; ++j)
		{
			vec_t tmp = a[i][j];
			a[i][j] = a[j][i];
			a[j][i] = tmp;
		}
	}
}


/*
* svdcomp - SVD decomposition routine.
* Takes an 3x3 matrix a and decomposes it into udv, where u,v are
* left and right orthogonal transformation matrices, and d is a
* diagonal matrix of singular values.
*
* Input to dsvd is as follows:
*   a = 3x3 matrix to be decomposed, gets overwritten with u
*   w = returns the vector of singular values of a
*   v = returns the right orthogonal transformation matrix
*/
int MatrixSingularValueDcomposition(mat3x3_t a, vec3_t w, mat3x3_t v);
