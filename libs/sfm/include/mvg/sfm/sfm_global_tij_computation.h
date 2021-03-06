﻿#ifndef MVG_GLOBAL_SFM_ENGINE_TIJ_COMPUTATION_H
#define MVG_GLOBAL_SFM_ENGINE_TIJ_COMPUTATION_H

#include "mvg/math/numeric.h"
#include "mvg/tracking/tracks.h"
#include "mvg/sfm/sfm_global_engine.h"
#include "mvg/sfm/sfm_global_engine_triplet_t_estimator.h"

#undef DYNAMIC
#include "mvg/sfm/problem_data_container.h"
#include "mvg/sfm/sfm_bundle_adjustment_helper_tonly.h"

#include "mvg/utils/indexed_sort.h"
#include "mvg/feature/feature.h"

#include "mvg/sfm/mutex_set.h"
#include "mvg/multiview/triangulation_nview.h"
#include "mvg/multiview/essential.h"
using namespace mvg::feature;
using namespace mvg::multiview;
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
namespace mvg{
	namespace sfm{
		// Robust estimation and refinement of a translation and 3D points of an image triplets.
		bool estimate_T_triplet(
			size_t w, size_t h,
			const mvg::tracking::MapTracks & map_tracksCommon,
			const std::map<size_t, std::vector<mvg::feature::ScalePointFeature> > & map_feats,
			const std::vector<Mat3> & vec_global_KR_Triplet,
			const Mat3 & K,
			std::vector<Vec3> & vec_tis,
			double & dPrecision,
			std::vector<size_t> & vec_inliers)
		{

			// Convert data
			Mat x1(2, map_tracksCommon.size());
			Mat x2(2, map_tracksCommon.size());
			Mat x3(2, map_tracksCommon.size());

			Mat* xxx[3] = { &x1, &x2, &x3 };

			size_t cpt = 0;
			for (MapTracks::const_iterator iterTracks = map_tracksCommon.begin();
				iterTracks != map_tracksCommon.end(); ++iterTracks, ++cpt) {
				const SubmapTrack & subTrack = iterTracks->second;
				size_t index = 0;
				for (SubmapTrack::const_iterator iter = subTrack.begin(); iter != subTrack.end(); ++iter, ++index) {
					const size_t imaIndex = iter->first;
					const size_t featIndex = iter->second;
					const mvg::feature::ScalePointFeature & pt = map_feats.find(imaIndex)->second[featIndex];
					xxx[index]->col(cpt)(0) = pt.x();
					xxx[index]->col(cpt)(1) = pt.y();
				}
			}

			typedef TrifocalKernel_ACRansac_N_tisXis<
				tisXisTrifocalSolver,
				tisXisTrifocalSolver,
				TrifocalTensorModel> KernelType;
			KernelType kernel(x1, x2, x3, w, h, vec_global_KR_Triplet, K);

			const size_t ORSA_ITER = 320;

			TrifocalTensorModel T;
			Mat3 Kinv = K.inverse();
			dPrecision = dPrecision * Kinv(0, 0) * Kinv(0, 0);//std::numeric_limits<double>::infinity();
			std::pair<double, double> acStat = ACRANSAC(kernel, vec_inliers, ORSA_ITER, &T, dPrecision, false);
			dPrecision = acStat.first;

			//-- Export data in order to have an idea of the precision of the estimates
			vec_tis.resize(3);
			Mat3 K2, R;
			Vec3 t;
			KRt_From_P(T.P1, &K2, &R, &t);
			vec_tis[0] = t;
			KRt_From_P(T.P2, &K2, &R, &t);
			vec_tis[1] = t;
			KRt_From_P(T.P3, &K2, &R, &t);
			vec_tis[2] = t;

			// fill Xis
			std::vector<double> vec_residuals(vec_inliers.size());
			std::vector<Vec3> vec_Xis(vec_inliers.size());
			for (size_t i = 0; i < vec_inliers.size(); ++i)  {

				Triangulation triangulation;
				triangulation.add(T.P1, x1.col(vec_inliers[i]));
				triangulation.add(T.P2, x2.col(vec_inliers[i]));
				triangulation.add(T.P3, x3.col(vec_inliers[i]));
				vec_residuals[i] = triangulation.error();
				vec_Xis[i] = triangulation.compute();
			}

			double min, max, mean, median;
			MinMaxMeanMedian<double>(vec_residuals.begin(), vec_residuals.end(),
				min, max, mean, median);

			bool bTest(vec_inliers.size() > 30);

			if (!bTest)
			{
				MVG_INFO << "Triplet rejected : AC: " << dPrecision
					<< " median: " << median
					<< " inliers count " << vec_inliers.size()
					<< " total putative " << map_tracksCommon.size() << std::endl;
			}

			bool bRefine = true;
			if (bRefine && bTest)
			{
				// BA on tis, Xis

				const size_t nbCams = 3;
				const size_t nbPoints3D = vec_Xis.size();

				// Count the number of measurement (sum of the reconstructed track length)
				size_t nbmeasurements = nbPoints3D * 3;

				// Setup a BA problem
				BAProblemData<3> ba_problem; // Will refine translation and 3D points

				// Configure the size of the problem
				ba_problem.num_cameras_ = nbCams;
				ba_problem.num_points_ = nbPoints3D;
				ba_problem.num_observations_ = nbmeasurements;

				ba_problem.point_index_.reserve(ba_problem.num_observations_);
				ba_problem.camera_index_.reserve(ba_problem.num_observations_);
				ba_problem.observations_.reserve(2 * ba_problem.num_observations_);

				ba_problem.num_parameters_ = 3 * ba_problem.num_cameras_ + 3 * ba_problem.num_points_;
				ba_problem.parameters_.reserve(ba_problem.num_parameters_);

				// fill camera
				std::vector<double> vec_Rot(vec_Xis.size() * 3, 0.0);
				{
					Mat3 R = K.inverse() * vec_global_KR_Triplet[0];
					double angleAxis[3];
					ceres::RotationMatrixToAngleAxis((const double*)R.data(), angleAxis);
					vec_Rot[0] = angleAxis[0];
					vec_Rot[1] = angleAxis[1];
					vec_Rot[2] = angleAxis[2];

					// translation
					ba_problem.parameters_.push_back(vec_tis[0](0));
					ba_problem.parameters_.push_back(vec_tis[0](1));
					ba_problem.parameters_.push_back(vec_tis[0](2));
				}
	{
		Mat3 R = K.inverse() * vec_global_KR_Triplet[1];
		double angleAxis[3];
		ceres::RotationMatrixToAngleAxis((const double*)R.data(), angleAxis);
		vec_Rot[3] = angleAxis[0];
		vec_Rot[4] = angleAxis[1];
		vec_Rot[5] = angleAxis[2];

		// translation
		ba_problem.parameters_.push_back(vec_tis[1](0));
		ba_problem.parameters_.push_back(vec_tis[1](1));
		ba_problem.parameters_.push_back(vec_tis[1](2));
	}
	{
		Mat3 R = K.inverse() * vec_global_KR_Triplet[2];
		double angleAxis[3];
		ceres::RotationMatrixToAngleAxis((const double*)R.data(), angleAxis);
		vec_Rot[6] = angleAxis[0];
		vec_Rot[7] = angleAxis[1];
		vec_Rot[8] = angleAxis[2];

		// translation
		ba_problem.parameters_.push_back(vec_tis[2](0));
		ba_problem.parameters_.push_back(vec_tis[2](1));
		ba_problem.parameters_.push_back(vec_tis[2](2));
	}

				// fill 3D points
				for (std::vector<Vec3>::const_iterator iter = vec_Xis.begin();
					iter != vec_Xis.end();
					++iter)
				{
					const Vec3 & point_3d = *iter;
					ba_problem.parameters_.push_back(point_3d[0]);
					ba_problem.parameters_.push_back(point_3d[1]);
					ba_problem.parameters_.push_back(point_3d[2]);
				}

				// fill the measurements
				for (size_t i = 0; i < vec_inliers.size(); ++i)
				{
					double ppx = K(0, 2), ppy = K(1, 2);
					Vec2 ptFeat = x1.col(vec_inliers[i]);
					ba_problem.observations_.push_back(ptFeat.x() - ppx);
					ba_problem.observations_.push_back(ptFeat.y() - ppy);

					ba_problem.point_index_.push_back(i);
					ba_problem.camera_index_.push_back(0);

					ptFeat = x2.col(vec_inliers[i]);
					ba_problem.observations_.push_back(ptFeat.x() - ppx);
					ba_problem.observations_.push_back(ptFeat.y() - ppy);

					ba_problem.point_index_.push_back(i);
					ba_problem.camera_index_.push_back(1);

					ptFeat = x3.col(vec_inliers[i]);
					ba_problem.observations_.push_back(ptFeat.x() - ppx);
					ba_problem.observations_.push_back(ptFeat.y() - ppy);

					ba_problem.point_index_.push_back(i);
					ba_problem.camera_index_.push_back(2);
				}

				// Create residuals for each observation in the bundle adjustment problem. The
				// parameters for cameras and points are added automatically.
				ceres::Problem problem;
				for (size_t i = 0; i < ba_problem.num_observations(); ++i) {
					// Each Residual block takes a point and a camera as input and outputs a 2
					// dimensional residual. Internally, the cost function stores the observed
					// image location and compares the reprojection against the observation.

					ceres::CostFunction* cost_function =
						new ceres::AutoDiffCostFunction<PinholeReprojectionError_t, 2, 3, 3>(
						new PinholeReprojectionError_t(
						&ba_problem.observations()[2 * i + 0],
						K(0, 0),
						&vec_Rot[ba_problem.camera_index_[i] * 3]));

					problem.AddResidualBlock(cost_function,
						NULL, // squared loss
						//new ceres::HuberLoss(Square(4.0)),
						ba_problem.mutable_camera_for_observation(i),
						ba_problem.mutable_point_for_observation(i));
				}
				// Configure a BA engine and run it
				//  Make Ceres automatically detect the bundle structure.
				ceres::Solver::Options options;
				options.linear_solver_type = ceres::SPARSE_SCHUR;
				if (ceres::IsSparseLinearAlgebraLibraryTypeAvailable(ceres::SUITE_SPARSE))
					options.sparse_linear_algebra_library_type = ceres::SUITE_SPARSE;
				else
					if (ceres::IsSparseLinearAlgebraLibraryTypeAvailable(ceres::CX_SPARSE))
						options.sparse_linear_algebra_library_type = ceres::CX_SPARSE;
					else
					{
						// No sparse backend for Ceres.
						// Use dense solving
						options.linear_solver_type = ceres::DENSE_SCHUR;
					}

				options.minimizer_progress_to_stdout = false;
				options.logging_type = ceres::SILENT;
#ifdef USE_OPENMP
				options.num_threads = omp_get_num_threads();
#endif // USE_OPENMP

				// Solve BA
				ceres::Solver::Summary summary;
				ceres::Solve(options, &problem, &summary);

				// If convergence and no error, get back refined parameters
				if (summary.IsSolutionUsable())
				{
					size_t i = 0;
					// Get back updated cameras
					Vec3 * tt[3] = { &vec_tis[0], &vec_tis[1], &vec_tis[2] };
					for (i = 0; i < 3; ++i)
					{
						const double * cam = ba_problem.mutable_cameras() + i * 3;

						(*tt[i]) = Vec3(cam[0], cam[1], cam[2]);
					}
				}
			}
			return bTest;
		}

		//-- Perform a trifocal estimation of the graph contain in vec_triplets with an
		// edge coverage algorithm. It's complexity is sub-linear in term of edges count.
		void GlobalReconstructionEngine::ComputePutativeTranslationEdgesCoverage(
			const std::map<size_t, Mat3> & map_globalR,
			const std::vector< Triplet > & vec_triplets,
			std::vector<mvg::sfm::RelativeInfo > & vec_initial_estimates,
			feature::PairWiseMatches & newpairMatches) const
		{
			// The same K matrix is used by all the camera
			const Mat3 camera_matrix_ = vec_intrinsic_groups_[0].camera_matrix;

			//-- Prepare global rotations
			std::map<size_t, Mat3> map_global_KR;
			for (std::map<std::size_t, Mat3>::const_iterator iter = map_globalR.begin();
				iter != map_globalR.end(); ++iter)
			{
				map_global_KR[iter->first] = camera_matrix_ * iter->second;
			}

			//-- Prepare tracks count per triplets:
			std::map<size_t, size_t> map_tracksPerTriplets;
#ifdef USE_OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
			for (int i = 0; i < (int)vec_triplets.size(); ++i)
			{
				const Triplet & triplet = vec_triplets[i];
				const size_t I = triplet.i, J = triplet.j, K = triplet.k;

				PairWiseMatches map_matchesIJK;
				if (map_matches_fundamental_.find(std::make_pair(I, J)) != map_matches_fundamental_.end())
					map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(I, J)));
				else
					if (map_matches_fundamental_.find(std::make_pair(J, I)) != map_matches_fundamental_.end())
						map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(J, I)));

				if (map_matches_fundamental_.find(std::make_pair(I, K)) != map_matches_fundamental_.end())
					map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(I, K)));
				else
					if (map_matches_fundamental_.find(std::make_pair(K, I)) != map_matches_fundamental_.end())
						map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(K, I)));

				if (map_matches_fundamental_.find(std::make_pair(J, K)) != map_matches_fundamental_.end())
					map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(J, K)));
				else
					if (map_matches_fundamental_.find(std::make_pair(K, J)) != map_matches_fundamental_.end())
						map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(K, J)));

				// Compute tracks:
				mvg::tracking::MapTracks map_tracks;
				TracksBuilder tracks_builder;
				{
					tracks_builder.Build(map_matchesIJK);
					tracks_builder.Filter(3);
					tracks_builder.ExportToSTL(map_tracks);
				}
#ifdef USE_OPENMP
#pragma omp critical
#endif
				map_tracksPerTriplets[i] = map_tracks.size();
			}

			typedef std::pair<size_t, size_t> myEdge;

			//-- List all edges
			std::set<myEdge > set_edges;

			for (size_t i = 0; i < vec_triplets.size(); ++i)
			{
				const Triplet & triplet = vec_triplets[i];
				size_t I = triplet.i, J = triplet.j, K = triplet.k;
				// Add three edges
				set_edges.insert(std::make_pair(std::min(I, J), std::max(I, J)));
				set_edges.insert(std::make_pair(std::min(I, K), std::max(I, K)));
				set_edges.insert(std::make_pair(std::min(J, K), std::max(J, K)));
			}

			// Copy them in vector in order to try to compute them in parallel
			std::vector<myEdge > vec_edges(set_edges.begin(), set_edges.end());

			MutexSet<myEdge> m_mutexSet;

			MVG_INFO << std::endl
				<< "Computation of the relative translations over the graph with an edge coverage algorithm" << std::endl;
#ifdef USE_OPENMP
			//#pragma omp parallel for schedule(dynamic)
#pragma omp parallel for schedule(static, 6)
#endif
			for (int k = 0; k < vec_edges.size(); ++k)
			{
				const myEdge & edge = vec_edges[k];
				//-- If current edge already computed continue
				if (m_mutexSet.isDiscarded(edge) || m_mutexSet.size() == vec_edges.size())
				{
					MVG_INFO << "EDGES WAS PREVIOUSLY COMPUTED" << std::endl;
					continue;
				}

				std::vector<size_t> vec_possibleTriplets;
				// Find the triplet that contain the given edge
				for (size_t i = 0; i < vec_triplets.size(); ++i)
				{
					const Triplet & triplet = vec_triplets[i];
					if (triplet.contain(edge))
					{
						vec_possibleTriplets.push_back(i);
					}
				}

				//-- Sort the triplet according the number of matches they have on their edges
				std::vector<size_t> vec_commonTracksPerTriplets;
				for (size_t i = 0; i < vec_possibleTriplets.size(); ++i)
				{
					vec_commonTracksPerTriplets.push_back(map_tracksPerTriplets[vec_possibleTriplets[i]]);
				}
				//-- If current edge already computed continue
				if (m_mutexSet.isDiscarded(edge))
					continue;

				std::vector< mvg::utils::SortIndexPacketDescend < size_t, size_t> > packet_vec(vec_commonTracksPerTriplets.size());
				mvg::utils::SortIndexHelper(packet_vec, &vec_commonTracksPerTriplets[0]);

				std::vector<size_t> vec_possibleTripletsSorted;
				for (size_t i = 0; i < vec_commonTracksPerTriplets.size(); ++i) {
					vec_possibleTripletsSorted.push_back(vec_possibleTriplets[packet_vec[i].index]);
				}
				vec_possibleTriplets = vec_possibleTripletsSorted;

				// Try to solve the triplets
				// Search the possible triplet:
				for (size_t i = 0; i < vec_possibleTriplets.size(); ++i)
				{
					const Triplet & triplet = vec_triplets[vec_possibleTriplets[i]];
					const size_t I = triplet.i, J = triplet.j, K = triplet.k;
					{
						PairWiseMatches map_matchesIJK;
						if (map_matches_fundamental_.find(std::make_pair(I, J)) != map_matches_fundamental_.end())
							map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(I, J)));
						else
							if (map_matches_fundamental_.find(std::make_pair(J, I)) != map_matches_fundamental_.end())
								map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(J, I)));

						if (map_matches_fundamental_.find(std::make_pair(I, K)) != map_matches_fundamental_.end())
							map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(I, K)));
						else
							if (map_matches_fundamental_.find(std::make_pair(K, I)) != map_matches_fundamental_.end())
								map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(K, I)));

						if (map_matches_fundamental_.find(std::make_pair(J, K)) != map_matches_fundamental_.end())
							map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(J, K)));
						else
							if (map_matches_fundamental_.find(std::make_pair(K, J)) != map_matches_fundamental_.end())
								map_matchesIJK.insert(*map_matches_fundamental_.find(std::make_pair(K, J)));

						// Select common point:
						MapTracks map_tracksCommon;
						TracksBuilder tracks_builder;
						{
							tracks_builder.Build(map_matchesIJK);
							tracks_builder.Filter(3);
							tracks_builder.ExportToSTL(map_tracksCommon);
						}

						// Try to estimate this triplet:
						size_t w = vec_intrinsic_groups_[vec_camera_image_names_[I].intrinsic_id].width;
						size_t h = vec_intrinsic_groups_[vec_camera_image_names_[I].intrinsic_id].height;

						std::vector<Vec3> vec_tis(3);

						// Get rotation:
						std::vector<Mat3> vec_global_KR_Triplet;
						vec_global_KR_Triplet.push_back(map_global_KR[I]);
						vec_global_KR_Triplet.push_back(map_global_KR[J]);
						vec_global_KR_Triplet.push_back(map_global_KR[K]);

						double dPrecision = 4.0;

						std::vector<size_t> vec_inliers;

						if (map_tracksCommon.size() > 50 &&
							estimate_T_triplet(
							w, h,
							map_tracksCommon, map_features_, vec_global_KR_Triplet, camera_matrix_,
							vec_tis, dPrecision, vec_inliers))
						{
							MVG_INFO << dPrecision << "\t" << vec_inliers.size() << std::endl;

							//-- Build the three camera:
							const Mat3 RI = map_globalR.find(I)->second;
							const Mat3 RJ = map_globalR.find(J)->second;
							const Mat3 RK = map_globalR.find(K)->second;
							const Vec3 ti = vec_tis[0];
							const Vec3 tj = vec_tis[1];
							const Vec3 tk = vec_tis[2];

							// Build the 3 relative translations estimations.
							// IJ, JK, IK

							//--- ATOMIC
#ifdef USE_OPENMP
#pragma omp critical
#endif
							{
								Mat3 RijGt;
								Vec3 tij;
								RelativeCameraMotion(RI, ti, RJ, tj, &RijGt, &tij);
								vec_initial_estimates.push_back(
									std::make_pair(std::make_pair(I, J), std::make_pair(RijGt, tij)));

								Mat3 RjkGt;
								Vec3 tjk;
								RelativeCameraMotion(RJ, tj, RK, tk, &RjkGt, &tjk);
								vec_initial_estimates.push_back(
									std::make_pair(std::make_pair(J, K), std::make_pair(RjkGt, tjk)));

								Mat3 RikGt;
								Vec3 tik;
								RelativeCameraMotion(RI, ti, RK, tk, &RikGt, &tik);
								vec_initial_estimates.push_back(
									std::make_pair(std::make_pair(I, K), std::make_pair(RikGt, tik)));

								// Add trifocal inliers as valid 3D points
								for (std::vector<size_t>::const_iterator iterInliers = vec_inliers.begin();
									iterInliers != vec_inliers.end(); ++iterInliers)
								{
									MapTracks::const_iterator iterTracks = map_tracksCommon.begin();
									std::advance(iterTracks, *iterInliers);
									const SubmapTrack & subTrack = iterTracks->second;
									SubmapTrack::const_iterator iterI, iterJ, iterK;
									iterI = iterJ = iterK = subTrack.begin();
									std::advance(iterJ, 1);
									std::advance(iterK, 2);

									newpairMatches[std::make_pair(I, J)].push_back(IndexedMatch(iterI->second, iterJ->second));
									newpairMatches[std::make_pair(J, K)].push_back(IndexedMatch(iterJ->second, iterK->second));
									newpairMatches[std::make_pair(I, K)].push_back(IndexedMatch(iterI->second, iterK->second));
								}
							}

							//-- Remove the 3 edges validated by the trifocal tensor
							m_mutexSet.discard(std::make_pair(std::min(I, J), std::max(I, J)));
							m_mutexSet.discard(std::make_pair(std::min(I, K), std::max(I, K)));
							m_mutexSet.discard(std::make_pair(std::min(J, K), std::max(J, K)));
							break;
						}
					}
				}
			}
		}
	}
} // namespace mvg

#endif // MVG_GLOBAL_SFM_ENGINE_TIJ_COMPUTATION_H
