﻿
//
//
// Five point relative pose computation using Groebner basis.
// We follow the algorithm of [1] and apply some optimization hints of [2].
//
// [1] H. Stew茅nius, C. Engels and D. Nist茅r,  "Recent Developments on Direct
//     Relative Orientation",  ISPRS 2006
//
// [2] D. Nist茅r,  "An Efficient Solution to the Five-Point Relative Pose",
//     PAMI 2004


#ifndef MVG_MULTIVIEW_SOLVER_ESSENTIAL_FIVE_POINT_H_
#define MVG_MULTIVIEW_SOLVER_ESSENTIAL_FIVE_POINT_H_

#include <vector>
#include "mvg/math/numeric.h"

using namespace mvg::math;

namespace mvg {
	namespace multiview{
		using namespace std;

		/** Computes the relative pose of two calibrated cameras from 5 correspondences.
		 *
		 * \param x1 Points in the first image.  One per column.
		 * \param x2 Corresponding points in the second image. One per column.
		 * \param E  A list of at most 10 candidate essential matrix solutions.
		 */
		void FivePointsRelativePose(const Mat2X &x1, const Mat2X &x2,
			vector<Mat3> *E);

		// Compute the nullspace of the linear constraints given by the matches.
		Mat FivePointsNullspaceBasis(const Mat2X &x1, const Mat2X &x2);

		// Multiply two polynomials of degree 1.
		Vec o1(const Vec &a, const Vec &b);

		// Multiply a polynomial of degree 2, a, by a polynomial of degree 1, b.
		Vec o2(const Vec &a, const Vec &b);

		// Builds the polynomial constraint matrix M.
		Mat FivePointsPolynomialConstraints(const Mat &E_basis);

		// Gauss--Jordan elimination for the constraint matrix.
		void FivePointsGaussJordan(Mat *Mp);

		// In the following code, polynomials are expressed as vectors containing
		// their coeficients in the basis of monomials:
		//
		//  [xxx xxy xyy yyy xxz xyz yyz xzz yzz zzz xx xy yy xz yz zz x y z 1]
		//
		// Note that there is an error in Stewenius' paper.  In equation (9) they
		// propose to use the basis:
		//
		//  [xxx xxy xxz xyy xyz xzz yyy yyz yzz zzz xx xy xz yy yz zz x y z 1]
		//
		// But this is not the basis used in the rest of the paper, neither in
		// the code they provide.  I (pau) have spend 4 hours debugging and
		// reverse engineering their code to find the problem. :(
		enum {
			coef_xxx,
			coef_xxy,
			coef_xyy,
			coef_yyy,
			coef_xxz,
			coef_xyz,
			coef_yyz,
			coef_xzz,
			coef_yzz,
			coef_zzz,
			coef_xx,
			coef_xy,
			coef_yy,
			coef_xz,
			coef_yz,
			coef_zz,
			coef_x,
			coef_y,
			coef_z,
			coef_1
		};
	}
} // namespace mvg

#endif  // MVG_MULTIVIEW_SOLVER_ESSENTIAL_FIVE_POINT_H_

