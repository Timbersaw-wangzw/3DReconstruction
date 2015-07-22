﻿#ifndef FBLIB_MULTIVIEW_ROTATION_AVERAGING_L1_H_
#define FBLIB_MULTIVIEW_ROTATION_AVERAGING_L1_H_

#include "fblib/multiview/rotation_averaging_common.h"

//------------------
//-- Bibliography --
//------------------
//- [1] "Efficient and Robust Large-Scale Rotation Averaging"
//- Authors: Avishek Chatterjee and Venu Madhav Govindu
//- Date: December 2013.
//- Conference: ICCV.

namespace fblib {
	namespace multiview {
		namespace l1  {

			

			typedef std::vector<fblib::math::Mat3> Matrix3x3Arr;

			/**
			 * @brief Compute an initial estimation of global rotation and refines them under the L1 norm, [1].
			 *
			 * @param[in] RelRs Relative weighted rotation matrices
			 * @param[out] Rs output global rotation matrices
			 * @param[in] nMainViewID Id of the image considered as Identity (unit rotation)
			 * @param[in] threshold (optionnal) threshold
			 * @param[out] vec_inliers rotation labelled as inliers or outliers
			 */
			bool GlobalRotationsRobust(
				const std::vector<RelRotationData>& RelRs,
				Matrix3x3Arr& Rs,
				const size_t nMainViewID,
				float threshold = 0.f,
				std::vector<bool> * vec_inliers = NULL);

			/**
			 * @brief Implementation of Iteratively Reweighted Least Squares (IRLS) [1].
			 *
			 * @param[in] RelRs Relative weighted rotation matrices
			 * @param[out] Rs output global rotation matrices
			 * @param[in] nMainViewID Id of the image considered as Identity (unit rotation)
			 * @param[in] sigma factor
			 */
			bool RefineRotationsAvgL1IRLS(
				const std::vector<RelRotationData>& RelRs,
				Matrix3x3Arr& Rs,
				const size_t nMainViewID,
				double sigma = fblib::math::D2R(5));

			/**
			 * @brief Sort relative rotation as inlier, outlier rotations.
			 *
			 * @param[in] RelRs Relative weighted rotation matrices
			 * @param[out] Rs output global rotation matrices
			 * @param[in] threshold used to label rotations as inlier, or outlier (if 0, threshold is computed with the X84 law)
			 * @param[in] vec_inliers inlier, outlier labels
			 */
			unsigned int FilterRelativeRotations(
				const std::vector<RelRotationData>& RelRs,
				const Matrix3x3Arr& Rs,
				float threshold = 0.f,
				std::vector<bool> * vec_inliers = NULL);


			// Minimization Stuff

			// L1RA [1] for dense A matrix
			bool RobustRegressionL1PD(
				const Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>& A,
				const Eigen::Matrix<double, Eigen::Dynamic, 1>& b,
				Eigen::Matrix<double, Eigen::Dynamic, 1>& x,
				double pdtol = 1e-3, unsigned pdmaxiter = 50);

			// L1RA [1] for sparse A matrix
			bool RobustRegressionL1PD(
				const Eigen::SparseMatrix<double, Eigen::ColMajor>& A,
				const Eigen::Matrix<double, Eigen::Dynamic, 1>& b,
				Eigen::Matrix<double, Eigen::Dynamic, 1>& x,
				double pdtol = 1e-3, unsigned pdmaxiter = 50);

			/// IRLS [1] for dense A matrix
			bool IterativelyReweightedLeastSquares(
				const Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>& A,
				const Eigen::Matrix<double, Eigen::Dynamic, 1>& b,
				Eigen::Matrix<double, Eigen::Dynamic, 1>& x,
				double sigma, double eps = 1e-5);

			/// IRLS [1] for sparse A matrix
			bool IterativelyReweightedLeastSquares(
				const Eigen::SparseMatrix<double, Eigen::ColMajor>& A,
				const Eigen::Matrix<double, Eigen::Dynamic, 1>& b,
				Eigen::Matrix<double, Eigen::Dynamic, 1>& x,
				double sigma, double eps = 1e-5);

		} // namespace l1
	} // namespace rotation_averaging
} // namespace fblib

#endif // FBLIB_MULTIVIEW_ROTATION_AVERAGING_L1_H_
