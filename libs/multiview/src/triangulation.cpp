﻿#include "multiview_precomp.h"
#include "fblib/camera/projection.h"
#include "fblib/multiview/triangulation.h"

using fblib::camera::HomogeneousToEuclidean;

namespace fblib {
	namespace multiview{
		
		void TriangulateDLT(const Mat34 &P1, const Vec2 &x1,
			const Mat34 &P2, const Vec2 &x2,
			Vec4 *X_homogeneous) {
			Mat4 design;
			for (int i = 0; i < 4; ++i) {
				design(0, i) = x1[0] * P1(2, i) - P1(0, i);
				design(1, i) = x1[1] * P1(2, i) - P1(1, i);
				design(2, i) = x2[0] * P2(2, i) - P2(0, i);
				design(3, i) = x2[1] * P2(2, i) - P2(1, i);
			}
			Nullspace(&design, X_homogeneous);
		}

		void TriangulateDLT(const Mat34 &P1, const Vec2 &x1,
			const Mat34 &P2, const Vec2 &x2,
			Vec3 *X_euclidean) {
			Vec4 X_homogeneous;
			TriangulateDLT(P1, x1, P2, x2, &X_homogeneous);
			HomogeneousToEuclidean(X_homogeneous, X_euclidean);
		}
	}// namespace multiview
}  // namespace fblib
