﻿#include "camera_precomp.h"
#include "mvg/camera/projection.h"

namespace mvg {
	namespace camera{
		
		void P_From_KRt(const Mat3 &K, const Mat3 &R, const Vec3 &t, Mat34 *P) {
			P->block<3, 3>(0, 0) = R;
			P->col(3) = t;
			(*P) = K * (*P);
		}

		void KRt_From_P(const Mat34 &P, Mat3 *Kp, Mat3 *Rp, Vec3 *tp) {
			
			Mat3 K = P.block(0, 0, 3, 3);

			Mat3 Q;
			Q.setIdentity();

			// Set K(2,1) to zero.
			if (K(2, 1) != 0) {
				double c = -K(2, 2);
				double s = K(2, 1);
				double l = sqrt(c * c + s * s);
				c /= l; s /= l;
				Mat3 Qx;
				Qx << 1, 0, 0,
					0, c, -s,
					0, s, c;
				K = K * Qx;
				Q = Qx.transpose() * Q;
			}
			// Set K(2,0) to zero.
			if (K(2, 0) != 0) {
				double c = K(2, 2);
				double s = K(2, 0);
				double l = sqrt(c * c + s * s);
				c /= l; s /= l;
				Mat3 Qy;
				Qy << c, 0, s,
					0, 1, 0,
					-s, 0, c;
				K = K * Qy;
				Q = Qy.transpose() * Q;
			}
			// Set K(1,0) to zero.
			if (K(1, 0) != 0) {
				double c = -K(1, 1);
				double s = K(1, 0);
				double l = sqrt(c * c + s * s);
				c /= l; s /= l;
				Mat3 Qz;
				Qz << c, -s, 0,
					s, c, 0,
					0, 0, 1;
				K = K * Qz;
				Q = Qz.transpose() * Q;
			}

			Mat3 R = Q;

			//Mat3 H = P.block(0, 0, 3, 3);
			// RQ decomposition
			//Eigen::HouseholderQR<Mat3> qr(H);
			//Mat3 K = qr.matrixQR().triangularView<Eigen::Upper>();
			//Mat3 R = qr.householderQ();

			// Ensure that the diagonal is positive and R determinant == 1.
			if (K(2, 2) < 0) {
				K = -K;
				R = -R;
			}
			if (K(1, 1) < 0) {
				Mat3 S;
				S << 1, 0, 0,
					0, -1, 0,
					0, 0, 1;
				K = K * S;
				R = S * R;
			}
			if (K(0, 0) < 0) {
				Mat3 S;
				S << -1, 0, 0,
					0, 1, 0,
					0, 0, 1;
				K = K * S;
				R = S * R;
			}

			// Compute translation.
			Eigen::PartialPivLU<Mat3> lu(K);
			Vec3 t = lu.solve(P.col(3));

			if (R.determinant() < 0) {
				R = -R;
				t = -t;
			}

			// scale K so that K(2,2) = 1
			K = K / K(2, 2);

			*Kp = K;
			*Rp = R;
			*tp = t;
		}


		Vec2 Project(const Mat34 &P, const Vec3 &X) {
			//构建齐次坐标
			Vec4 HX;
			HX << X, 1.0;
			Vec3 hx = P * HX;
			return hx.head<2>() / hx(2);
		}

		void Project(const Mat34 &P, const Mat3X &X, Mat2X *x) {
			x->resize(2, X.cols());
			for (size_t c = 0; c < static_cast<size_t>(X.cols()); ++c) {
				x->col(c) = Project(P, Vec3(X.col(c)));
			}
		}

		void Project(const Mat34 &P, const Mat4X &X, Mat2X *x) {
			x->resize(2, X.cols());
			for (Mat4X::Index c = 0; c < X.cols(); ++c) {
				Vec3 hx = P * X.col(c);
				x->col(c) = hx.head<2>() / hx(2);
			}
		}

		Mat2X Project(const Mat34 &P, const Mat3X &X) {
			Mat2X x(2, X.cols());
			Project(P, X, &x);
			return x;
		}

		Mat2X Project(const Mat34 &P, const Mat4X &X) {
			Mat2X x(2, X.cols());
			Project(P, X, &x);
			return x;
		}

		/**
		* \brief	齐次坐标做笛卡尔坐标
		*
		* \param	H		 	齐次坐标
		* \param [in,out]	X   输出笛卡尔坐标
		*/
		void HomogeneousToEuclidean(const Vec4 &H, Vec3 *X) {
			double w = H(3);
			*X << H(0) / w, H(1) / w, H(2) / w;
		}

		/**
		* \brief	笛卡尔坐标转齐次坐标
		*
		* \param	X		 	笛卡尔坐标
		* \param [in,out]	H   输出齐次坐标
		*/
		void EuclideanToHomogeneous(const Mat &X, Mat *H) {
			// 添加一行，赋值为1
			Mat::Index d = X.rows();
			Mat::Index n = X.cols();
			H->resize(d + 1, n);
			H->block(0, 0, d, n) = X;
			H->row(d).setOnes();
		}

		double Depth(const Mat3 &R, const Vec3 &t, const Vec3 &X) {
			return (R*X)[2] + t[2];
		}

		Vec3 EuclideanToHomogeneous(const Vec2 &x) {
			return Vec3(x(0), x(1), 1.0);
		}

		void HomogeneousToEuclidean(const Mat &H, Mat *X) {
			Mat::Index d = H.rows() - 1;
			Mat::Index n = H.cols();
			X->resize(d, n);
			for (Mat::Index i = 0; i < n; ++i) {
				double h = H(d, i);
				for (int j = 0; j < d; ++j) {
					(*X)(j, i) = H(j, i) / h;
				}
			}
		}

		Mat3X EuclideanToHomogeneous(const Mat2X &x) {
			Mat3X h(3, x.cols());
			h.block(0, 0, 2, x.cols()) = x;
			h.row(2).setOnes();
			return h;
		}

		void EuclideanToHomogeneous(const Mat2X &x, Mat3X *h) {
			h->resize(3, x.cols());
			h->block(0, 0, 2, x.cols()) = x;
			h->row(2).setOnes();
		}

		void HomogeneousToEuclidean(const Mat3X &h, Mat2X *e) {
			e->resize(2, h.cols());
			e->row(0) = h.row(0).array() / h.row(2).array();
			e->row(1) = h.row(1).array() / h.row(2).array();
		}

		void EuclideanToNormalizedCamera(const Mat2X &x, const Mat3 &K, Mat2X *n) {
			Mat3X x_image_h;
			EuclideanToHomogeneous(x, &x_image_h);
			Mat3X x_camera_h = K.inverse() * x_image_h;
			HomogeneousToEuclidean(x_camera_h, n);
		}

		void HomogeneousToNormalizedCamera(const Mat3X &x, const Mat3 &K, Mat2X *n) {
			Mat3X x_camera_h = K.inverse() * x;
			HomogeneousToEuclidean(x_camera_h, n);
		}

		/// Estimates the root mean square error (2D)
		double RootMeanSquareError(const Mat2X &x_image,
			const Mat4X &X_world,
			const Mat34 &P) {
			size_t num_points = x_image.cols();
			Mat2X dx = Project(P, X_world) - x_image;
			return dx.norm() / num_points;
		}

		/// Estimates the root mean square error (2D)
		double RootMeanSquareError(const Mat2X &x_image,
			const Mat3X &X_world,
			const Mat3 &K,
			const Mat3 &R,
			const Vec3 &t) {
			Mat34 P;
			P_From_KRt(K, R, t, &P);
			size_t num_points = x_image.cols();
			Mat2X dx = Project(P, X_world) - x_image;
			return dx.norm() / num_points;
		}
	}// namespace multiview
} // namespace mvg

