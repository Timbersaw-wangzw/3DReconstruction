﻿#include "multiview_precomp.h"
#include "fblib/math/numeric.h"
#include "fblib/multiview/projection.h"
#include "fblib/multiview/triangulation.h"
#include "fblib/multiview/essential.h"

namespace fblib {
	namespace multiview{
		
		/**
		* \brief	根据给定的基础矩阵和两相机的内参计算本质矩阵
		*
		* \param	fundamental_matrix		 	基础矩阵
		* \param	K1		 	第一个相机的内参
		* \param	K2		 	第二个相机的内参
		* \param [in,out]	E	得到两幅图像之间的本质矩阵
		*/
		void EssentialFromFundamental(const Mat3 &fundamental_matrix,
			const Mat3 &K1,
			const Mat3 &K2,
			Mat3 *E) {
			*E = K2.transpose() * fundamental_matrix * K1;
		}

		/**
		* \brief	根据给定的本质矩阵和两相机的内参计算基础矩阵
		* 			参考：http://ai.stanford.edu/~birch/projective/node20.html
		*
		* \param	E		 	两图像之间的本质矩阵
		* \param	K1		 	第一个相机的内参
		* \param	K2		 	第二个相机的内参
		* \param [in,out]	fundamental_matrix	输出两图像之间的基础矩阵
		*/
		void FundamentalFromEssential(const Mat3 &E,
			const Mat3 &K1,
			const Mat3 &K2,
			Mat3 *fundamental_matrix)  {
			*fundamental_matrix = K2.inverse().transpose() * E * K1.inverse();
		}

		void RelativeCameraMotion(const Mat3 &R1,
			const Vec3 &t1,
			const Mat3 &R2,
			const Vec3 &t2,
			Mat3 *R,
			Vec3 *t) {
			*R = R2 * R1.transpose();
			*t = t2 - (*R) * t1;
		}

		// HZ 9.6 pag 257
		void EssentialFromRt(const Mat3 &R1,
			const Vec3 &t1,
			const Mat3 &R2,
			const Vec3 &t2,
			Mat3 *E) {
			Mat3 R;
			Vec3 t;
			RelativeCameraMotion(R1, t1, R2, t2, &R, &t);
			Mat3 Tx = CrossProductMatrix(t);
			*E = Tx * R;
		} 

		/**
		* \brief	根据本质矩阵恢复相机外参
		*
		* \param	E		  	本质矩阵
		* \param [in,out]	Rs	相机外参R
		* \param [in,out]	ts	相机外参t
		*/
		void MotionFromEssential(const Mat3 &E,
			std::vector<Mat3> *Rs,
			std::vector<Vec3> *ts) {

			Eigen::JacobiSVD<Mat3> USV(E, Eigen::ComputeFullU | Eigen::ComputeFullV);
			Mat3 U = USV.matrixU();
			Vec3 d = USV.singularValues();
			Mat3 Vt = USV.matrixV().transpose();

			// Last column of U is undetermined since d = (a a 0).
			if (U.determinant() < 0) {
				U.col(2) *= -1;
			}
			// Last row of Vt is undetermined since d = (a a 0).
			if (Vt.determinant() < 0) {
				Vt.row(2) *= -1;
			}

			Mat3 W;
			W << 0, -1, 0,
				1, 0, 0,
				0, 0, 1;

			Mat3 U_W_Vt = U * W * Vt;
			Mat3 U_Wt_Vt = U * W.transpose() * Vt;

			Rs->resize(4);
			ts->resize(4);
			(*Rs)[0] = U_W_Vt;  (*ts)[0] = U.col(2);
			(*Rs)[1] = U_W_Vt;  (*ts)[1] = -U.col(2);
			(*Rs)[2] = U_Wt_Vt; (*ts)[2] = U.col(2);
			(*Rs)[3] = U_Wt_Vt; (*ts)[3] = -U.col(2);
		}

		// HZ 9.6 pag 259 (9.6.3 Geometrical interpretation of the 4 solutions)
		int MotionFromEssentialChooseSolution(const std::vector<Mat3> &Rs,
			const std::vector<Vec3> &ts,
			const Mat3 &K1,
			const Vec2 &x1,
			const Mat3 &K2,
			const Vec2 &x2) {
			assert(Rs.size() == 4);
			assert(ts.size() == 4);

			Mat34 P1, P2;
			// Set P1 = K1 [Id|0]
			Mat3 R1 = Mat3::Identity();
			Vec3 t1 = Vec3::Zero();
			P_From_KRt(K1, R1, t1, &P1);

			for (int i = 0; i < 4; ++i) {
				const Mat3 &R2 = Rs[i];
				const Vec3 &t2 = ts[i];
				P_From_KRt(K2, R2, t2, &P2);
				Vec3 X;
				TriangulateDLT(P1, x1, P2, x2, &X);
				// Test if point is front to the two cameras (positive depth)
				if (Depth(R1, t1, X) > 0 && Depth(R2, t2, X) > 0) {
					return i;
				}
			}
			return -1;
		}

		bool MotionFromEssentialAndCorrespondence(const Mat3 &E,
			const Mat3 &K1,
			const Vec2 &x1,
			const Mat3 &K2,
			const Vec2 &x2,
			Mat3 *R,
			Vec3 *t) {
			std::vector<Mat3> Rs;
			std::vector<Vec3> ts;
			MotionFromEssential(E, &Rs, &ts);
			int solution = MotionFromEssentialChooseSolution(Rs, ts, K1, x1, K2, x2);
			if (solution >= 0) {
				*R = Rs[solution];
				*t = ts[solution];
				return true;
			}
			else {
				return false;
			}
		}
	}// namespace multiview
}  // namespace fblib
