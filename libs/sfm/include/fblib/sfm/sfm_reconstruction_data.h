﻿#ifndef FBLIB_SFM_RECONSTRUCTION_DATA_H
#define FBLIB_SFM_RECONSTRUCTION_DATA_H

#include <iostream>
#include <iterator>
#include <string>
#include <map>
#include <set>
#include <iomanip>
#include <vector>
#include <clocale>

#include "fblib/image/image.h"
#include "fblib/tracking/tracks.h"
#include "fblib/camera/pinhole_camera.h"
#include "fblib/camera/brown_pinhole_camera.h"
#include "fblib/camera/projection.h"

#include "fblib/sfm/sfm_ply_helper.h"
#include "fblib/utils/progress.h"
#include "fblib/utils/stl_map.h"
#include "fblib/utils/file_system.h"
#include "fblib/math/numeric.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
namespace fblib{
	namespace sfm{
		using namespace std;

		// A simple container and undistort function for the Brown's distortion model [1]
		// Variables:
		// (x,y): 2D point in the image (pixel)
		// (u,v): the undistorted 2D point (pixel)
		// radial_distortion (k1, k2, k3, ...): vector containing the radial distortion
		// (cx,cy): camera principal point
		// tangential factors are not considered here
		//
		// Equation:
		// u = x + (x - cx) * (k1 * r^2 + k2 * r^4 +...)
		// v = y + (y - cy) * (k1 * r^2 + k2 * r^4 +...)
		//
		// [1] Decentering distortion of lenses.
		//      Brown, Duane C
		//      Photometric Engineering 1966
		struct BrownDistoModel
		{
			Vec2 m_disto_center; // distortion center
			Vec m_radial_distortion; // radial distortion factor
			double m_f; // focal

			inline void computeUndistortedCoordinates(double xu, double yu, double &xd, double& yd) const
			{
				Vec2 point(xu, yu);
				Vec2 principal_point(m_disto_center);
				Vec2 point_centered = point - principal_point;

				double u = point_centered.x() / m_f;
				double v = point_centered.y() / m_f;
				double radius_squared = u * u + v * v;

				double coef_radial = 0.0;
				for (int i = int(m_radial_distortion.size() - 1); i >= 0; --i) {
					coef_radial = (coef_radial + m_radial_distortion[i]) * radius_squared;
				}

				Vec2 undistorted_point = point + point_centered * coef_radial;
				xd = undistorted_point(0);
				yd = undistorted_point(1);
			}
		};

		/// Undistort an image according a given Distortion model
		template <typename Image>
		Image undistortImage(
			const Image& I,
			const BrownDistoModel& d,
			fblib::image::RGBColor fillcolor = fblib::image::BLACK,
			bool bcenteringPPpoint = false)
		{
			int w = I.Width();
			int h = I.Height();
			double cx = w * .5, cy = h * .5;
			Vec2 offset(0, 0);
			if (bcenteringPPpoint)
				offset = Vec2(cx, cy) - d.m_disto_center;

			Image J(w, h);
#ifdef USE_OPENMP
#pragma omp parallel for
#endif
			for (int j = 0; j < h; j++) {
				for (int i = 0; i < w; i++) {
					double xu, yu, xd, yd;
					xu = double(i);
					yu = double(j);
					d.computeUndistortedCoordinates(xu, yu, xd, yd);
					xd -= offset(0);
					yd -= offset(1);
					if (!J.Contains((int)yd, (int)xd))
						J(j, i) = fillcolor;
					else
						J(j, i) = SampleLinear(I, (float)yd, (float)xd);
				}
			}
			return J;
		}

		/// Represent data in order to make 3D reconstruction process easier
		struct ReconstructorHelper
		{
			//--
			// TYPEDEF
			//--

			typedef std::map<size_t, fblib::camera::BrownPinholeCamera> Map_BrownPinholeCamera;

			// Reconstructed tracks (updated during the process)
			std::set<size_t> set_trackId;
			std::map<size_t, Vec3> map_3DPoints; // Associated 3D point

			// Reconstructed camera information
			std::set<size_t> set_imagedId;
			Map_BrownPinholeCamera map_Camera;

			bool exportToPlyFile(const std::string & file_name, const std::vector<Vec3> * pvec_color = NULL) const
			{
				// get back 3D point into a vector (map value to vector transformation)
				std::vector<Vec3> vec_Reconstructed3DPoints;
				vec_Reconstructed3DPoints.reserve(map_3DPoints.size());
				std::transform(map_3DPoints.begin(),
					map_3DPoints.end(),
					std::back_inserter(vec_Reconstructed3DPoints),
					RetrieveValue());
				//-- Add camera position to the Point cloud
				std::vector<Vec3> vec_camera_pose;
				for (Map_BrownPinholeCamera::const_iterator iter = map_Camera.begin();
					iter != map_Camera.end(); ++iter) {
					vec_camera_pose.push_back(iter->second.camera_center_);
				}
				return ExportToPly(vec_Reconstructed3DPoints, vec_camera_pose, file_name, pvec_color);
			}

			bool ExportToOpenMVGFormat(
				const std::string & out_dir,  //Export directory
				const std::vector<std::string> & file_names, // vector of image filenames
				const std::string & image_path,  // The images path
				const std::vector< std::pair<size_t, size_t> > & vec_imageSize, // Size of each image
				const fblib::tracking::MapTracks & map_reconstructed, // Tracks (Visibility)
				const std::vector<Vec3> * pvec_color = NULL, // Tracks color
				bool bExportImage = true, //Export image ?
				const std::string sComment = std::string("Generated by the OpenMVG library")
				) const
			{
				bool is_ok = true;
				if (!fblib::utils::is_folder(out_dir))
				{
					fblib::utils::folder_create(out_dir);
					is_ok = fblib::utils::is_folder(out_dir);
				}

				// Create basis directory structure
				fblib::utils::folder_create(fblib::utils::folder_append_separator(out_dir) + "cameras");
				fblib::utils::folder_create(fblib::utils::folder_append_separator(out_dir) + "cameras_disto");
				fblib::utils::folder_create(fblib::utils::folder_append_separator(out_dir) + "clouds");
				fblib::utils::folder_create(fblib::utils::folder_append_separator(out_dir) + "images");

				if (is_ok &&
					fblib::utils::is_folder(fblib::utils::folder_append_separator(out_dir) + "cameras") &&
					fblib::utils::is_folder(fblib::utils::folder_append_separator(out_dir) + "cameras_disto") &&
					fblib::utils::is_folder(fblib::utils::folder_append_separator(out_dir) + "clouds") &&
					fblib::utils::is_folder(fblib::utils::folder_append_separator(out_dir) + "images")
					)
				{
					is_ok = true;
				}
				else  {
					std::cerr << "Cannot access to one of the desired output directory" << std::endl;
				}

				if (is_ok)
				{
					//Export Camera as binary files
					std::map<size_t, size_t> map_cameratoIndex;
					size_t count = 0;
					for (Map_BrownPinholeCamera::const_iterator iter =
						map_Camera.begin();
						iter != map_Camera.end();
					++iter)
					{
						map_cameratoIndex[iter->first] = count;
						const Mat34 & PMat = iter->second.projection_matrix_;
						std::ofstream file(
							fblib::utils::create_filespec(fblib::utils::folder_append_separator(out_dir) + "cameras",
							fblib::utils::basename_part(file_names[iter->first])
							, "bin").c_str(), std::ios::out | std::ios::binary);
						file.write((const char*)PMat.data(), (std::streamsize)(3 * 4)*sizeof(double));

						is_ok &= (!file.fail());
						file.close();
						++count;
					}

					//-- Export the camera with disto
					for (Map_BrownPinholeCamera::const_iterator iter =
						map_Camera.begin();
						iter != map_Camera.end();
					++iter)
					{
						const fblib::camera::BrownPinholeCamera & cam = iter->second;
						std::ofstream file(
							fblib::utils::create_filespec(fblib::utils::folder_append_separator(out_dir) + "cameras_disto",
							fblib::utils::basename_part(file_names[iter->first])
							, "txt").c_str(), std::ios::out);
						// Save intrinsic data:
						file << cam._f << " "
							<< cam._ppx << " "
							<< cam._ppy << " "
							<< cam._k1 << " "
							<< cam._k2 << " "
							<< cam._k3 << "\n";
						// Save extrinsic data
						const Mat3 & R = cam.rotation_matrix_;
						file << R(0, 0) << " " << R(0, 1) << " " << R(0, 2) << "\n"
							<< R(1, 0) << " " << R(1, 1) << " " << R(1, 2) << "\n"
							<< R(2, 0) << " " << R(2, 1) << " " << R(2, 2) << "\n";
						file << cam.translation_vector_(0) << " " << cam.translation_vector_(1) << " " << cam.translation_vector_(2) << "\n";
						is_ok &= (!file.fail());
						file.close();
					}

					//Export 3D point and tracks

					size_t nc = map_Camera.size();
					size_t nt = set_trackId.size();

					// Clipping planes (near and far Z depth per view)
					std::vector<double> znear(nc, (numeric_limits<double>::max)()), zfar(nc, 0);
					// Cloud
					std::ofstream f_cloud(
						fblib::utils::create_filespec(fblib::utils::folder_append_separator(out_dir) + "clouds",
						"calib", "ply").c_str());
					std::ofstream f_visibility(
						fblib::utils::create_filespec(fblib::utils::folder_append_separator(out_dir) + "clouds",
						"visibility", "txt").c_str());

					if (!f_cloud.is_open()) {
						std::cerr << "cannot save cloud" << std::endl;
						return false;
					}
					if (!f_visibility.is_open()) {
						std::cerr << "cannot save cloud desc" << std::endl;
						return false;
					}
					f_cloud << "ply\nformat ascii 1.0\n"
						<< "comment " << sComment << "\n"
						<< "element vertex " << nt << "\n"
						<< "property float x\nproperty float y\nproperty float z" << "\n"
						<< "property uchar red\nproperty uchar green\nproperty uchar blue" << "\n"
						<< "property float confidence\nproperty list uchar int visibility" << "\n"
						<< "element face 0\nproperty list uchar int vertex_index" << "\n"
						<< "end_header" << "\n";
					size_t pointCount = 0;
					for (std::set<size_t>::const_iterator iter = set_trackId.begin();
						iter != set_trackId.end();
						++iter, ++pointCount)
					{
						const size_t trackId = *iter;

						// Look through the track and add point position
						const tracking::SubmapTrack & track = (map_reconstructed.find(trackId))->second;

						Vec3 pos = map_3DPoints.find(trackId)->second;

						if (pvec_color)
						{
							const Vec3 & color = (*pvec_color)[pointCount];
							f_cloud << pos.transpose() << " " << color.transpose() << " " << 3.14;
						}
						else
							f_cloud << pos.transpose() << " 255 255 255 " << 3.14;

						std::ostringstream s_visibility;

						std::set< size_t > set_image_index;
						for (tracking::SubmapTrack::const_iterator iterTrack = track.begin();
							iterTrack != track.end();
							++iterTrack)
						{
							const size_t imageId = iterTrack->first;

							if (map_cameratoIndex.find(imageId) != map_cameratoIndex.end())
							{
								set_image_index.insert(map_cameratoIndex[imageId]);
								const fblib::camera::BrownPinholeCamera & cam = (map_Camera.find(imageId))->second;
								double z = fblib::camera::Depth(cam.rotation_matrix_, cam.translation_vector_, pos);
								znear[map_cameratoIndex[imageId]] = std::min(znear[map_cameratoIndex[imageId]], z);
								zfar[map_cameratoIndex[imageId]] = std::max(zfar[map_cameratoIndex[imageId]], z);
							}

							s_visibility << iterTrack->first << " " << iterTrack->second << " ";
						}

						//export images indexes
						f_cloud << " " << set_image_index.size() << " ";
						copy(set_image_index.begin(), set_image_index.end(), std::ostream_iterator<size_t>(f_cloud, " "));
						f_cloud << std::endl;

						f_visibility << pos.transpose() << " " << set_image_index.size() << " ";
						f_visibility << s_visibility.str() << "\n";
					}
					f_cloud.close();
					f_visibility.close();

					// Views
					f_cloud.open(fblib::utils::create_filespec(fblib::utils::folder_append_separator(out_dir),
						"views", "txt").c_str());
					if (!f_cloud.is_open()) {
						std::cerr << "Cannot write views" << endl;
						return false;
					}
					f_cloud << "images\ncameras\n" << nc << "\n";

					count = 0;
					for (Map_BrownPinholeCamera::const_iterator iter = map_Camera.begin();
						iter != map_Camera.end();
						++iter)
					{
						const size_t camIndex = iter->first;
						f_cloud << file_names[camIndex]
							<< ' ' << vec_imageSize[camIndex].first
							<< ' ' << vec_imageSize[camIndex].second
							<< ' ' << fblib::utils::basename_part(file_names[camIndex]) << ".bin"
							<< ' ' << znear[count] / 2
							<< ' ' << zfar[count] * 2
							<< "\n";
						++count;
					}
					f_cloud.close();

					// EXPORT un-distorted IMAGES
					if (bExportImage)
					{
						FBLIB_INFO << " -- Export the undistorted image set, it can take some time ..." << std::endl;
						fblib::utils::ControlProgressDisplay my_progress_bar(static_cast<unsigned long>(map_Camera.size()));
						for (Map_BrownPinholeCamera::const_iterator iter = map_Camera.begin();
							iter != map_Camera.end();
							++iter, ++my_progress_bar)
						{
							// Get distortion information of the image
							const fblib::camera::BrownPinholeCamera & cam = iter->second;
							BrownDistoModel distoModel;
							distoModel.m_disto_center = Vec2(cam._ppx, cam._ppy);
							distoModel.m_radial_distortion = Vec3(cam._k1, cam._k2, cam._k3);
							distoModel.m_f = cam._f;

							// Build the output filename from the input one
							size_t imageIndex = iter->first;
							std::string sImageName = file_names[imageIndex];
							std::string sOutImagePath =
								fblib::utils::create_filespec(fblib::utils::folder_append_separator(out_dir) + "images",
								fblib::utils::basename_part(sImageName),
								fblib::utils::extension_part(sImageName));

							if (distoModel.m_radial_distortion.norm() == 0)
							{
								// Distortion is null, perform a direct copy of the image
								fblib::utils::file_copy(fblib::utils::create_filespec(image_path, sImageName), sOutImagePath);
							}
							else
							{
								// Image with no null distortion
								// - Open the image, undistort it and export it
								fblib::image::Image<fblib::image::RGBColor > image;
								if (fblib::image::ReadImage(fblib::utils::create_filespec(image_path, sImageName).c_str(), &image))
								{
									fblib::image::Image<fblib::image::RGBColor> imageU = undistortImage(image, distoModel);
									fblib::image::WriteImage(sOutImagePath.c_str(), imageU);
								}
							}
						}
					}
				}
				return is_ok;
			}

			/// Export to PMVS format
			/// 'visualize' directory (8 digit coded image name jpg or ppm)
			/// 'txt' camera P matrix
			/// pmvs_options.txt
			/// ignore: vis.dat image links
			bool exportToPMVSFormat(
				const std::string & out_dir,  //Output PMVS files directory
				const std::vector<std::string> & file_names, // vector of filenames
				const std::string & image_path  // The images path
				) const
			{
				bool is_ok = true;
				if (!fblib::utils::is_folder(out_dir))
				{
					fblib::utils::folder_create(out_dir);
					is_ok = fblib::utils::is_folder(out_dir);
				}

				// Create basis directory structure
				fblib::utils::folder_create(fblib::utils::folder_append_separator(out_dir) + "models");
				fblib::utils::folder_create(fblib::utils::folder_append_separator(out_dir) + "txt");
				fblib::utils::folder_create(fblib::utils::folder_append_separator(out_dir) + "visualize");

				if (is_ok &&
					fblib::utils::is_folder(fblib::utils::folder_append_separator(out_dir) + "models") &&
					fblib::utils::is_folder(fblib::utils::folder_append_separator(out_dir) + "txt") &&
					fblib::utils::is_folder(fblib::utils::folder_append_separator(out_dir) + "visualize")
					)
				{
					is_ok = true;
				}
				else  {
					std::cerr << "Cannot access to one of the desired output directory" << std::endl;
				}

				if (is_ok)
				{
					// Export data :
					//Camera

					size_t count = 0;
					for (Map_BrownPinholeCamera::const_iterator iter = map_Camera.begin();
						iter != map_Camera.end(); ++iter, ++count)
					{
						const Mat34 & PMat = iter->second.projection_matrix_;
						std::ostringstream os;
						os << std::setw(8) << std::setfill('0') << count;
						std::ofstream file(
							fblib::utils::create_filespec(fblib::utils::folder_append_separator(out_dir) + "txt",
							os.str(), "txt").c_str());
						file << "CONTOUR\n"
							<< PMat.row(0) << "\n" << PMat.row(1) << "\n" << PMat.row(2) << std::endl;
						file.close();
					}

					// Image
					count = 0;
					fblib::image::Image<fblib::image::RGBColor> image;
					for (Map_BrownPinholeCamera::const_iterator iter = map_Camera.begin();
						iter != map_Camera.end();  ++iter, ++count)
					{
						size_t imageIndex = iter->first;
						const std::string & sImageName = file_names[imageIndex];
						std::ostringstream os;
						os << std::setw(8) << std::setfill('0') << count;
						fblib::image::ReadImage(fblib::utils::create_filespec(image_path, sImageName).c_str(), &image);
						std::string sCompleteImageName = fblib::utils::create_filespec(
							fblib::utils::folder_append_separator(out_dir) + "visualize", os.str(), "jpg");
						fblib::image::WriteImage(sCompleteImageName.c_str(), image);
					}

					//pmvs_options.txt
					std::ostringstream os;
					os << "level 1" << "\n"
						<< "csize 2" << "\n"
						<< "threshold 0.7" << "\n"
						<< "wsize 7" << "\n"
						<< "minImageNum 3" << "\n"
						<< "CPU 8" << "\n"
						<< "setEdge 0" << "\n"
						<< "useBound 0" << "\n"
						<< "useVisData 0" << "\n"
						<< "sequence -1" << "\n"
						<< "timages -1 0 " << map_Camera.size() << "\n"
						<< "oimages 0" << "\n"; // ?

					std::ofstream file(fblib::utils::create_filespec(out_dir, "pmvs_options", "txt").c_str());
					file << os.str();
					file.close();
				}
				return is_ok;
			}
		};
	}// namespace sfm
} // namespace fblib

#endif // FBLIB_SFM_RECONSTRUCTION_DATA_H

