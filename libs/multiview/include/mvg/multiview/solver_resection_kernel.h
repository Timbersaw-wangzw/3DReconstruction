﻿#ifndef MVG_MULTIVIEW_RESECTION_KERNEL_H_
#define MVG_MULTIVIEW_RESECTION_KERNEL_H_

#include <vector>
#include "mvg/camera/projection.h"
#include "mvg/multiview/two_view_kernel.h"

using namespace mvg::math;
using namespace mvg::camera;

namespace mvg {
	namespace multiview {
		namespace resection {

			/**
			 * Six-point resection
			 * P Matrix estimation (Pose estimation)
			 * Compute a projection matrix using linear least squares.
			 * Rely on Linear Resection algorithm.
			 * Work from 6 to N points.
			 */
			struct SixPointResectionSolver {
				enum { MINIMUM_SAMPLES = 6 };
				enum { MAX_MODELS = 1 };
				// Solve the problem of camera pose.
				// First 3d point will be translated in order to have X0 = (0,0,0,1)
				static void Solve(const Mat &point_2d, const Mat &point_3d, std::vector<Mat34> *P, bool bcheck = true);

				// Compute the residual of the projection distance(point_2d, Project(P,point_3d))
				static double Error(const Mat34 & P, const Vec2 & point_2d, const Vec3 & point_3d){
					Vec2 x = Project(P, point_3d);
					return (x - point_2d).norm();
				}
			};

			//-- Generic Solver for the 6pt Resection algorithm using linear least squares.
			template<typename SolverArg,
				typename ErrorArg,
				typename ModelArg = Mat34>
			class ResectionKernel :
				public two_view::Kernel < SolverArg, ErrorArg, ModelArg >
			{
			public:
				// 2D / 3D points
				ResectionKernel(const Mat &point_2d, const Mat &point_3d) :
					two_view::Kernel<SolverArg, ErrorArg, ModelArg>(point_2d, point_3d){}

				void Fit(const std::vector<size_t> &samples, std::vector<ModelArg> *models) const {
					Mat pt2d = extractColumns(this->x1_, samples);
					Mat point_3d = extractColumns(this->x2_, samples);

					assert(2 == pt2d.rows());
					assert(3 == point_3d.rows());
					assert(SolverArg::MINIMUM_SAMPLES <= pt2d.cols());
					assert(pt2d.cols() == point_3d.cols());

					SolverArg::Solve(pt2d, point_3d, models);
				}

				// Error : re-projection error of the sample
				double Error(size_t sample, const ModelArg &model) const {
					return ErrorArg::Error(model, this->x1_.col(sample), this->x2_.col(sample));
				}
			};

			//-- Usable solver for the 6pt Resection estimation
			typedef two_view::Kernel<SixPointResectionSolver,
				SixPointResectionSolver, Mat34>  PoseResectionKernel;

		}  // namespace kernel
	}  // namespace resection
}  // namespace mvg

//--
// Euclidean resection kernel (Have K intrinsic helps)
//--

namespace mvg {
	namespace multiview {
		namespace euclidean {

			/**
			 * Computes the extrinsic parameters, R and t for a calibrated camera from 4 or
			 * more 3D points and their images.
			 *
			 * \param x_camera Image points in normalized camera coordinates,
			 *                 e.g. x_camera = inv(K) * x_image
			 * \param X_world 3D points in the world coordinate system
			 * \param R       Solution for the camera rotation matrix
			 * \param t       Solution for the camera translation vector
			 *
			 * This is the algorithm described in:
			 * "{EP$n$P: An Accurate $O(n)$ Solution to the P$n$P Problem", by V. Lepetit
			 * and F. Moreno-Noguer and P. Fua, IJCV 2009. vol. 81, no. 2
			 * \note: the non-linear optimization is not implemented here.
			 */
			bool EuclideanResectionEPnP(const Mat2X &x_camera,
				const Mat3X &X_world,
				Mat3 *R, Vec3 *t);

			struct EpnpSolver {
				enum { MINIMUM_SAMPLES = /*5*/ 6 };
				enum { MAX_MODELS = 1 };
				// Solve the problem of camera pose.
				static void Solve(const Mat &point_2d, const Mat &point_3d, std::vector<Mat34> *models)
				{
					Mat3 R;
					Vec3 t;
					Mat34 P;
					if (EuclideanResectionEPnP(point_2d, point_3d, &R, &t)) {
						P_From_KRt(Mat3::Identity(), R, t, &P); // K = Id
						models->push_back(P);
					}
				}

				// Compute the residual of the projection distance(point_2d, Project(P,point_3d))
				static double Error(const Mat34 & P, const Vec2 & point_2d, const Vec3 & point_3d) {
					return (point_2d - Project(P, point_3d)).norm();
				}
			};

			class ResectionKernel_K {
			public:
				typedef Mat34 Model;
				enum { MINIMUM_SAMPLES = 6 };

				ResectionKernel_K(const Mat2X &x_camera, const Mat3X &X) : x_camera_(x_camera), X_(X) {
					assert(x_camera.cols() == X.cols());
					x_image_ = x_camera_;
					K_ = Mat3::Identity();
				}

				ResectionKernel_K(const Mat2X &x_image, const Mat3X &X, const Mat3 &K)
					: x_image_(x_image), X_(X), K_(K)
				{
					assert(x_image.cols() == X.cols());
					// Conversion from image coordinates to normalized camera coordinates
					EuclideanToNormalizedCamera(x_image_, K, &x_camera_);
				}

				void Fit(const std::vector<size_t> &samples, std::vector<Model> *models) const {
					Mat2X x = extractColumns(x_camera_, samples);
					Mat3X X = extractColumns(X_, samples);
					Mat34 P;
					Mat3 R;
					Vec3 t;
					if (EuclideanResectionEPnP(x, X, &R, &t))
					{
						P_From_KRt(K_, R, t, &P);
						models->push_back(P);
					}
				}

				double Error(size_t sample, const Model &model) const {
					Mat3X X = X_.col(sample);
					Mat2X error = Project(model, X) - x_image_.col(sample);
					return error.col(0).norm();
				}

				size_t NumSamples() const {
					return static_cast<size_t>(x_camera_.cols());
				}

			private:
				// x_camera_ contains the normalized camera coordinates
				Mat2X  x_camera_, x_image_;
				const Mat3X &X_;
				Mat3 K_;
			};

		}  // namespace kernel
	}  // namespace euclidean_resection
}  // namespace mvg

#endif  // MVG_MULTIVIEW_RESECTION_KERNEL_H_
