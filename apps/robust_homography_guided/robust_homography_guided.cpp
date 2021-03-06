#include "mvg/camera/pinhole_camera.h"
#include "mvg/image/image.h"
#include "mvg/feature/features.h"
#include "mvg/feature/matcher_brute_force.h"
#include "mvg/feature/indexed_match_decorator.h"
#include "mvg/feature/estimator_acransac_kernel_adaptator.h"
#include "mvg/camera/projection.h"
#include "mvg/multiview/triangulation.h"
#include "mvg/multiview/essential_estimation.h"
#include "mvg/multiview/solver_affine.h"

#include "mvg/feature/sift.hpp"
#include "mvg/feature/two_view_matches.h"

#include "mvg/utils/file_system.h"
#include "mvg/utils/svg_drawer.h"
#include "mvg/math/numeric.h"
#include "mvg/multiview/solver_homography_kernel.h"
#include "mvg/feature/guided_matching.h"
#include <string>
#include <iostream>

using namespace mvg::utils;
using namespace mvg::feature;
using namespace mvg::camera;
using namespace mvg::multiview;

int main() {

  Image<RGBColor> image;
  string left_image_name = string(THIS_SOURCE_DIR)
    + "/data/imageData/StanfordMobileVisualSearch/Ace_0.png";
  string right_image_name = string(THIS_SOURCE_DIR)
    + "/data/imageData/StanfordMobileVisualSearch/Ace_1.png";

  Image<unsigned char> left_image, right_image;
  readImage(left_image_name.c_str(), &left_image);
  readImage(right_image_name.c_str(), &right_image);

  // 定义使用的描述子(SIFT : 128 float类型值)
  typedef float descType;
  typedef Descriptor<descType,128> SIFTDescriptor;

  // 定义vector用来存储检测到的特征和对应描述子
  std::vector<ScalePointFeature> left_features, right_features;
  std::vector<SIFTDescriptor > left_descriptors, right_descriptors;
  // 计算SIFT特征，设置计算特征参数
  bool is_zoom = true;
  bool is_root_sift = true;
  SIFTDetector(left_image, left_features, left_descriptors, is_zoom, is_root_sift);
  SIFTDetector(right_image, right_features, right_descriptors, is_zoom, is_root_sift);

  // 将左右图像合并显示进行对比
  {
    Image<unsigned char> concat;
    concatHorizontal(left_image, right_image, concat);
    string out_filename = "01_concat.jpg";
    writeImage(out_filename.c_str(), concat);
  }

  // 画出左右两幅图像的特征
  {
    Image<unsigned char> concat;
    concatHorizontal(left_image, right_image, concat);

	// 画出特征 :
    for (size_t i=0; i < left_features.size(); ++i )  {
      const ScalePointFeature & left_img = left_features[i];
      DrawCircle(left_img.x(), left_img.y(), left_img.scale(), 255, &concat);
    }
    for (size_t i=0; i < right_features.size(); ++i )  {
      const ScalePointFeature & right_img = right_features[i];
      DrawCircle(right_img.x()+left_image.Width(), right_img.y(), right_img.scale(), 255, &concat);
    }
    string out_filename = "02_features.jpg";
    writeImage(out_filename.c_str(), concat);
  }

  std::vector<IndexedMatch> vec_putative_matches;
  // 执行特征匹配，找最近邻匹配，通过距离比进行过滤
  {
	 //定义匹配的度量标准，采用欧式距离的平方
    typedef SquaredEuclideanDistanceVectorized<SIFTDescriptor::bin_type> Metric;
	// 定义暴力匹配
    typedef ArrayMatcherBruteForce<SIFTDescriptor::bin_type, Metric> MatcherT;

	// 设定距离比率进行匹配
    GetPutativesMatches<SIFTDescriptor, MatcherT>(left_descriptors, right_descriptors, Square(0.8), vec_putative_matches);

	// 距离比率过后画出相对于的匹配
    SvgDrawer svg_stream( left_image.Width() + right_image.Width(), max(left_image.Height(), right_image.Height()));
    svg_stream.drawImage(left_image_name, left_image.Width(), left_image.Height());
    svg_stream.drawImage(right_image_name, right_image.Width(), right_image.Height(), left_image.Width());
    for (size_t i = 0; i < vec_putative_matches.size(); ++i) {
		//得到特征，绘制圆用直线进行连接
      const ScalePointFeature & L = left_features[vec_putative_matches[i]._i];
      const ScalePointFeature & R = right_features[vec_putative_matches[i]._j];
      svg_stream.drawLine(L.x(), L.y(), R.x()+left_image.Width(), R.y(), SvgStyle().stroke("green", 2.0));
      svg_stream.drawCircle(L.x(), L.y(), L.scale(), SvgStyle().stroke("yellow", 2.0));
      svg_stream.drawCircle(R.x()+left_image.Width(), R.y(), R.scale(),SvgStyle().stroke("yellow", 2.0));
    }
    std::string out_filename = "03_siftMatches.svg";
    std::ofstream svg_file( out_filename.c_str() );
    svg_file << svg_stream.closeSvgFile().str();
    svg_file.close();
  }

  // 通过两图像之间的单应关系对匹配点进行过滤
  {
	 //A. 准备匹配的对应点
    Mat left_points(2, vec_putative_matches.size());
    Mat right_points(2, vec_putative_matches.size());

    for (size_t k = 0; k < vec_putative_matches.size(); ++k)  {
      const ScalePointFeature &left_feature = left_features[vec_putative_matches[k]._i];
      const ScalePointFeature &right_feature = right_features[vec_putative_matches[k]._j];
      left_points.col(k) = left_feature.coords().cast<double>();
      right_points.col(k) = right_feature.coords().cast<double>();
    }

    //单应矩阵 鲁棒性估计
    std::vector<size_t> vec_inliers;
    typedef ACKernelAdaptor<
      mvg::multiview::homography::FourPointSolver,
	  mvg::multiview::homography::AsymmetricError,
      UnnormalizerI,
      Mat3>
      KernelType;

    KernelType kernel(
      left_points, left_image.Width(), left_image.Height(),
      right_points, right_image.Width(), right_image.Height(),
      false); // configure as point to point error model.

    Mat3 H;
    std::pair<double,double> acransac_out = ACRANSAC(kernel, vec_inliers, 1024, &H,
      std::numeric_limits<double>::infinity(),
      true);
    const double & thresholdH = acransac_out.first;

    // Check the homography support some point to be considered as valid
    if (vec_inliers.size() > KernelType::MINIMUM_SAMPLES *2.5) {

      std::cout << "\nFound a homography under the confidence threshold of: "
        << thresholdH << " pixels\n\twith: " << vec_inliers.size() << " inliers"
        << " from: " << vec_putative_matches.size()
         << " putatives correspondences"
        << std::endl;

      //Show homography validated point and compute residuals
      std::vector<double> vec_residuals(vec_inliers.size(), 0.0);
      SvgDrawer svg_stream( left_image.Width() + right_image.Width(), max(left_image.Height(), right_image.Height()));
      svg_stream.drawImage(left_image_name, left_image.Width(), left_image.Height());
      svg_stream.drawImage(right_image_name, right_image.Width(), right_image.Height(), left_image.Width());
      for ( size_t i = 0; i < vec_inliers.size(); ++i)  {
        const ScalePointFeature & LL = left_features[vec_putative_matches[vec_inliers[i]]._i];
        const ScalePointFeature & RR = right_features[vec_putative_matches[vec_inliers[i]]._j];
        const Vec2f L = LL.coords();
        const Vec2f R = RR.coords();
        svg_stream.drawLine(L.x(), L.y(), R.x()+left_image.Width(), R.y(), SvgStyle().stroke("green", 2.0));
        svg_stream.drawCircle(L.x(), L.y(), LL.scale(), SvgStyle().stroke("yellow", 2.0));
        svg_stream.drawCircle(R.x()+left_image.Width(), R.y(), RR.scale(),SvgStyle().stroke("yellow", 2.0));
        // residual computation
        vec_residuals[i] = std::sqrt(KernelType::ErrorT::Error(H,
                                       LL.coords().cast<double>(),
                                       RR.coords().cast<double>()));
      }
      string out_filename = "04_ACRansacHomography.svg";
      ofstream svg_file( out_filename.c_str() );
      svg_file << svg_stream.closeSvgFile().str();
      svg_file.close();

      // Display some statistics of reprojection errors
      float min_residual, max_residual, mean_residual, median_residual;
      MinMaxMeanMedian<float>(vec_residuals.begin(), vec_residuals.end(),
                            min_residual, max_residual, mean_residual, median_residual);

      std::cout << std::endl
        << "Homography matrix estimation, residuals statistics:" << "\n"
        << "\t-- Residual min:\t" << min_residual << std::endl
        << "\t-- Residual median:\t" << median_residual << std::endl
        << "\t-- Residual max:\t "  << max_residual << std::endl
        << "\t-- Residual mean:\t " << mean_residual << std::endl;

      // Perform GUIDED MATCHING
      // Use the computed model with all possible point couples
      //  and keep the one that have the error under the AC-RANSAC precision
      //  value.

      Mat left_points, right_points;
      PointsToMat(left_features, left_points);
      PointsToMat(right_features, right_points);
      std::vector<IndexedMatch> vec_corresponding_index;
      GuidedMatching<mvg::math::Mat3, mvg::multiview::homography::AsymmetricError>(
        H, left_points, right_points, Square(thresholdH), vec_corresponding_index);

      std::cout << "\nGuided homography matching found "
        << vec_corresponding_index.size() << " correspondences."
        << std::endl;

      // Merge AC Ransac and H-Guided matches
      std::set<IndexedMatch> set_matches(vec_corresponding_index.begin(),
        vec_corresponding_index.end());
      for ( size_t i = 0; i < vec_inliers.size(); ++i)  {
        set_matches.insert(vec_putative_matches[vec_inliers[i]]);
      }
      std::cout << "\nGuided homography + AC Ransac results matching found "
        << set_matches.size() << " correspondences."
        << std::endl;
      // Update the corresponding index:
      vec_corresponding_index.clear();
      vec_corresponding_index.assign(set_matches.begin(), set_matches.end());

      {
        //Show homography validated correspondences and compute residuals
        std::vector<double> vec_residuals(vec_corresponding_index.size(), 0.0);
        SvgDrawer svg_stream( left_image.Width() + right_image.Width(), max(left_image.Height(), right_image.Height()));
        svg_stream.drawImage(left_image_name, left_image.Width(), left_image.Height());
        svg_stream.drawImage(right_image_name, right_image.Width(), right_image.Height(), left_image.Width());
        for ( size_t i = 0; i < vec_corresponding_index.size(); ++i)  {

          const ScalePointFeature & LL = left_features[vec_corresponding_index[i]._i];
          const ScalePointFeature & RR = right_features[vec_corresponding_index[i]._j];
          const Vec2f L = LL.coords();
          const Vec2f R = RR.coords();
          svg_stream.drawLine(L.x(), L.y(), R.x()+left_image.Width(), R.y(), SvgStyle().stroke("green", 2.0));
          svg_stream.drawCircle(L.x(), L.y(), LL.scale(), SvgStyle().stroke("yellow", 2.0));
          svg_stream.drawCircle(R.x()+left_image.Width(), R.y(), RR.scale(),SvgStyle().stroke("yellow", 2.0));
          // residual computation
          vec_residuals[i] = std::sqrt(KernelType::ErrorT::Error(H,
                                         LL.coords().cast<double>(),
                                         RR.coords().cast<double>()));
        }
        string out_filename = "04_ACRansacHomography_guided.svg";
        ofstream svg_file( out_filename.c_str() );
        svg_file << svg_stream.closeSvgFile().str();
        svg_file.close();

        // Display some statistics of reprojection errors
        float min_residual, max_residual, mean_residual, median_residual;
        MinMaxMeanMedian<float>(vec_residuals.begin(), vec_residuals.end(),
                              min_residual, max_residual, mean_residual, median_residual);

        std::cout << std::endl
          << "Homography matrix estimation, residuals statistics:" << "\n"
          << "\t-- Residual min:\t" << min_residual << std::endl
          << "\t-- Residual median:\t" << median_residual << std::endl
          << "\t-- Residual max:\t "  << max_residual << std::endl
          << "\t-- Residual mean:\t " << mean_residual << std::endl;
      }
    }
    else  {
      std::cout << "ACRANSAC was unable to estimate a rigid homography"
        << std::endl;
    }
  }
  return EXIT_SUCCESS;
}
