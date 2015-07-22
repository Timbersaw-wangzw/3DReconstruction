#include "fblib/tracking/lmicklt_region_tracker.h"

#include "fblib/utils/notify.h"
#include "fblib/image/image.h"
#include "fblib/image/convolve.h"
#include "fblib/image/sample.h"
#include "fblib/math/numeric.h"
using namespace fblib::image;
using namespace fblib::utils;

namespace fblib {
	namespace tracking{
		// TODO(keir): Reduce duplication between here and the other region trackers.
		static bool RegionIsInBounds(const Image<float> &image1,
			double x, double y,
			int half_window_size) {
			// Check the minimum coordinates.
			int min_x = floor(x) - half_window_size - 1;
			int min_y = floor(y) - half_window_size - 1;
			if (min_x < 0.0 ||
				min_y < 0.0) {
				return false;
			}

			// Check the maximum coordinates.
			int max_x = ceil(x) + half_window_size + 1;
			int max_y = ceil(y) + half_window_size + 1;
			if (max_x > image1.cols() ||
				max_y > image1.rows()) {
				return false;
			}

			// Ok, we're good.
			return true;
		}

		// Estimate "reasonable" error by computing autocorrelation for a small shift.
		static double EstimateReasonableError(const Image<float> &image,
			double x, double y,
			int half_width) {
			double error = 0.0;
			for (int r = -half_width; r <= half_width; ++r) {
				for (int c = -half_width; c <= half_width; ++c) {
					double s = SampleLinear(image, y + r, x + c);
					double e1 = SampleLinear(image, y + r + 0.5, x + c) - s;
					double e2 = SampleLinear(image, y + r, x + c + 0.5) - s;
					error += e1*e1 + e2*e2;
				}
			}
			return error / 2.0 * 8.0;
		}

		// This is implemented from "Lukas and Kanade 20 years on: Part 1. Page 42,
		// figure 14: the Levenberg-Marquardt-Inverse Compositional Algorithm".
		bool LmickltRegionTracker::Track(const Image<float> &image1,
			const Image<float> &image2,
			double  x1, double  y1,
			double *x2, double *y2) const {
			if (!RegionIsInBounds(image1, x1, y1, half_window_size)) {
				notify(INFO) << "Fell out of image1's window with x1=" << x1 << ", y1=" << y1
					<< ", hw=" << half_window_size << ".";
				return false;
			}

			int width = 2 * half_window_size + 1;

			// TODO(keir): Avoid recomputing gradients for e.g. the pyramid tracker.
			Image<RGBfColor> image_and_gradient1;
			Image<RGBfColor> image_and_gradient2;
			BlurredImageAndDerivativesChannels(image1, sigma, &image_and_gradient1);

			// TODO(keir): Avoid computing the derivative of image2.
			BlurredImageAndDerivativesChannels(image2, sigma, &image_and_gradient2);

			// Step -1: Resample the template (image1) since it is not pixel aligned.
			//
			// Take a sample of the gradient of the pattern area of image1 at the
			// subpixel position x1, x2. This is reused for each iteration, so
			// precomputing it saves time.
			Image<RGBfColor> image_and_gradient1_sampled;
			SamplePattern(image_and_gradient1, x1, y1, half_window_size, 
				&image_and_gradient1_sampled);

			// Step 0: Initialize delta = 0.01.
			//
			// Ignored for my "normal" LM loop.

			double reasonable_error =
				EstimateReasonableError(image1, x1, y1, half_window_size);

			// Step 1: Warp I with W(x, p) to compute I(W(x; p).
			//
			// Use two images for accepting / rejecting updates.
			int current_image = 0, new_image = 1;
			Image<RGBfColor> image2_sampled[2];
			SamplePattern(image_and_gradient2, *x2, *y2, half_window_size, 
				&image2_sampled[current_image]);

			// Step 2: Compute the squared error I - J.
			double error = 0;
			for (int r = 0; r < width; ++r) {
				for (int c = 0; c < width; ++c) {
					double e = image_and_gradient1_sampled(r, c)(0) -
						image2_sampled[current_image](r, c)(0);
					error += e*e;
				}
			}

			// Step 3: Evaluate the gradient of the template.
			//
			// This is done above when sampling the template (step -1).

			// Step 4: Evaluate the jacobian dw/dp at (x; 0).
			//
			// The jacobian between dx,dy and the warp is constant 2x2 identity, so it
			// doesn't have to get computed. The Gauss-Newton Hessian matrix computation
			// below would normally have to include a multiply by the jacobian.

			// Step 5: Compute the steepest descent images of the template.
			//
			// Since the jacobian of the position w.r.t. the sampled template position is
			// the identity, the steepest descent images are the same as the gradient.

			// Step 6: Compute the Gauss-Newton Hessian for the template (image1).
			//
			// This could get rolled into the previous loop, but split it for now for
			// clarity.
			Mat2 H = Mat2::Zero();
			for (int r = 0; r < width; ++r) {
				for (int c = 0; c < width; ++c) {
					Vec2 g(image_and_gradient1_sampled(r, c)(1),
						image_and_gradient1_sampled(r, c)(2));
					H += g * g.transpose();
				}
			}

			double tau = 1e-3;

			double mu = tau * std::max(H(0, 0), H(1, 1));
			double nu = 2.0;

			int i;
			for (i = 0; i < max_iterations; ++i) {
				// Check that the entire image patch is within the bounds of the images.
				if (!RegionIsInBounds(image2, *x2, *y2, half_window_size)) {
					notify(INFO) << "Fell out of image2's window with x2=" << *x2 << ", y2=" << *y2
						<< ", hw=" << half_window_size << ".";
					return false;
				}

				// Step 7: Compute z
				Vec2 z = Vec2::Zero();
				for (int r = 0; r < width; ++r) {
					for (int c = 0; c < width; ++c) {
						double e = image2_sampled[current_image](r, c)(0) -
							image_and_gradient1_sampled(r, c)(0);
						z(0) += image_and_gradient1_sampled(r, c)(1) * e;
						z(1) += image_and_gradient1_sampled(r, c)(2) * e;
					}
				}

				// Step 8: Compute Hlm and (dx,dy)
				Mat2 diag = H.diagonal().asDiagonal();
				diag *= mu;
				Mat2 Hlm = H + diag;
				Vec2 d = Hlm.lu().solve(z);

				// TODO(keir): Use the usual LM termination and update criterion instead of
				// this hacky version from the LK 20 years on paper.
				notify(INFO) << "x=" << *x2 << ", y=" << *y2 << ", dx=" << d[0] << ", dy=" << d[1]
					<< ", mu=" << mu << ", nu=" << nu;

				// Step 9: Update the warp; W(x; p) <-- W(x;p) compose W(x, dp)^-1
				double new_x2 = *x2 - d[0];
				double new_y2 = *y2 - d[1];

				// Step 9.1: Sample the image at the new position.
				SamplePattern(image_and_gradient2, new_x2, new_y2, half_window_size, 
					&image2_sampled[new_image]);

				// Step 9.2: Compute the new error.
				// TODO(keir): Eliminate duplication with above code.
				double new_error = 0;
				for (int r = 0; r < width; ++r) {
					for (int c = 0; c < width; ++c) {
						double e = image_and_gradient1_sampled(r, c)(0) -
							image2_sampled[new_image](r, c)(0);
						new_error += e*e;
					}
				}
				notify(INFO) << "Old error: " << error << ", new error: " << new_error;

				// If the step was accepted, then check for termination.
				if (d.squaredNorm() < min_update_squared_distance) {
					if (new_error > reasonable_error) {
						notify(INFO) << "Update size shrank but reasonable error ("
							<< reasonable_error << ") not achieved; failing.";
						return false;
					}
					notify(INFO) << "Successful track in " << i << " iterations.";
					return true;
				}

				double rho = (error - new_error) / (d.transpose() * (mu * d + z));

				// Step 10: Accept or reject step.
				if (rho <= 0) {
					// This was a bad step, so don't update.
					mu *= nu;
					nu *= 2;
					notify(INFO) << "Error increased, so reject update.";
				}
				else {
					// The step was good, so update.
					*x2 = new_x2;
					*y2 = new_y2;
					std::swap(new_image, current_image);
					error = new_error;

					mu *= std::max(1 / 3., 1 - pow(2 * rho - 1, 3));
					nu = 2;
				}
			}
			// Getting here means we hit max iterations, so tracking failed.
			notify(INFO) << "Too many iterations; max is set to " << max_iterations << ".";
			return false;
		}
	}
}  // namespace fblib
