﻿#include "mvg/multiview/nview_data_sets.h"
#include "mvg/multiview/solver_resection_kernel.h"
#include "mvg/multiview/solver_resection_p3p.h"
#include "testing.h"
#include <vector>

using namespace mvg::multiview;

TEST(Resection_Kernel, Multiview) {

  const int views_num = 3;
  const int points_num = 10;
  const NViewDataSet d = NRealisticCamerasRing(views_num, points_num,
    NViewDatasetConfigurator(1,1,0,0,5,0)); // Suppose a camera with Unit matrix as K

  const int nResectionCameraIndex = 2;

  // Solve the problem and check that fitted value are good enough
  {
    Mat x = d.projected_points_[nResectionCameraIndex];
    Mat X = d.point_3d_;
    mvg::multiview::resection::PoseResectionKernel kernel(x, X);

    size_t samples_[6]={0,1,2,3,4,5};
	std::vector<size_t> samples(samples_, samples_ + 6);
	std::vector<Mat34> Ps;
    kernel.Fit(samples, &Ps);
    for (size_t i = 0; i < x.cols(); ++i) {
      EXPECT_NEAR(0.0, kernel.Error(i, Ps[0]), 1e-8);
    }

	EXPECT_EQ(1, Ps.size());

    // Check that Projection matrix is near to the GT :
    Mat34 GT_ProjectionMatrix = d.P(nResectionCameraIndex).array()
                                / d.P(nResectionCameraIndex).norm();
    Mat34 COMPUTED_ProjectionMatrix = Ps[0].array() / Ps[0].norm();
    EXPECT_MATRIX_NEAR(GT_ProjectionMatrix, COMPUTED_ProjectionMatrix, 1e-8);
  }
}

TEST(P3P_Kneip_CVPR11, Multiview) {

  const int views_num = 3;
  const int points_num = 3;
  const NViewDataSet d = NRealisticCamerasRing(views_num, points_num,
    NViewDatasetConfigurator(1,1,0,0,5,0)); // Suppose a camera with Unit matrix as K

  const int nResectionCameraIndex = 2;

  // Solve the problem and check that fitted value are good enough
  {
    Mat x = d.projected_points_[nResectionCameraIndex];
    Mat X = d.point_3d_;
    mvg::multiview::P3P_ResectionKernel_K kernel(x, X, d.camera_matrix_[0]);

    size_t samples_[3]={0,1,2};
    std::vector<size_t> samples(samples_, samples_+3);
	std::vector<Mat34> Ps;
    kernel.Fit(samples, &Ps);

    bool bFound = false;
    char index = -1;
    for (size_t i = 0; i < Ps.size(); ++i)  {
      Mat34 GT_ProjectionMatrix = d.P(nResectionCameraIndex).array()
                                / d.P(nResectionCameraIndex).norm();
      Mat34 COMPUTED_ProjectionMatrix = Ps[i].array() / Ps[i].norm();
      if ( NormLInfinity(GT_ProjectionMatrix - COMPUTED_ProjectionMatrix) < 1e-8 )
      {
        bFound = true;
        index = i;
      }
    }
    EXPECT_TRUE(bFound);

    // Check that for the found matrix residual is small
    for (size_t i = 0; i < x.cols(); ++i) {
      EXPECT_NEAR(0.0, kernel.Error(i,Ps[index]), 1e-8);
    }
  }
}

// Create a new synthetic dataset for the EPnP implementation.
// It seems it do not perform well on translation like t = (0,0,x)

// Generates all necessary inputs and expected outputs for EuclideanResection.
void CreateCameraSystem(const Mat3& KK,
                        const Mat3X& x_image,
                        const Vec& X_distances,
                        const Mat3& R_input,
                        const Vec3& T_input,
                        Mat2X *x_camera,
                        Mat3X *X_world,
                        Mat3  *R_expected,
                        Vec3  *T_expected) {
  int num_points = x_image.cols();

  Mat3X x_unit_cam(3, num_points);
  x_unit_cam = KK.inverse() * x_image;

  // Create normalized camera coordinates to be used as an input to the PnP
  // function, instead of using NormalizeColumnVectors(&x_unit_cam).
  *x_camera = x_unit_cam.block(0, 0, 2, num_points);
  for (int i = 0; i < num_points; ++i){
    x_unit_cam.col(i).normalize();
  }

  // Create the 3D points in the camera system.
  Mat X_camera(3, num_points);
  for (int i = 0; i < num_points; ++i) {
    X_camera.col(i) = X_distances(i) * x_unit_cam.col(i);
  }

  // Apply the transformation to the camera 3D points
  Mat translation_matrix(3, num_points);
  translation_matrix.row(0).setConstant(T_input(0));
  translation_matrix.row(1).setConstant(T_input(1));
  translation_matrix.row(2).setConstant(T_input(2));

  *X_world = R_input * X_camera + translation_matrix;

  // Create the expected result for comparison.
  *R_expected = R_input.transpose();
  *T_expected = *R_expected * ( - T_input);
};


TEST(EuclideanResection, Points6AllRandomInput) {
  Mat3 KK;
  KK << 2796, 0,    800,
        0 ,   2796, 600,
        0,    0,    1;

  // Create random image points for a 1600x1200 image.
  int w = 1600;
  int h = 1200;
  int num_points = 6;
  Mat3X x_image(3, num_points);
  x_image.row(0) = w * Vec::Random(num_points).array().abs();
  x_image.row(1) = h * Vec::Random(num_points).array().abs();
  x_image.row(2).setOnes();

  // Normalized camera coordinates to be used as an input to the PnP function.
  Mat2X x_camera;
  Vec X_distances = 100 * Vec::Random(num_points).array().abs();

  // Create the random camera motion R and t that resection should recover.
  Mat3 R_input;
  R_input = Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitZ())
          * Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitY())
          * Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitZ());

  Vec3 T_input;
  T_input = T_input.setRandom().array() * 100;

  // Create the camera system.
  Mat3 R_expected;
  Vec3 T_expected;
  Mat3X X_world;
  CreateCameraSystem(KK, x_image, X_distances, R_input, T_input,
                     &x_camera, &X_world, &R_expected, &T_expected);


  {
    typedef mvg::multiview::euclidean::ResectionKernel_K Kernel;
    Kernel kernel(x_image.block(0, 0, 2, 6), X_world, KK);

    size_t samples_[6]={0,1,2,3,4,5};
	std::vector<size_t> samples(samples_, samples_ + 6);
	std::vector<Mat34> Ps;
    kernel.Fit(samples, &Ps);

	EXPECT_EQ(1, Ps.size());

    bool bFound = false;
    for (size_t i = 0; i < Ps.size(); ++i)  {
      Mat3 R_output;
      Vec3 T_output;
      Mat3 K;
      KRt_From_P(Ps[i], &K, &R_output, &T_output);
      if ( NormLInfinity(T_output-T_expected) < 1e-8 &&
           NormLInfinity(R_output-R_expected) < 1e-8)
          bFound = true;
    }
    EXPECT_TRUE(bFound);
  }

}
