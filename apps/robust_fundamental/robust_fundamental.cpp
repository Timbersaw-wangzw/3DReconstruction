#include "mvg/camera/pinhole_camera.h"
#include "mvg/image/image.h"
#include "mvg/feature/features.h"
#include "mvg/feature/matcher_brute_force.h"
#include "mvg/feature/indexed_match_decorator.h"
#include "mvg/camera/projection.h"
#include "mvg/multiview/triangulation.h"
#include "mvg/multiview/essential_estimation.h"

#include "mvg/feature/sift.hpp"
#include "mvg/feature/two_view_matches.h"

#include "mvg/utils/file_system.h"
#include "mvg/utils/svg_drawer.h"


#include <string>
#include <iostream>

using namespace mvg::utils;
using namespace mvg::feature;
using namespace mvg::camera;
using namespace mvg::multiview;

int main() {

  std::string input_dir = string(THIS_SOURCE_DIR)
    + "/data/imageData/SceauxCastle/";
  Image<RGBColor> image;
  string left_image_name = input_dir + "100_7101.jpg";
  string right_image_name = input_dir + "100_7102.jpg";

  Image<unsigned char> left_image, right_image;
  readImage(left_image_name.c_str(), &left_image);
  readImage(right_image_name.c_str(), &right_image);

  //  ����ʹ�õ�������(SIFT : 128 float����ֵ)
  typedef float descType;
  typedef Descriptor<descType,128> SIFTDescriptor;

  // ����vector�����洢��⵽�������Ͷ�Ӧ������
  std::vector<ScalePointFeature> left_features, right_features;
  std::vector<SIFTDescriptor > left_descriptors, right_descriptors;
  // ����SIFT���������ü�����������
  bool is_zoom = false;
  bool is_root_sift = true;
  SIFTDetector(left_image, left_features, left_descriptors, is_zoom, is_root_sift);
  SIFTDetector(right_image, right_features, right_descriptors, is_zoom, is_root_sift);

  // ������ͼ��ϲ���ʾ���жԱ�
  {
    Image<unsigned char> concat;
    concatHorizontal(left_image, right_image, concat);
    string out_filename = "01_concat.jpg";
    writeImage(out_filename.c_str(), concat);
  }

  // ������������ͼ�������
  {
    Image<unsigned char> concat;
    concatHorizontal(left_image, right_image, concat);

	// �������� :
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
  // ִ������ƥ�䣬�������ƥ�䣬ͨ������Ƚ��й���
  {
	 //����ƥ��Ķ�����׼������ŷʽ�����ƽ��
    typedef SquaredEuclideanDistanceVectorized<SIFTDescriptor::bin_type> Metric;
	// ���屩��ƥ��
    typedef ArrayMatcherBruteForce<SIFTDescriptor::bin_type, Metric> MatcherT;

	// �趨������ʽ���ƥ��
    GetPutativesMatches<SIFTDescriptor, MatcherT>(left_descriptors, right_descriptors, Square(0.8), vec_putative_matches);

	// ������ʹ��󻭳�����ڵ�ƥ��
    SvgDrawer svg_stream( left_image.Width() + right_image.Width(), max(left_image.Height(), right_image.Height()));
    svg_stream.drawImage(left_image_name, left_image.Width(), left_image.Height());
    svg_stream.drawImage(right_image_name, right_image.Width(), right_image.Height(), left_image.Width());
    for (size_t i = 0; i < vec_putative_matches.size(); ++i) {
		//�õ�����������Բ��ֱ�߽�������
      const ScalePointFeature & L = left_features[vec_putative_matches[i]._i];
      const ScalePointFeature & R = right_features[vec_putative_matches[i]._j];
      svg_stream.drawLine(L.x(), L.y(), R.x()+left_image.Width(), R.y(), SvgStyle().stroke("green", 2.0));
      svg_stream.drawCircle(L.x(), L.y(), L.scale(), SvgStyle().stroke("yellow", 2.0));
      svg_stream.drawCircle(R.x()+left_image.Width(), R.y(), R.scale(),SvgStyle().stroke("yellow", 2.0));
    }
    string out_filename = "03_siftMatches.svg";
    ofstream svg_file( out_filename.c_str() );
    svg_file << svg_stream.closeSvgFile().str();
    svg_file.close();
  }

  // ͨ����ͼ��֮��Ļ��������ϵ����ƥ����й���
  {
	//A. ׼��ƥ��Ķ�Ӧ��
    Mat left_points(2, vec_putative_matches.size());
    Mat right_points(2, vec_putative_matches.size());

    for (size_t k = 0; k < vec_putative_matches.size(); ++k)  {
      const ScalePointFeature & left_feature = left_features[vec_putative_matches[k]._i];
      const ScalePointFeature & right_feature = right_features[vec_putative_matches[k]._j];
      left_points.col(k) = left_feature.coords().cast<double>();
      right_points.col(k) = right_feature.coords().cast<double>();
    }

    //���������³���Թ���
    std::vector<size_t> vec_inliers;
    typedef ACKernelAdaptor<
		mvg::multiview::fundamental::SevenPointSolver,
		mvg::multiview::fundamental::EpipolarDistanceError,
      UnnormalizerT,
      Mat3>
      KernelType;

    KernelType kernel(
      left_points, left_image.Width(), left_image.Height(),
      right_points, right_image.Width(), right_image.Height(),
      true); // configure as point to line error model.

    Mat3 fundamental_matrix;
    std::pair<double,double> acransac_out = ACRANSAC(kernel, vec_inliers, 1024, &fundamental_matrix,
      4.0, // Upper bound of authorized threshold
      true);
    const double & thresholdF = acransac_out.first;

    // Check the fundamental support some point to be considered as valid
    if (vec_inliers.size() > KernelType::MINIMUM_SAMPLES *2.5) {

      std::cout << "\nFound a fundamental under the confidence threshold of: "
        << thresholdF << " pixels\n\twith: " << vec_inliers.size() << " inliers"
        << " from: " << vec_putative_matches.size()
        << " putatives correspondences"
        << std::endl;

      //Show fundamental validated point and compute residuals
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
        vec_residuals[i] = std::sqrt(KernelType::ErrorT::Error(fundamental_matrix,
                                       LL.coords().cast<double>(),
                                       RR.coords().cast<double>()));
      }
      string out_filename = "04_ACRansacFundamental.svg";
      ofstream svg_file( out_filename.c_str() );
      svg_file << svg_stream.closeSvgFile().str();
      svg_file.close();

      // Display some statistics of reprojection errors
      float min_residual, max_residual, mean_residual, median_residual;
      MinMaxMeanMedian<float>(vec_residuals.begin(), vec_residuals.end(),
                            min_residual, max_residual, mean_residual, median_residual);

      std::cout << std::endl
        << "Fundamental matrix estimation, residuals statistics:" << "\n"
        << "\t-- Residual min:\t" << min_residual << std::endl
        << "\t-- Residual median:\t" << median_residual << std::endl
        << "\t-- Residual max:\t "  << max_residual << std::endl
        << "\t-- Residual mean:\t " << mean_residual << std::endl;
    }
    else  {
      std::cout << "ACRANSAC was unable to estimate a rigid fundamental"
        << std::endl;
    }
  }
  return EXIT_SUCCESS;
}
